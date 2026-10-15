/******************************************************************************
 *
 *  Copyright (C) 2014 Google, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/
#include <string.h>
#include "common/bt_defs.h"
#include "common/bt_trace.h"
#include "stack/bt_types.h"
#include "hci/hci_hal.h"
#include "hci/hci_internals.h"
#include "hci/hci_layer.h"
#include "hci/hci_trans_int.h"
#include "osi/thread.h"
#include "osi/pkt_queue.h"
#include "esp_bt_main.h"
#include "esp_bluedroid_hci.h"
#include "stack/hcimsgs.h"

#include "l2c_int.h"
#include "stack/hcimsgs.h"

#define HCI_BLE_EVENT 0x3e
#define PACKET_TYPE_TO_INBOUND_INDEX(type) ((type) - 2)
#define PACKET_TYPE_TO_INDEX(type) ((type) - 1)
#define HCI_UPSTREAM_DATA_QUEUE_IDX   (1)
#define HCI_HAL_BLE_ADV_RPT_QUEUE_LEN_MAX      (200)

static const uint8_t preamble_sizes[] = {
    HCI_COMMAND_PREAMBLE_SIZE,
    HCI_ACL_PREAMBLE_SIZE,
    HCI_SCO_PREAMBLE_SIZE,
    HCI_EVENT_PREAMBLE_SIZE
};

static const uint16_t outbound_event_types[] = {
    MSG_HC_TO_STACK_HCI_ERR,
    MSG_HC_TO_STACK_HCI_ACL,
    MSG_HC_TO_STACK_HCI_SCO,
    MSG_HC_TO_STACK_HCI_EVT
};

typedef struct {
    fixed_queue_t *rx_q;
    hci_hal_callbacks_t *callbacks;
    osi_thread_t *hci_h4_thread;
    struct osi_event *upstream_data_ready;
} hci_hal_env_t;


static hci_hal_env_t hci_hal_env;
static const hci_hal_t interface;
static const esp_bluedroid_hci_driver_callbacks_t hci_host_cb;

static void host_send_pkt_available_cb(void);
static int host_recv_pkt_cb(uint8_t *data, uint16_t len);
static void hci_hal_h4_hdl_rx_packet(BT_HDR *packet);
static void hci_upstream_data_handler(void *arg);
static bool hci_upstream_data_post(uint32_t timeout);

static bool hci_hal_env_init(const hci_hal_callbacks_t *upper_callbacks, osi_thread_t *task_thread)
{
    assert(upper_callbacks != NULL);
    assert(task_thread != NULL);

    hci_hal_env.hci_h4_thread = task_thread;
    hci_hal_env.callbacks = (hci_hal_callbacks_t *)upper_callbacks;
    hci_hal_env.rx_q = fixed_queue_new(QUEUE_SIZE_MAX);
    assert(hci_hal_env.rx_q != NULL);

    struct osi_event *event = osi_event_create(hci_upstream_data_handler, NULL);
    assert(event != NULL);
    hci_hal_env.upstream_data_ready = event;
    osi_event_bind(hci_hal_env.upstream_data_ready, hci_hal_env.hci_h4_thread, HCI_UPSTREAM_DATA_QUEUE_IDX);

    return true;
}

static void hci_hal_env_deinit(void)
{
    fixed_queue_t *rx_q = hci_hal_env.rx_q;
    struct osi_event *upstream_data_ready = hci_hal_env.upstream_data_ready;

    hci_hal_env.rx_q = NULL;
    hci_hal_env.upstream_data_ready = NULL;
    fixed_queue_free(rx_q, osi_free_func);
    osi_event_delete(upstream_data_ready);
    hci_hal_env.hci_h4_thread = NULL;

    memset(&hci_hal_env, 0, sizeof(hci_hal_env_t));
}

static bool hal_open(const hci_hal_callbacks_t *upper_callbacks, void *task_thread)
{
    hci_hal_env_init(upper_callbacks, (osi_thread_t *)task_thread);

    //register vhci host cb
    if (hci_host_register_callback(&hci_host_cb) != ESP_OK) {
        return false;
    }

    return true;
}

static void hal_close(void)
{
    hci_hal_env_deinit();
}

/**
 * Function: transmit_data -TX data to low-layer
 * It is ported from Bluedroid source code, so it is not
 * needed to use write() to send data.
 * TODO: Just use firmware API to send data.
 */
static uint16_t transmit_data(serial_data_type_t type,
                              uint8_t *data, uint16_t length)
{
    uint8_t previous_byte;

    assert(data != NULL);
    assert(length > 0);

    if (type < DATA_TYPE_COMMAND || type > DATA_TYPE_SCO) {
        HCI_TRACE_ERROR("%s invalid data type: %d", __func__, type);
        return 0;
    }

    // Write the signal byte right before the data
    --data;
    previous_byte = *data;
    *(data) = type;
    ++length;

    BTTRC_DUMP_BUFFER("Transmit Pkt", data, length);

    // TX Data to target
    hci_host_send_packet(data, length);

    // Be nice and restore the old value of that byte
    *(data) = previous_byte;

    return length - 1;
}

// Internal functions
static void hci_upstream_data_handler(void *arg)
{
    fixed_queue_t *rx_q = hci_hal_env.rx_q;
    size_t pkts_to_process;

    do {
        pkts_to_process = fixed_queue_length(rx_q);
        for (size_t i = 0; i < pkts_to_process; i++) {
            BT_HDR *packet = fixed_queue_dequeue(rx_q, 0);
            if (packet != NULL) {
                hci_hal_h4_hdl_rx_packet(packet);
            }
        }
    } while (0);
    if (!fixed_queue_is_empty(rx_q)
    ) {
        hci_upstream_data_post(OSI_THREAD_MAX_TIMEOUT);
    }
}

