/******************************************************************************
 *
 *  Copyright (C) 1999-2012 Broadcom Corporation
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

#ifndef HCIMSGS_H
#define HCIMSGS_H

#include <stddef.h>
#include "common/bt_target.h"
#include "stack/hcidefs.h"
#include "stack/bt_types.h"
#include "osi/pkt_queue.h"
#include "osi/allocator.h"

#define HCI_CMD_BUF_TYPE_METADATA       (0xa56e)

#define HCI_CMD_MSG_F_SRC_NOACK         (0x01)

typedef void (*hci_cmd_cmpl_cb)(BT_HDR *response, void *context);
typedef void (*hci_cmd_stat_cb)(uint8_t status, BT_HDR *command, void *context);
typedef void (*hci_cmd_free_cb)(pkt_linked_item_t *linked_pkt);

typedef struct {
    uint8_t flags_src;
    uint8_t flags_vnd; // used for downstream layer
    uint16_t opcode;
    hci_cmd_cmpl_cb command_complete_cb;
    hci_cmd_stat_cb command_status_cb;
    void *context;
    void *complete_future;
    hci_cmd_free_cb command_free_cb;
    BT_HDR command;
} hci_cmd_metadata_t;

#define HCI_CMD_METADATA_HDR_SIZE       (sizeof(hci_cmd_metadata_t))

#define HCI_CMD_LINKED_BUF_SIZE(paramlen)      (BT_PKT_LINKED_HDR_SIZE + HCI_CMD_METADATA_HDR_SIZE + HCIC_PREAMBLE_SIZE + (paramlen))

#define HCI_GET_CMD_METAMSG(cmd_ptr)  (hci_cmd_metadata_t *)((void *)(cmd_ptr) - offsetof(hci_cmd_metadata_t, command))
#define HCI_GET_CMD_LINKED_STRUCT(metadata_ptr) (pkt_linked_item_t *)((void *)(metadata_ptr) - offsetof(pkt_linked_item_t, data))

static inline BT_HDR *hci_get_cmd_buf(size_t param_len)
{
    pkt_linked_item_t *linked_pkt = osi_calloc(HCI_CMD_LINKED_BUF_SIZE(param_len));
    if (linked_pkt == NULL) {
        return NULL;
    }
    hci_cmd_metadata_t *metadata = (hci_cmd_metadata_t *)linked_pkt->data;
    BT_HDR *command = &metadata->command;

    command->layer_specific = HCI_CMD_BUF_TYPE_METADATA;
    command->len = HCIC_PREAMBLE_SIZE + param_len;
    command->offset = 0;

    return command;
}

static inline void hci_free_cmd_buf(BT_HDR *buf)
{
    assert(buf->layer_specific == HCI_CMD_BUF_TYPE_METADATA);
    hci_cmd_metadata_t *metadata = HCI_GET_CMD_METAMSG(buf);
    pkt_linked_item_t *linked_pkt = HCI_GET_CMD_LINKED_STRUCT(metadata);
    osi_free(linked_pkt);
}

#define HCI_GET_CMD_BUF(param_len)       hci_get_cmd_buf(param_len)
#define HCI_FREE_CMD_BUF(buf)            hci_free_cmd_buf(buf)

void bte_main_hci_send(BT_HDR *p_msg, UINT16 event);
void bte_main_lpm_allow_bt_device_sleep(void);

/* Message by message.... */

BOOLEAN btsnd_hcic_inquiry(const LAP inq_lap, UINT8 duration,
                           UINT8 response_cnt);

#define HCIC_PARAM_SIZE_INQUIRY 5


#define HCIC_INQ_INQ_LAP_OFF    0
#define HCIC_INQ_DUR_OFF        3
#define HCIC_INQ_RSP_CNT_OFF    4
/* Inquiry */

/* Inquiry Cancel */
BOOLEAN btsnd_hcic_inq_cancel(void);

#define HCIC_PARAM_SIZE_INQ_CANCEL   0

/* Periodic Inquiry Mode */
BOOLEAN btsnd_hcic_per_inq_mode(UINT16 max_period, UINT16 min_period,
                                const LAP inq_lap, UINT8 duration,
                                UINT8 response_cnt);

