/*
 * SPDX-FileCopyrightText: 2015-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "btm_int.h"
#include "stack/hcimsgs.h"
#include "osi/allocator.h"
#include "device/controller.h"
#include <string.h>
#include "l2c_int.h"

tBTM_BLE_ISO_CBACK ble_iso_cb;

extern void btm_ble_inter_set(bool extble_inter);

void BTM_BleIsoRegisterCallback(tBTM_BLE_ISO_CBACK cb)
{
    if (cb) {
        ble_iso_cb = cb;
    } else {
        BTM_TRACE_ERROR("%s, register fail, the cb function is NULL.", __func__);
    }
}

void BTM_IsoBleCallbackTrigger(tBTM_BLE_5_GAP_EVENT event, tBTM_BLE_ISO_CB_PARAMS *params)
{
    if(params && params->status == BTM_SUCCESS) {
        btm_ble_inter_set(true);
    }
    if (ble_iso_cb) {
        ble_iso_cb(event, params);
    }
}

void btm_ble_iso_read_iso_tx_sync_complete(UINT8 *p)
{
    tBTM_BLE_ISO_CB_PARAMS cb_params = {0};
    STREAM_TO_UINT8(cb_params.btm_read_tx_sync.status, p);
    STREAM_TO_UINT16(cb_params.btm_read_tx_sync.conn_hdl, p);
    STREAM_TO_UINT16(cb_params.btm_read_tx_sync.pkt_seq_num, p);
    STREAM_TO_UINT32(cb_params.btm_read_tx_sync.tx_time_stamp, p);
    STREAM_TO_UINT24(cb_params.btm_read_tx_sync.time_offset, p);

    if (cb_params.btm_read_tx_sync.status != HCI_SUCCESS) {
        cb_params.btm_read_tx_sync.status = (BTM_HCI_ERROR | cb_params.btm_read_tx_sync.status);
    }
    cb_params.btm_read_tx_sync.conn_hdl = (cb_params.btm_read_tx_sync.conn_hdl & 0xEFF);
    cb_params.btm_read_tx_sync.time_offset = (cb_params.btm_read_tx_sync.time_offset & 0xFFFFFF);
    BTM_TRACE_DEBUG("read tx sync cmpl, status 0x%x conn_hdl 0x%x pkt_seq_num %d tx_time_stamp %ld time_offset %ld\n",
                    cb_params.btm_read_tx_sync.status, cb_params.btm_read_tx_sync.conn_hdl, cb_params.btm_read_tx_sync.pkt_seq_num,
                    cb_params.btm_read_tx_sync.tx_time_stamp, cb_params.btm_read_tx_sync.time_offset);

    BTM_IsoBleCallbackTrigger(BTM_BLE_ISO_READ_TX_SYNC_EVT, &cb_params);
}

void btm_ble_iso_read_iso_link_quality_complete(UINT8 *p)
{
    tBTM_BLE_ISO_CB_PARAMS cb_params = {0};
    STREAM_TO_UINT8(cb_params.btm_read_link_quality.status, p);
    STREAM_TO_UINT16(cb_params.btm_read_link_quality.conn_hdl, p);
    STREAM_TO_UINT32(cb_params.btm_read_link_quality.tx_unacked_pkts, p);
    STREAM_TO_UINT32(cb_params.btm_read_link_quality.tx_flushed_pkts, p);
    STREAM_TO_UINT32(cb_params.btm_read_link_quality.tx_last_subevt_pkts, p);
    STREAM_TO_UINT32(cb_params.btm_read_link_quality.retransmitted_pkts, p);
    STREAM_TO_UINT32(cb_params.btm_read_link_quality.crc_error_pkts, p);
    STREAM_TO_UINT32(cb_params.btm_read_link_quality.rx_unreceived_pkts, p);
    STREAM_TO_UINT32(cb_params.btm_read_link_quality.duplicate_pkts, p);

    if (cb_params.btm_read_link_quality.status != HCI_SUCCESS) {
        cb_params.btm_read_link_quality.status = (BTM_HCI_ERROR | cb_params.btm_read_link_quality.status);
    }

    BTM_TRACE_DEBUG("read link quality cmpl, status 0x%x conn_hdl 0x%x tx_unacked_pkts %d tx_flushed_pkts %ld tx_last_subevt_pkts %ld retransmitted_pkts %d\
                    crc_error_pkts %d rx_unreceived_pkts %d duplicate_pkts %d\n",
                    cb_params.btm_read_link_quality.status, cb_params.btm_read_link_quality.conn_hdl, cb_params.btm_read_link_quality.tx_unacked_pkts,
                    cb_params.btm_read_link_quality.tx_flushed_pkts, cb_params.btm_read_link_quality.tx_last_subevt_pkts, cb_params.btm_read_link_quality.retransmitted_pkts,
                    cb_params.btm_read_link_quality.crc_error_pkts, cb_params.btm_read_link_quality.rx_unreceived_pkts, cb_params.btm_read_link_quality.duplicate_pkts);

    BTM_IsoBleCallbackTrigger(BTM_BLE_ISO_READ_LINK_QUALITY_EVT, &cb_params);
}

void btm_ble_iso_data_path_update_complete(UINT16 opcode, UINT8 hci_status, UINT16 conn_handle)
{
    tBTM_BLE_ISO_CB_PARAMS cb_params = {0};
    BTM_TRACE_DEBUG("data path update cmpl, opcode 0x%x status 0x%x conn_hdl %d\n", opcode, hci_status, conn_handle);
    if (hci_status != HCI_SUCCESS) {
        hci_status = (BTM_HCI_ERROR | hci_status);
    }
    cb_params.status = hci_status;
    cb_params.btm_data_path_update.op_type = ((opcode == HCI_BLE_ISO_SET_DATA_PATH) ? BTM_BLE_ISO_DATA_PATH_SETUP : BTM_BLE_ISO_DATA_PATH_REMOVE);
    cb_params.btm_data_path_update.conn_hdl = conn_handle;

    BTM_IsoBleCallbackTrigger(BTM_BLE_ISO_DATA_PATH_UPFATE_EVT, &cb_params);
}

tBTM_STATUS BTM_BleIsoSetDataPath(uint16_t conn_handle, uint8_t data_path_dir, uint8_t data_path_id, uint8_t coding_fmt,
                                    uint16_t company_id, uint16_t vs_codec_id, uint32_t controller_delay, uint8_t codec_len,
                                    uint8_t *codec_cfg)
{
    tHCI_STATUS err = HCI_SUCCESS;
    tBTM_STATUS status = BTM_SUCCESS;

    if (codec_len && (codec_cfg == NULL)) {
        tBTM_BLE_ISO_CB_PARAMS cb_params = {0};
        status = BTM_ILLEGAL_VALUE;
        cb_params.btm_data_path_update.op_type = BTM_BLE_ISO_DATA_PATH_SETUP;
        cb_params.status = status;
        cb_params.btm_data_path_update.conn_hdl = conn_handle;
        BTM_TRACE_ERROR("codec_cfg is NULL, set data path error");
        BTM_IsoBleCallbackTrigger(BTM_BLE_ISO_DATA_PATH_UPFATE_EVT, &cb_params);
        return status;
    }

    BTM_TRACE_DEBUG("BTM_BleIsoSetDataPath conn_handle %d data_path_dir %d\n", conn_handle, data_path_dir);

    if ((err = btsnd_hcic_ble_iso_set_data_path(conn_handle, data_path_dir, data_path_id, coding_fmt, company_id, vs_codec_id,
                                                controller_delay, codec_len, codec_cfg)) != HCI_SUCCESS) {
        BTM_TRACE_ERROR("iso data path update, cmd err=0x%x", err);
        return BTM_HCI_ERROR | err;
    }

    return status;
}

tBTM_STATUS BTM_BleIsoRemoveDataPath(uint16_t conn_handle, uint8_t data_path_dir)
{
    tHCI_STATUS err = HCI_SUCCESS;

    BTM_TRACE_DEBUG("BTM_BleIsoRemoveDataPath conn_handle %d data_path_dir %d\n", conn_handle, data_path_dir);

    if ((err = btsnd_hcic_ble_iso_remove_data_path(conn_handle, data_path_dir)) != HCI_SUCCESS) {
        BTM_TRACE_ERROR("iso data path remove, cmd err=0x%x", err);
        return BTM_HCI_ERROR | err;
    }

    return BTM_SUCCESS;
}

tBTM_STATUS BTM_BleIsoReadTxSync(uint16_t iso_hdl)
{
    tHCI_STATUS err = HCI_SUCCESS;
    tBTM_STATUS status = BTM_SUCCESS;

    BTM_TRACE_DEBUG("BTM_BleIsoReadTxSync iso_hdl %d\n", iso_hdl);

    if ((err = btsnd_hcic_ble_iso_read_tx_sync(iso_hdl)) != HCI_SUCCESS) {
        BTM_TRACE_ERROR("iso read tx sync, cmd err=0x%x", err);
        status = BTM_HCI_ERROR | err;
    }

    return status;
}

tBTM_STATUS BTM_BleIsoReadLinkQuality(uint16_t iso_hdl)
{
    tHCI_STATUS err = HCI_SUCCESS;
    tBTM_STATUS status = BTM_SUCCESS;

    BTM_TRACE_DEBUG("BTM_BleIsoReadLinkQuality iso_hdl %d\n", iso_hdl);

    if ((err = btsnd_hcic_ble_iso_read_iso_link_quality(iso_hdl)) != HCI_SUCCESS) {
        BTM_TRACE_ERROR("iso read link quality, cmd err=0x%x", err);
        status = BTM_HCI_ERROR | err;
    }

    return status;
}