static bool hci_upstream_data_post(uint32_t timeout)
{
    return osi_thread_post_event(hci_hal_env.upstream_data_ready, timeout);
}

static void hci_packet_complete(BT_HDR *packet){
    uint8_t type;
    uint16_t handle;
    uint16_t num_packets = 1;
    uint8_t *stream = packet->data + packet->offset;

    STREAM_TO_UINT8(type, stream);
    if (type == DATA_TYPE_ACL/* || type == DATA_TYPE_SCO*/) {
        STREAM_TO_UINT16(handle, stream);
        handle = handle & HCI_DATA_HANDLE_MASK;
        btsnd_hcic_host_num_xmitted_pkts(1, &handle, &num_packets);
    }
}

bool host_recv_adv_packet(uint8_t *packet)
{
    assert(packet);
    if(packet[0] == DATA_TYPE_EVENT && packet[1] == HCI_BLE_EVENT) {
        if(packet[3] ==  HCI_BLE_ADV_PKT_RPT_EVT || packet[3] == HCI_BLE_DIRECT_ADV_EVT
        ) {
            return true;
        }
    }
    return false;
}

static void hci_hal_h4_hdl_rx_packet(BT_HDR *packet)
{
    uint8_t type, hdr_size;
    uint16_t length;
    uint8_t *stream = NULL;

    if (!packet) {
        return;
    }
    stream = packet->data + packet->offset;

    hci_packet_complete(packet);

    STREAM_TO_UINT8(type, stream);
    packet->offset++;
    packet->len--;
    if (type == HCI_BLE_EVENT) {
#if (!CONFIG_BT_STACK_NO_LOG)
        if (packet->len < 1) {
            osi_free(packet);
            return;
        }
        uint8_t len = 0;
        STREAM_TO_UINT8(len, stream);
#endif
        HCI_TRACE_ERROR("Workaround stream corrupted during LE SCAN: pkt_len=%d ble_event_len=%d\n",
                  packet->len, len);
        osi_free(packet);
        return;
    }
    if (type < DATA_TYPE_ACL || type > DATA_TYPE_EVENT) {
        HCI_TRACE_ERROR("%s Unknown HCI message type. Dropping this byte 0x%x,"
                  " min %x, max %x\n", __func__, type,
                  DATA_TYPE_ACL, DATA_TYPE_EVENT);
        osi_free(packet);
        return;
    }
    hdr_size = preamble_sizes[type - 1];
    if (packet->len < hdr_size) {
        HCI_TRACE_ERROR("Wrong packet length type=%d pkt_len=%d hdr_len=%d",
                  type, packet->len, hdr_size);
        osi_free(packet);
        return;
    }
    if (type == DATA_TYPE_ACL) {
        stream += hdr_size - 2;
        STREAM_TO_UINT16(length, stream);
    } else {
        stream += hdr_size - 1;
        STREAM_TO_UINT8(length, stream);
    }

    // Prevents integer wrap-around when calculating (length + hdr_size).
    if (length != (packet->len - hdr_size)) {
        HCI_TRACE_ERROR("%s: SECURITY: parameter length (%d) exceeds packet bounds (%d)",
                        __func__, length, packet->len - hdr_size);
        osi_free(packet);
        return;
    }

    packet->event = outbound_event_types[PACKET_TYPE_TO_INDEX(type)];
    hci_hal_env.callbacks->packet_ready(packet);
}

static void host_send_pkt_available_cb(void)
{
    //Controller rx cache buffer is ready for receiving new host packet
    //Just Call Host main thread task to process pending packets.
    hci_downstream_data_post(OSI_THREAD_MAX_TIMEOUT);
}


void bt_record_hci_data(uint8_t *data, uint16_t len)
{
}

static int host_recv_pkt_cb(uint8_t *data, uint16_t len)
{
    //Target has packet to host, malloc new buffer for packet
    BT_HDR *pkt = NULL;
    size_t pkt_size;

    if ((hci_hal_env.rx_q == NULL) || (data == NULL)) {
        return 0;
    }

    bt_record_hci_data(data, len);

    bool is_adv_rpt = host_recv_adv_packet(data);

    if (!is_adv_rpt) {
        pkt_size = BT_HDR_SIZE + len;
        pkt = (BT_HDR *) osi_calloc(pkt_size);
        if (!pkt) {
            HCI_TRACE_ERROR("%s couldn't acquire memory for inbound data buffer.\n", __func__);
            assert(0);
        }

        pkt->offset = 0;
        pkt->len = len;
        pkt->layer_specific = 0;
        memcpy(pkt->data, data, len);
        {
            fixed_queue_enqueue(hci_hal_env.rx_q, pkt, FIXED_QUEUE_MAX_TIMEOUT);
        }
    } else {
        if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
            // Prevent race condition during host deinit/disable
            // Host not ready, dropped advertising report
            return 0;
        }
    assert(0);
    }

    hci_upstream_data_post(OSI_THREAD_MAX_TIMEOUT);

    BTTRC_DUMP_BUFFER("Recv Pkt", pkt->data, len);

    return 0;
}

static const esp_bluedroid_hci_driver_callbacks_t hci_host_cb = {
    .notify_host_send_available = host_send_pkt_available_cb,
    .notify_host_recv = host_recv_pkt_cb,
};

static const hci_hal_t interface = {
    hal_open,
    hal_close,
    transmit_data,
};

const hci_hal_t *hci_hal_h4_get_interface(void)
{
    return &interface;
}