#define HCIC_PARAM_SIZE_PER_INQ_MODE    9

#define HCI_PER_INQ_MAX_INTRVL_OFF  0
#define HCI_PER_INQ_MIN_INTRVL_OFF  2
#define HCI_PER_INQ_INQ_LAP_OFF     4
#define HCI_PER_INQ_DURATION_OFF    7
#define HCI_PER_INQ_RSP_CNT_OFF     8
/* Periodic Inquiry Mode */

/* Exit Periodic Inquiry Mode */
BOOLEAN btsnd_hcic_exit_per_inq(void);

#define HCIC_PARAM_SIZE_EXIT_PER_INQ   0
/* Create Connection */
BOOLEAN btsnd_hcic_create_conn(BD_ADDR dest, UINT16 packet_types,
                               UINT8 page_scan_rep_mode,
                               UINT8 page_scan_mode,
                               UINT16 clock_offset,
                               UINT8 allow_switch);

#define HCIC_PARAM_SIZE_CREATE_CONN  13

#define HCIC_CR_CONN_BD_ADDR_OFF        0
#define HCIC_CR_CONN_PKT_TYPES_OFF      6
#define HCIC_CR_CONN_REP_MODE_OFF       8
#define HCIC_CR_CONN_PAGE_SCAN_MODE_OFF 9
#define HCIC_CR_CONN_CLK_OFF_OFF        10
#define HCIC_CR_CONN_ALLOW_SWITCH_OFF   12
/* Create Connection */

/* Disconnect */
BOOLEAN btsnd_hcic_disconnect(UINT16 handle, UINT8 reason);

#define HCIC_PARAM_SIZE_DISCONNECT 3

#define HCI_DISC_HANDLE_OFF             0
#define HCI_DISC_REASON_OFF             2
/* Disconnect */

#define HCIC_PARAM_SIZE_ADD_SCO_CONN    4

#define HCI_ADD_SCO_HANDLE_OFF          0
#define HCI_ADD_SCO_PACKET_TYPES_OFF    2
/* Add SCO Connection */

/* Create Connection Cancel */
BOOLEAN btsnd_hcic_create_conn_cancel(BD_ADDR dest);

#define HCIC_PARAM_SIZE_CREATE_CONN_CANCEL  6

#define HCIC_CR_CONN_CANCEL_BD_ADDR_OFF     0
/* Create Connection Cancel */

/* Accept Connection Request */
BOOLEAN btsnd_hcic_accept_conn (BD_ADDR bd_addr, UINT8 role);

#define HCIC_PARAM_SIZE_ACCEPT_CONN     7

#define HCI_ACC_CONN_BD_ADDR_OFF        0
#define HCI_ACC_CONN_ROLE_OFF           6
/* Accept Connection Request */

/* Reject Connection Request */
BOOLEAN btsnd_hcic_reject_conn (BD_ADDR bd_addr, UINT8 reason);

#define HCIC_PARAM_SIZE_REJECT_CONN      7

#define HCI_REJ_CONN_BD_ADDR_OFF        0
#define HCI_REJ_CONN_REASON_OFF         6
/* Reject Connection Request */

/* Link Key Request Reply */
BOOLEAN btsnd_hcic_link_key_req_reply (BD_ADDR bd_addr,
                                       LINK_KEY link_key);

#define HCIC_PARAM_SIZE_LINK_KEY_REQ_REPLY   22

#define HCI_LINK_KEY_REPLY_BD_ADDR_OFF  0
#define HCI_LINK_KEY_REPLY_LINK_KEY_OFF 6
/* Link Key Request Reply  */

/* Link Key Request Neg Reply */
BOOLEAN btsnd_hcic_link_key_neg_reply (BD_ADDR bd_addr);

#define HCIC_PARAM_SIZE_LINK_KEY_NEG_REPLY   6

#define HCI_LINK_KEY_NEG_REP_BD_ADR_OFF 0
/* Link Key Request Neg Reply  */

