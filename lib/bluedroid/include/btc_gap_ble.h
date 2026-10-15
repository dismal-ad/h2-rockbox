/*
 * SPDX-FileCopyrightText: 2015-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __BTC_GAP_BLE_H__
#define __BTC_GAP_BLE_H__

#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"

#define BLE_ISVALID_PARAM(x, min, max)  (((x) >= (min) && (x) <= (max)))

#define BLE_VENDOR_PDU_RECV_EVT         (0xC0)
#define BLE_VENDOR_CHMAP_UPDATE_EVT     (0xC1)
#define BLE_VENDOR_SLEEP_WAKEUP_EVT     (0xC3)

typedef enum {
    BTC_GAP_BLE_ACT_UPDATE_CONN_PARAM = 6,
    BTC_GAP_BLE_ACT_SET_PKT_DATA_LEN,
    BTC_GAP_BLE_ACT_SET_RAND_ADDRESS,
    BTC_GAP_BLE_ACT_CLEAR_RAND_ADDRESS,
    BTC_GAP_BLE_ACT_CONFIG_LOCAL_PRIVACY,
    BTC_GAP_BLE_ACT_CONFIG_LOCAL_ICON,
    BTC_GAP_BLE_ACT_UPDATE_WHITE_LIST,
    BTC_GAP_BLE_ACT_CLEAR_WHITE_LIST,
    BTC_GAP_BLE_ACT_SET_DEV_NAME = 15,
    BTC_GAP_BLE_ACT_READ_RSSI = 18,
    BTC_GAP_BLE_SET_ENCRYPTION_EVT,
    BTC_GAP_BLE_SET_SECURITY_PARAM_EVT,
    BTC_GAP_BLE_SECURITY_RSP_EVT,
    BTC_GAP_BLE_PASSKEY_REPLY_EVT,
    BTC_GAP_BLE_CONFIRM_REPLY_EVT,
    BTC_GAP_BLE_DISCONNECT_EVT,
    BTC_GAP_BLE_REMOVE_BOND_DEV_EVT,
    BTC_GAP_BLE_OOB_REQ_REPLY_EVT,
    BTC_GAP_BLE_SC_OOB_REQ_REPLY_EVT,
    BTC_GAP_BLE_SC_CR_OOB_DATA_EVT,
    BTC_GAP_BLE_UPDATE_DUPLICATE_SCAN_EXCEPTIONAL_LIST,
    BTC_GAP_BLE_SET_AFH_CHANNELS,
    BTC_GAP_BLE_ACT_GET_DEV_NAME,
    BTC_GAP_BLE_DTM_STOP,
    BTC_GAP_BLE_ACT_SET_RESOLVABLE_PRIVATE_ADDRESS_TIMEOUT,
    BTC_GAP_BLE_ACT_ADD_DEVICE_TO_RESOLVING_LIST,
    BTC_GAP_BLE_SET_PRIVACY_MODE,
    BTC_GAP_BLE_READ_CHANNEL_MAP,
} btc_gap_ble_act_t;

/* btc_ble_gap_args_t */
typedef union {
    //BTC_GAP_BLE_ACT_UPDATE_CONN_PARAM,
    struct conn_update_params_args {
        esp_ble_conn_update_params_t conn_params;
    } conn_update_params;
    //BTC_GAP_BLE_ACT_SET_PKT_DATA_LEN
    struct set_pkt_data_len_args {
        esp_bd_addr_t remote_device;
        uint16_t tx_data_length;
    } set_pkt_data_len;
    //BTC_GAP_BLE_ACT_SET_RAND_ADDRESS,
    struct set_rand_addr_args {
        esp_bd_addr_t rand_addr;
    } set_rand_addr;
    // BTC_GAP_BLE_ACT_SET_RESOLVABLE_PRIVATE_ADDRESS_TIMEOUT
    struct set_rpa_timeout_args {
        uint16_t rpa_timeout;
    } set_rpa_timeout;
    //BTC_GAP_BLE_ACT_ADD_DEVICE_TO_RESOLVING_LIST
    struct add_dev_to_resolving_list_args {
        esp_bd_addr_t addr;
        uint8_t addr_type;
        uint8_t irk[PEER_IRK_LEN];
    } add_dev_to_resolving_list;
    //BTC_GAP_BLE_ACT_CONFIG_LOCAL_PRIVACY,
    struct cfg_local_privacy_args {
        bool privacy_enable;
    } cfg_local_privacy;
    //BTC_GAP_BLE_ACT_CONFIG_LOCAL_ICON,
    struct cfg_local_icon_args {
        uint16_t icon;
    } cfg_local_icon;
    //BTC_GAP_BLE_ACT_UPDATE_WHITE_LIST
    struct update_white_list_args {
        bool add_remove;
        esp_bd_addr_t remote_bda;
        esp_ble_wl_addr_type_t wl_addr_type;
    } update_white_list;
    //BTC_GAP_BLE_ACT_SET_DEV_NAME,
    struct set_dev_name_args {
#define ESP_GAP_DEVICE_NAME_MAX (32)
        char *device_name;
    } set_dev_name;
    //BTC_GAP_BLE_SET_ENCRYPTION_EVT
    struct set_encryption_args {
        esp_bd_addr_t bd_addr;
        esp_ble_sec_act_t sec_act;
    } set_encryption;
    //BTC_GAP_BLE_SET_SECURITY_PARAM_EVT
    struct set_security_param_args {
        esp_ble_sm_param_t param_type;
        uint8_t len;
        uint8_t *value;
    } set_security_param;
    //BTC_GAP_BLE_SECURITY_RSP_EVT
    struct enc_rsp_args {
        esp_bd_addr_t bd_addr;
        bool accept;
    } sec_rsp;
    //BTC_GAP_BLE_PASSKEY_REPLY_EVT
    struct enc_passkey_reply_args {
        esp_bd_addr_t bd_addr;
        bool accept;
        uint32_t passkey;
    } enc_passkey_replay;
    //BTC_GAP_BLE_CONFIRM_REPLY_EVT
    struct enc_comfirm_reply_args {
        esp_bd_addr_t bd_addr;
        bool accept;
    } enc_comfirm_replay;
    //BTC_GAP_BLE_OOB_DATA_REPLY_EVT
    struct oob_req_reply_args {
        esp_bd_addr_t bd_addr;
        uint8_t len;
        uint8_t *p_value;
    } oob_req_reply;
    struct sc_oob_req_reply_args {
        esp_bd_addr_t bd_addr;
        uint8_t *p_c;
        uint8_t *p_r;
    } sc_oob_req_reply;
    //BTC_GAP_BLE_DISCONNECT_EVT
    struct disconnect_args {
        esp_bd_addr_t remote_device;
    } disconnect;
    //BTC_GAP_BLE_REMOVE_BOND_DEV_EVT
    struct remove_bond_device_args {
        esp_bd_addr_t bd_addr;
    } remove_bond_device;
    //BTC_GAP_BLE_ACT_READ_RSSI
    struct read_rssi_args {
        esp_bd_addr_t remote_addr;
    } read_rssi;
    // BTC_GAP_BLE_SET_AFH_CHANNELS
    struct set_channels_args {
       esp_gap_ble_channels channels;
    } set_channels;
    //BTC_DEV_VENDOR_HCI_CMD_EVT
    struct vendor_cmd_send_args {
        uint16_t  opcode;
        uint8_t  param_len;
        uint8_t *p_param_buf;
    } vendor_cmd_send;
    // BTC_GAP_BLE_SET_PRIVACY_MODE
    struct set_privacy_mode {
        esp_ble_addr_type_t addr_type;
        esp_bd_addr_t addr;
        uint8_t privacy_mode;
    } set_privacy_mode;
    // BTC_GAP_BLE_SET_CSA_SUPPORT
    struct set_csa_support_args {
        uint8_t csa_select;
    } set_csa_support;
    // BTC_GAP_BLE_ACT_SET_VENDOR_EVT_MASK
    struct set_vendor_evt_mask_args {
        uint32_t evt_mask;
    } set_vendor_evt_mask;
    struct read_channel_map_args {
        esp_bd_addr_t bd_addr;
    } read_channel_map;
} btc_ble_gap_args_t;

void btc_gap_ble_call_handler(btc_msg_t *msg);
void btc_gap_ble_cb_handler(btc_msg_t *msg);
void btc_get_whitelist_size(uint16_t *length);
void btc_gap_ble_arg_deep_copy(btc_msg_t *msg, void *p_dest, void *p_src);
void btc_gap_ble_arg_deep_free(btc_msg_t *msg);
void btc_gap_ble_cb_deep_free(btc_msg_t *msg);
void btc_gap_ble_cb_deep_copy(btc_msg_t *msg, void *p_dest, void *p_src);
void btc_gap_callback_init(void);
bool btc_gap_ble_init(void);
void btc_gap_ble_deinit(void);
void btc_adv_list_init(void);
void btc_adv_list_deinit(void);

#endif /* __BTC_GAP_BLE_H__ */