/* PIN Code Request Reply */
BOOLEAN btsnd_hcic_pin_code_req_reply (BD_ADDR bd_addr,
                                       UINT8 pin_code_len,
                                       PIN_CODE pin_code);

#define HCIC_PARAM_SIZE_PIN_CODE_REQ_REPLY   23

#define HCI_PIN_CODE_REPLY_BD_ADDR_OFF  0
#define HCI_PIN_CODE_REPLY_PIN_LEN_OFF  6
#define HCI_PIN_CODE_REPLY_PIN_CODE_OFF 7
/* PIN Code Request Reply  */

/* Link Key Request Neg Reply */
BOOLEAN btsnd_hcic_pin_code_neg_reply (BD_ADDR bd_addr);

#define HCIC_PARAM_SIZE_PIN_CODE_NEG_REPLY   6

#define HCI_PIN_CODE_NEG_REP_BD_ADR_OFF 0
/* Link Key Request Neg Reply  */

/* Change Connection Type */
BOOLEAN btsnd_hcic_change_conn_type (UINT16 handle, UINT16 packet_types);

#define HCIC_PARAM_SIZE_CHANGE_CONN_TYPE     4

#define HCI_CHNG_PKT_TYPE_HANDLE_OFF    0
#define HCI_CHNG_PKT_TYPE_PKT_TYPE_OFF  2
/* Change Connection Type */

#define HCIC_PARAM_SIZE_CMD_HANDLE      2

#define HCI_CMD_HANDLE_HANDLE_OFF       0

BOOLEAN btsnd_hcic_auth_request (UINT16 handle);     /* Authentication Request */

/* Set Connection Encryption */
BOOLEAN btsnd_hcic_set_conn_encrypt (UINT16 handle, BOOLEAN enable);
#define HCIC_PARAM_SIZE_SET_CONN_ENCRYPT     3


#define HCI_SET_ENCRYPT_HANDLE_OFF      0
#define HCI_SET_ENCRYPT_ENABLE_OFF      2
/* Set Connection Encryption */

/* Remote Name Request */
BOOLEAN btsnd_hcic_rmt_name_req (BD_ADDR bd_addr,
                                 UINT8 page_scan_rep_mode,
                                 UINT8 page_scan_mode,
                                 UINT16 clock_offset);

#define HCIC_PARAM_SIZE_RMT_NAME_REQ   10

#define HCI_RMT_NAME_BD_ADDR_OFF        0
#define HCI_RMT_NAME_REP_MODE_OFF       6
#define HCI_RMT_NAME_PAGE_SCAN_MODE_OFF 7
#define HCI_RMT_NAME_CLK_OFF_OFF        8
/* Remote Name Request */

/* Remote Name Request Cancel */
BOOLEAN btsnd_hcic_rmt_name_req_cancel(BD_ADDR bd_addr);

#define HCIC_PARAM_SIZE_RMT_NAME_REQ_CANCEL   6

#define HCI_RMT_NAME_CANCEL_BD_ADDR_OFF       0
/* Remote Name Request Cancel */

BOOLEAN btsnd_hcic_rmt_features_req(UINT16 handle);      /* Remote Features Request */

/* Remote Extended Features */
BOOLEAN btsnd_hcic_rmt_ext_features(UINT16 handle, UINT8 page_num);

#define HCIC_PARAM_SIZE_RMT_EXT_FEATURES   3

#define HCI_RMT_EXT_FEATURES_HANDLE_OFF    0
#define HCI_RMT_EXT_FEATURES_PAGE_NUM_OFF  2
/* Remote Extended Features */


BOOLEAN btsnd_hcic_rmt_ver_req(UINT16 handle);           /* Remote Version Info Request */
BOOLEAN btsnd_hcic_read_rmt_clk_offset(UINT16 handle);   /* Remote Clock Offset */
BOOLEAN btsnd_hcic_read_lmp_handle(UINT16 handle);       /* Remote LMP Handle */

BOOLEAN btsnd_hcic_setup_esco_conn (UINT16 handle,
                                    UINT32 tx_bw, UINT32 rx_bw,
                                    UINT16 max_latency, UINT16 voice,
                                    UINT8 retrans_effort,
                                    UINT16 packet_types);
#define HCIC_PARAM_SIZE_SETUP_ESCO      17

#define HCI_SETUP_ESCO_HANDLE_OFF       0
#define HCI_SETUP_ESCO_TX_BW_OFF        2
#define HCI_SETUP_ESCO_RX_BW_OFF        6
#define HCI_SETUP_ESCO_MAX_LAT_OFF      10
#define HCI_SETUP_ESCO_VOICE_OFF        12
#define HCI_SETUP_ESCO_RETRAN_EFF_OFF   14
#define HCI_SETUP_ESCO_PKT_TYPES_OFF    15


BOOLEAN btsnd_hcic_accept_esco_conn (BD_ADDR bd_addr,
                                     UINT32 tx_bw, UINT32 rx_bw,
                                     UINT16 max_latency,
                                     UINT16 content_fmt,
                                     UINT8 retrans_effort,
                                     UINT16 packet_types);
#define HCIC_PARAM_SIZE_ACCEPT_ESCO     21

#define HCI_ACCEPT_ESCO_BDADDR_OFF      0
#define HCI_ACCEPT_ESCO_TX_BW_OFF       6
#define HCI_ACCEPT_ESCO_RX_BW_OFF       10
#define HCI_ACCEPT_ESCO_MAX_LAT_OFF     14
#define HCI_ACCEPT_ESCO_VOICE_OFF       16
#define HCI_ACCEPT_ESCO_RETRAN_EFF_OFF  18
#define HCI_ACCEPT_ESCO_PKT_TYPES_OFF   19


BOOLEAN btsnd_hcic_reject_esco_conn (BD_ADDR bd_addr, UINT8 reason);
#define HCIC_PARAM_SIZE_REJECT_ESCO     7

#define HCI_REJECT_ESCO_BDADDR_OFF      0
#define HCI_REJECT_ESCO_REASON_OFF      6

/* Hold Mode */
BOOLEAN btsnd_hcic_hold_mode(UINT16 handle, UINT16 max_hold_period,
                             UINT16 min_hold_period);

#define HCIC_PARAM_SIZE_HOLD_MODE       6

#define HCI_HOLD_MODE_HANDLE_OFF        0
#define HCI_HOLD_MODE_MAX_PER_OFF       2
#define HCI_HOLD_MODE_MIN_PER_OFF       4
/* Hold Mode */

/**
 * Sniff Mode
 * sniff_attempt should no more than 0xFF
 */
BOOLEAN btsnd_hcic_sniff_mode(UINT16 handle,
                              UINT16 max_sniff_period,
                              UINT16 min_sniff_period,
                              UINT16 sniff_attempt,
                              UINT16 sniff_timeout);

#define HCIC_PARAM_SIZE_SNIFF_MODE      10


#define HCI_SNIFF_MODE_HANDLE_OFF       0
#define HCI_SNIFF_MODE_MAX_PER_OFF      2
#define HCI_SNIFF_MODE_MIN_PER_OFF      4
#define HCI_SNIFF_MODE_ATTEMPT_OFF      6
#define HCI_SNIFF_MODE_TIMEOUT_OFF      8
/* Sniff Mode */

BOOLEAN btsnd_hcic_exit_sniff_mode(UINT16 handle);       /* Exit Sniff Mode */

/* Park Mode */
BOOLEAN btsnd_hcic_park_mode (UINT16 handle,
                              UINT16 beacon_max_interval,
                              UINT16 beacon_min_interval);

#define HCIC_PARAM_SIZE_PARK_MODE       6

#define HCI_PARK_MODE_HANDLE_OFF        0
#define HCI_PARK_MODE_MAX_PER_OFF       2
#define HCI_PARK_MODE_MIN_PER_OFF       4
/* Park Mode */

BOOLEAN btsnd_hcic_exit_park_mode(UINT16 handle);  /* Exit Park Mode */

/* QoS Setup */
BOOLEAN btsnd_hcic_qos_setup (UINT16 handle, UINT8 flags,
                              UINT8 service_type,
                              UINT32 token_rate, UINT32 peak,
                              UINT32 latency, UINT32 delay_var);

#define HCIC_PARAM_SIZE_QOS_SETUP       20

#define HCI_QOS_HANDLE_OFF              0
#define HCI_QOS_FLAGS_OFF               2
#define HCI_QOS_SERVICE_TYPE_OFF        3
#define HCI_QOS_TOKEN_RATE_OFF          4
#define HCI_QOS_PEAK_BANDWIDTH_OFF      8
#define HCI_QOS_LATENCY_OFF             12
#define HCI_QOS_DELAY_VAR_OFF           16
/* QoS Setup */

/* Switch Role Request */
BOOLEAN btsnd_hcic_switch_role (BD_ADDR bd_addr, UINT8 role);

#define HCIC_PARAM_SIZE_SWITCH_ROLE  7

#define HCI_SWITCH_BD_ADDR_OFF          0
#define HCI_SWITCH_ROLE_OFF             6
/* Switch Role Request */

/* Write Policy Settings */
BOOLEAN btsnd_hcic_write_policy_set(UINT16 handle, UINT16 settings);

#define HCIC_PARAM_SIZE_WRITE_POLICY_SET     4

#define HCI_WRITE_POLICY_HANDLE_OFF          0
#define HCI_WRITE_POLICY_SETTINGS_OFF        2
/* Write Policy Settings */

/* Write Default Policy Settings */
BOOLEAN btsnd_hcic_write_def_policy_set(UINT16 settings);

#define HCIC_PARAM_SIZE_WRITE_DEF_POLICY_SET     2

#define HCI_WRITE_DEF_POLICY_SETTINGS_OFF        0
/* Write Default Policy Settings */

/******************************************
**    Lisbon Features
*******************************************/

#define btsnd_hcic_sniff_sub_rate(handle, max_lat, min_remote_lat, min_local_lat) FALSE

/* Extended Inquiry Response */
void btsnd_hcic_write_ext_inquiry_response(BT_HDR *buffer, UINT8 fec_req);

#define HCIC_PARAM_SIZE_EXT_INQ_RESP        241

#define HCIC_EXT_INQ_RESP_FEC_OFF     0
#define HCIC_EXT_INQ_RESP_RESPONSE    1
/* IO Capabilities Response */
BOOLEAN btsnd_hcic_io_cap_req_reply (BD_ADDR bd_addr, UINT8 capability,
                                     UINT8 oob_present, UINT8 auth_req);

#define HCIC_PARAM_SIZE_IO_CAP_RESP     9

#define HCI_IO_CAP_BD_ADDR_OFF          0
#define HCI_IO_CAPABILITY_OFF           6
#define HCI_IO_CAP_OOB_DATA_OFF         7
#define HCI_IO_CAP_AUTH_REQ_OFF         8

/* IO Capabilities Req Neg Reply */
BOOLEAN btsnd_hcic_io_cap_req_neg_reply (BD_ADDR bd_addr, UINT8 err_code);

#define HCIC_PARAM_SIZE_IO_CAP_NEG_REPLY 7

#define HCI_IO_CAP_NR_BD_ADDR_OFF        0
#define HCI_IO_CAP_NR_ERR_CODE           6

/* Read Local OOB Data */
BOOLEAN btsnd_hcic_read_local_oob_data (void);

#define HCIC_PARAM_SIZE_R_LOCAL_OOB     0


BOOLEAN btsnd_hcic_user_conf_reply (BD_ADDR bd_addr, BOOLEAN is_yes);

#define HCIC_PARAM_SIZE_UCONF_REPLY     6

#define HCI_USER_CONF_BD_ADDR_OFF       0


BOOLEAN btsnd_hcic_user_passkey_reply (BD_ADDR bd_addr, UINT32 value);

#define HCIC_PARAM_SIZE_U_PKEY_REPLY    10

#define HCI_USER_PASSKEY_BD_ADDR_OFF    0
#define HCI_USER_PASSKEY_VALUE_OFF      6


BOOLEAN btsnd_hcic_user_passkey_neg_reply (BD_ADDR bd_addr);

#define HCIC_PARAM_SIZE_U_PKEY_NEG_REPLY 6

#define HCI_USER_PASSKEY_NEG_BD_ADDR_OFF 0

/* Remote OOB Data Request Reply */
BOOLEAN btsnd_hcic_rem_oob_reply (BD_ADDR bd_addr, UINT8 *p_c,
                                  UINT8 *p_r);

#define HCIC_PARAM_SIZE_REM_OOB_REPLY   38

#define HCI_REM_OOB_DATA_BD_ADDR_OFF    0
#define HCI_REM_OOB_DATA_C_OFF          6
#define HCI_REM_OOB_DATA_R_OFF          22

/* Remote OOB Data Request Negative Reply */
BOOLEAN btsnd_hcic_rem_oob_neg_reply (BD_ADDR bd_addr);

#define HCIC_PARAM_SIZE_REM_OOB_NEG_REPLY   6

#define HCI_REM_OOB_DATA_NEG_BD_ADDR_OFF    0

/* Read Tx Power Level */
BOOLEAN btsnd_hcic_read_inq_tx_power (void);

#define HCIC_PARAM_SIZE_R_TX_POWER      0

/* Write Inquiry Tx Power Level */
BOOLEAN btsnd_hcic_write_inq_tx_power (INT8 tx_power);

#define HCIC_PARAM_SIZE_W_INQ_TX_POWER  1

/* Read Default Erroneous Data Reporting */
BOOLEAN btsnd_hcic_read_default_erroneous_data_rpt (void);

#define HCIC_PARAM_SIZE_R_ERR_DATA_RPT      0

BOOLEAN btsnd_hcic_enhanced_flush (UINT16 handle, UINT8 packet_type);

#define HCIC_PARAM_SIZE_ENHANCED_FLUSH  3


BOOLEAN btsnd_hcic_send_keypress_notif (BD_ADDR bd_addr, UINT8 notif);

#define HCIC_PARAM_SIZE_SEND_KEYPRESS_NOTIF    7

#define HCI_SEND_KEYPRESS_NOTIF_BD_ADDR_OFF    0
#define HCI_SEND_KEYPRESS_NOTIF_NOTIF_OFF      6

/**** end of Simple Pairing Commands ****/

/* Store Current Settings */
#define MAX_FILT_COND   (sizeof (BD_ADDR) + 1)

BOOLEAN btsnd_hcic_set_event_filter(UINT8 filt_type,
                                    UINT8 filt_cond_type,
                                    UINT8 *filt_cond,
                                    UINT8 filt_cond_len);

#define HCIC_PARAM_SIZE_SET_EVT_FILTER  9

#define HCI_FILT_COND_FILT_TYPE_OFF     0
#define HCI_FILT_COND_COND_TYPE_OFF     1
#define HCI_FILT_COND_FILT_OFF          2
/* Set Event Filter */

/* Delete Stored Key */
BOOLEAN btsnd_hcic_delete_stored_key (BD_ADDR bd_addr, BOOLEAN delete_all_flag);

#define HCIC_PARAM_SIZE_DELETE_STORED_KEY        7

#define HCI_DELETE_KEY_BD_ADDR_OFF      0
#define HCI_DELETE_KEY_ALL_FLAG_OFF     6
/* Delete Stored Key */

/* Change Local Name */
BOOLEAN btsnd_hcic_change_name (const UINT8 *name);

#define HCIC_PARAM_SIZE_CHANGE_NAME     BD_NAME_LEN

#define HCI_CHANGE_NAME_NAME_OFF        0
/* Change Local Name */


#define HCIC_PARAM_SIZE_READ_CMD     0

#define HCIC_PARAM_SIZE_WRITE_PARAM1     1

#define HCIC_WRITE_PARAM1_PARAM_OFF      0

#define HCIC_PARAM_SIZE_WRITE_PARAM2     2

#define HCIC_WRITE_PARAM2_PARAM_OFF      0

#define HCIC_PARAM_SIZE_WRITE_PARAM3     3

#define HCIC_WRITE_PARAM3_PARAM_OFF      0

/* set AFH channels */
BOOLEAN btsnd_hcic_set_afh_channels (AFH_CHANNELS channels);
#define HCIC_PARAM_SIZE_SET_AFH_CHANNELS    10
BOOLEAN btsnd_hcic_ble_set_channels (BLE_CHANNELS channels);
#define HCIC_PARAM_SIZE_BLE_SET_CHANNELS    5

/* set minimum encryption key size */
BOOLEAN btsnd_hcic_set_min_enc_key_size (UINT8 size);
#define HCIC_PARAM_SIZE_SET_MIN_ENC_KEY_SIZE    1

BOOLEAN btsnd_hcic_write_pin_type(UINT8 type);                   /* Write PIN Type */
BOOLEAN btsnd_hcic_write_auto_accept(UINT8 flag);                /* Write Auto Accept */
BOOLEAN btsnd_hcic_read_name (void);                             /* Read Local Name */
BOOLEAN btsnd_hcic_write_page_tout(UINT16 timeout);              /* Write Page Timeout */
BOOLEAN btsnd_hcic_write_scan_enable(UINT8 flag);                /* Write Scan Enable */
BOOLEAN btsnd_hcic_write_pagescan_cfg(UINT16 interval,
                                      UINT16 window);            /* Write Page Scan Activity */

#define HCIC_PARAM_SIZE_WRITE_PAGESCAN_CFG  4

#define HCI_SCAN_CFG_INTERVAL_OFF       0
#define HCI_SCAN_CFG_WINDOW_OFF         2
/* Write Page Scan Activity */

/* Write Inquiry Scan Activity */
BOOLEAN btsnd_hcic_write_inqscan_cfg(UINT16 interval, UINT16 window);

#define HCIC_PARAM_SIZE_WRITE_INQSCAN_CFG    4

#define HCI_SCAN_CFG_INTERVAL_OFF       0
#define HCI_SCAN_CFG_WINDOW_OFF         2
/* Write Inquiry Scan Activity */

BOOLEAN btsnd_hcic_write_auth_enable(UINT8 flag);                 /* Write Authentication Enable */
BOOLEAN btsnd_hcic_write_dev_class(DEV_CLASS dev);                /* Write Class of Device */
BOOLEAN btsnd_hcic_write_voice_settings(UINT16 flags);            /* Write Voice Settings */

/* Host Controller to Host flow control */
#define HCI_HOST_FLOW_CTRL_OFF          0
#define HCI_HOST_FLOW_CTRL_ACL_ON       1
#define HCI_HOST_FLOW_CTRL_SCO_ON       2
#define HCI_HOST_FLOW_CTRL_BOTH_ON      3

#define  HCI_HOST_FLOW_CTRL_ADV_REPORT_OFF     0
#define  HCI_HOST_FLOW_CTRL_ADV_REPORT_ON      1


BOOLEAN btsnd_hcic_write_auto_flush_tout(UINT16 handle,
        UINT16 timeout);    /* Write Retransmit Timeout */

#define HCIC_PARAM_SIZE_WRITE_AUTO_FLUSH_TOUT    4

#define HCI_FLUSH_TOUT_HANDLE_OFF       0
#define HCI_FLUSH_TOUT_TOUT_OFF         2

BOOLEAN btsnd_hcic_read_tx_power(UINT16 handle, UINT8 type);     /* Read Tx Power */

#define HCIC_PARAM_SIZE_READ_TX_POWER    3

#define HCI_READ_TX_POWER_HANDLE_OFF    0
#define HCI_READ_TX_POWER_TYPE_OFF      2

/* Read transmit power level parameter */
#define HCI_READ_CURRENT                0x00
#define HCI_READ_MAXIMUM                0x01

BOOLEAN btsnd_hcic_host_num_xmitted_pkts (UINT8 num_handles,
        UINT16 *handle,
        UINT16 *num_pkts);         /* Set Host Buffer Size */

#define HCIC_PARAM_SIZE_NUM_PKTS_DONE_SIZE    sizeof(btmsg_hcic_num_pkts_done_t)

#define MAX_DATA_HANDLES        10

#define HCI_PKTS_DONE_NUM_HANDLES_OFF   0
#define HCI_PKTS_DONE_HANDLE_OFF        1
#define HCI_PKTS_DONE_NUM_PKTS_OFF      3

/* Write Link Supervision Timeout */
BOOLEAN btsnd_hcic_write_link_super_tout(UINT8 local_controller_id, UINT16 handle, UINT16 timeout);

#define HCIC_PARAM_SIZE_WRITE_LINK_SUPER_TOUT        4

#define HCI_LINK_SUPER_TOUT_HANDLE_OFF  0
#define HCI_LINK_SUPER_TOUT_TOUT_OFF    2
/* Write Link Supervision Timeout */

BOOLEAN btsnd_hcic_write_cur_iac_lap (UINT8 num_cur_iac,
                                      LAP *const iac_lap);   /* Write Current IAC LAP */

#define MAX_IAC_LAPS    0x40

#define HCI_WRITE_IAC_LAP_NUM_OFF       0
#define HCI_WRITE_IAC_LAP_LAP_OFF       1
/* Write Current IAC LAP */

BOOLEAN btsnd_hcic_get_link_quality (UINT16 handle);            /* Get Link Quality */
BOOLEAN btsnd_hcic_read_rssi (UINT16 handle);                   /* Read RSSI */
BOOLEAN btsnd_hcic_enable_test_mode (void);                     /* Enable Device Under Test Mode */
BOOLEAN btsnd_hcic_write_pagescan_type(UINT8 type);             /* Write Page Scan Type */
BOOLEAN btsnd_hcic_write_inqscan_type(UINT8 type);              /* Write Inquiry Scan Type */
BOOLEAN btsnd_hcic_write_inquiry_mode(UINT8 type);              /* Write Inquiry Mode */

#define HCI_DATA_HANDLE_MASK 0x0FFF

#define HCID_GET_HANDLE_EVENT(p)  (UINT16)((*((UINT8 *)((p) + 1) + p->offset) + \
                                           (*((UINT8 *)((p) + 1) + p->offset + 1) << 8)))

#define HCID_GET_HANDLE(u16) (UINT16)((u16) & HCI_DATA_HANDLE_MASK)

#define HCI_DATA_EVENT_MASK   3
#define HCI_DATA_EVENT_OFFSET 12
#define HCID_GET_EVENT(u16)   (UINT8)(((u16) >> HCI_DATA_EVENT_OFFSET) & HCI_DATA_EVENT_MASK)

#define HCI_DATA_BCAST_MASK   3
#define HCI_DATA_BCAST_OFFSET 10
#define HCID_GET_BCAST(u16)   (UINT8)(((u16) >> HCI_DATA_BCAST_OFFSET) & HCI_DATA_BCAST_MASK)

#define HCID_GET_ACL_LEN(p)     (UINT16)((*((UINT8 *)((p) + 1) + p->offset + 2) + \
                                         (*((UINT8 *)((p) + 1) + p->offset + 3) << 8)))

#define HCID_HEADER_SIZE      4

#define HCID_GET_SCO_LEN(p)  (*((UINT8 *)((p) + 1) + p->offset + 2))

void btsnd_hcic_vendor_spec_cmd (BT_HDR *buffer, UINT16 opcode,
                                 UINT8 len, UINT8 *p_data,
                                 void *p_cmd_cplt_cback);

BOOLEAN btsnd_hcic_read_authenticated_payload_tout(UINT16 handle);

BOOLEAN btsnd_hcic_write_authenticated_payload_tout(UINT16 handle,
        UINT16 timeout);

BOOLEAN btsnd_hcic_ble_update_adv_report_flow_control (UINT16 num, BT_HDR *static_buf);

#define HCIC_PARAM_SIZE_WRITE_AUTHENT_PAYLOAD_TOUT  4

#define HCI__WRITE_AUTHENT_PAYLOAD_TOUT_HANDLE_OFF  0
#define HCI__WRITE_AUTHENT_PAYLOAD_TOUT_TOUT_OFF    2

#endif
