/*
 * SPDX-FileCopyrightText: 2015-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "bta/bta_api.h"
#include "common/bt_trace.h"
#include "btc/btc_manage.h"
#include "btc_gap_ble.h"
#include "btc/btc_ble_storage.h"
#include "esp_random.h"

esp_err_t esp_ble_gap_register_callback(esp_gap_ble_cb_t callback)
{
    ESP_BLUEDROID_STATUS_CHECK(ESP_BLUEDROID_STATUS_ENABLED);

    return (btc_profile_cb_set(BTC_PID_GAP_BLE, callback) == 0 ? ESP_OK : ESP_FAIL);
}

esp_gap_ble_cb_t esp_ble_gap_get_callback(void)
{
    return (esp_gap_ble_cb_t) btc_profile_cb_get(BTC_PID_GAP_BLE);
}

esp_err_t esp_ble_gap_update_conn_params(esp_ble_conn_update_params_t *params)
{
    btc_msg_t msg = {0};
    btc_ble_gap_args_t arg;

    ESP_BLUEDROID_STATUS_CHECK(ESP_BLUEDROID_STATUS_ENABLED);
    if(!params) {
        LOG_ERROR("%s,params is NULL", __func__);
        return ESP_FAIL;
    }

    if (ESP_BLE_IS_VALID_PARAM(params->min_int, ESP_BLE_CONN_INT_MIN, ESP_BLE_CONN_INT_MAX) &&
        ESP_BLE_IS_VALID_PARAM(params->max_int, ESP_BLE_CONN_INT_MIN, ESP_BLE_CONN_INT_MAX) &&
        ESP_BLE_IS_VALID_PARAM(params->timeout, ESP_BLE_CONN_SUP_TOUT_MIN, ESP_BLE_CONN_SUP_TOUT_MAX) &&
        (params->latency <= ESP_BLE_CONN_LATENCY_MAX) &&
        ((params->timeout * 10) >= ((1 + params->latency) * ((params->max_int * 5) >> 1))) && params->min_int <= params->max_int) {

        msg.sig = BTC_SIG_API_CALL;
        msg.pid = BTC_PID_GAP_BLE;
        msg.act = BTC_GAP_BLE_ACT_UPDATE_CONN_PARAM;
        memcpy(&arg.conn_update_params.conn_params, params, sizeof(esp_ble_conn_update_params_t));

        return (btc_transfer_context(&msg, &arg, sizeof(btc_ble_gap_args_t), NULL, NULL) == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
    } else {
        LOG_ERROR("%s,invalid connection params:min_int = %d, max_int = %d, latency = %d, timeout = %d",\
                            __func__, params->min_int, params->max_int, params->latency, params->timeout);
        return ESP_FAIL;
    }
}

esp_err_t esp_ble_gap_set_pkt_data_len(esp_bd_addr_t remote_device, uint16_t tx_data_length)
{
    btc_msg_t msg = {0};
    btc_ble_gap_args_t arg;

    ESP_BLUEDROID_STATUS_CHECK(ESP_BLUEDROID_STATUS_ENABLED);

    if (remote_device == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_GAP_BLE;
    msg.act = BTC_GAP_BLE_ACT_SET_PKT_DATA_LEN;
    arg.set_pkt_data_len.tx_data_length = tx_data_length;
    memcpy(arg.set_pkt_data_len.remote_device, remote_device, ESP_BD_ADDR_LEN);

    return (btc_transfer_context(&msg, &arg, sizeof(btc_ble_gap_args_t), NULL, NULL) == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}

esp_err_t esp_ble_gap_addr_create_static(esp_bd_addr_t rand_addr)
{
    if (rand_addr == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    // Static device address: First two bits are '11', rest is random
    rand_addr[0] = 0xC0 | (esp_random() & 0x3F);
    for (int i = 1; i < 6; i++) {
        rand_addr[i] = esp_random() & 0xFF; // Randomize remaining bits
    }
    return ESP_OK;
}

esp_err_t esp_ble_gap_addr_create_nrpa(esp_bd_addr_t rand_addr)
{
    if (rand_addr == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    // Non-resolvable private address: First two bits are '00', rest is random
    rand_addr[0] = (esp_random() & 0x3F);
    for (int i = 1; i < 6; i++) {
        rand_addr[i] = esp_random() & 0xFF; // Randomize remaining bits
    }
    return ESP_OK;
}

esp_err_t esp_ble_gap_set_rand_addr(esp_bd_addr_t rand_addr)
{
    btc_msg_t msg = {0};
    btc_ble_gap_args_t arg;

    ESP_BLUEDROID_STATUS_CHECK(ESP_BLUEDROID_STATUS_ENABLED);

    if (rand_addr == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_GAP_BLE;
    msg.act = BTC_GAP_BLE_ACT_SET_RAND_ADDRESS;
    memcpy(arg.set_rand_addr.rand_addr, rand_addr, ESP_BD_ADDR_LEN);

    return (btc_transfer_context(&msg, &arg, sizeof(btc_ble_gap_args_t), NULL, NULL) == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}

esp_err_t esp_ble_gap_set_resolvable_private_address_timeout(uint16_t rpa_timeout)
{
    ESP_BLUEDROID_STATUS_CHECK(ESP_BLUEDROID_STATUS_ENABLED);

    if (rpa_timeout < 0x0001 || rpa_timeout > 0x0E10) {
        return ESP_ERR_INVALID_ARG;
    }

    btc_msg_t msg = {0};
    btc_ble_gap_args_t arg;

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_GAP_BLE;
    msg.act = BTC_GAP_BLE_ACT_SET_RESOLVABLE_PRIVATE_ADDRESS_TIMEOUT;
    arg.set_rpa_timeout.rpa_timeout = rpa_timeout;

    return (btc_transfer_context(&msg, &arg, sizeof(btc_ble_gap_args_t), NULL, NULL) == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}


esp_err_t esp_ble_gap_add_device_to_resolving_list(esp_bd_addr_t peer_addr, uint8_t addr_type, uint8_t *peer_irk)
{
    ESP_BLUEDROID_STATUS_CHECK(ESP_BLUEDROID_STATUS_ENABLED);

    if (addr_type > BLE_ADDR_TYPE_RANDOM || !peer_addr || !peer_irk || (addr_type && ((peer_addr[0] & 0xC0) != 0xC0))) {
        return ESP_ERR_INVALID_ARG;
    }

    btc_msg_t msg = {0};
    btc_ble_gap_args_t arg;

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_GAP_BLE;
    msg.act = BTC_GAP_BLE_ACT_ADD_DEVICE_TO_RESOLVING_LIST;

    memcpy(arg.add_dev_to_resolving_list.addr, peer_addr, ESP_BD_ADDR_LEN);
    arg.add_dev_to_resolving_list.addr_type = addr_type;
    memcpy(arg.add_dev_to_resolving_list.irk, peer_irk, ESP_PEER_IRK_LEN);

    return (btc_transfer_context(&msg, &arg, sizeof(btc_ble_gap_args_t), NULL, NULL) == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}

esp_err_t esp_ble_gap_clear_rand_addr(void)
{
    btc_msg_t msg;

    ESP_BLUEDROID_STATUS_CHECK(ESP_BLUEDROID_STATUS_ENABLED);

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_GAP_BLE;
    msg.act = BTC_GAP_BLE_ACT_CLEAR_RAND_ADDRESS;

    return (btc_transfer_context(&msg, NULL, 0, NULL, NULL) == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}

esp_err_t esp_ble_gap_config_local_privacy (bool privacy_enable)
{
    btc_msg_t msg = {0};
    btc_ble_gap_args_t arg;

    ESP_BLUEDROID_STATUS_CHECK(ESP_BLUEDROID_STATUS_ENABLED);

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_GAP_BLE;
    msg.act = BTC_GAP_BLE_ACT_CONFIG_LOCAL_PRIVACY;
    arg.cfg_local_privacy.privacy_enable = privacy_enable;

    return (btc_transfer_context(&msg, &arg, sizeof(btc_ble_gap_args_t), NULL, NULL) == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}

esp_err_t esp_ble_gap_config_local_icon (uint16_t icon)
{
    btc_msg_t msg = {0};
    btc_ble_gap_args_t arg;

    ESP_BLUEDROID_STATUS_CHECK(ESP_BLUEDROID_STATUS_ENABLED);

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_GAP_BLE;
    msg.act = BTC_GAP_BLE_ACT_CONFIG_LOCAL_ICON;
    arg.cfg_local_icon.icon = icon;

    return (btc_transfer_context(&msg, &arg, sizeof(btc_ble_gap_args_t), NULL, NULL) == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}

esp_err_t esp_ble_gap_update_whitelist(bool add_remove, esp_bd_addr_t remote_bda, esp_ble_wl_addr_type_t wl_addr_type)
{
    btc_msg_t msg = {0};
    btc_ble_gap_args_t arg;

    if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!remote_bda){
        return ESP_ERR_INVALID_SIZE;
    }
    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_GAP_BLE;
    msg.act = BTC_GAP_BLE_ACT_UPDATE_WHITE_LIST;
    arg.update_white_list.add_remove = add_remove;
    arg.update_white_list.wl_addr_type = wl_addr_type;
    memcpy(arg.update_white_list.remote_bda, remote_bda, sizeof(esp_bd_addr_t));

    return (btc_transfer_context(&msg, &arg, sizeof(btc_ble_gap_args_t), NULL, NULL)
                == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}

esp_err_t esp_ble_gap_clear_whitelist(void)
{
    btc_msg_t msg;

    if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
        return ESP_ERR_INVALID_STATE;
    }
    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_GAP_BLE;
    msg.act = BTC_GAP_BLE_ACT_CLEAR_WHITE_LIST;

    return (btc_transfer_context(&msg, NULL, 0, NULL, NULL) == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}

esp_err_t esp_ble_gap_get_whitelist_size(uint16_t *length)
{
    if (length == NULL) {
        return ESP_FAIL;
    }
    btc_get_whitelist_size(length);

    return ESP_OK;
}

esp_err_t esp_ble_gap_set_device_name(const char *name)
{
    btc_msg_t msg = {0};
    btc_ble_gap_args_t arg;

    if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!name){
        return ESP_ERR_INVALID_ARG;
    }
    if (strlen(name) > BTC_MAX_LOC_BD_NAME_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_GAP_BLE;
    msg.act = BTC_GAP_BLE_ACT_SET_DEV_NAME;
    arg.set_dev_name.device_name = (char *)name;

    return (btc_transfer_context(&msg, &arg, sizeof(btc_ble_gap_args_t), btc_gap_ble_arg_deep_copy, btc_gap_ble_arg_deep_free) == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}

esp_err_t esp_ble_gap_get_device_name(void)
{
    btc_msg_t msg = {0};

    ESP_BLUEDROID_STATUS_CHECK(ESP_BLUEDROID_STATUS_ENABLED);

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_GAP_BLE;
    msg.act = BTC_GAP_BLE_ACT_GET_DEV_NAME;

    return (btc_transfer_context(&msg, NULL, 0, NULL, NULL) == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}

esp_err_t esp_ble_gap_get_local_used_addr(esp_bd_addr_t local_used_addr, uint8_t * addr_type)
{
    if(esp_bluedroid_get_status() != (ESP_BLUEDROID_STATUS_ENABLED)) {
        LOG_ERROR("%s, bluedroid status error", __func__);
        return ESP_FAIL;
    }
    if (local_used_addr == NULL || addr_type == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if(!BTM_BleGetCurrentAddress(local_used_addr, addr_type)) {
        return ESP_FAIL;
    }
    return ESP_OK;
}
esp_err_t esp_ble_gap_read_rssi(esp_bd_addr_t remote_addr)
{
    btc_msg_t msg = {0};
    btc_ble_gap_args_t arg;

    if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
        return ESP_ERR_INVALID_STATE;
    }

    if (remote_addr == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_GAP_BLE;
    msg.act = BTC_GAP_BLE_ACT_READ_RSSI;
    memcpy(arg.read_rssi.remote_addr, remote_addr, sizeof(esp_bd_addr_t));

    return (btc_transfer_context(&msg, &arg, sizeof(btc_ble_gap_args_t), NULL, NULL) == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}

esp_err_t esp_ble_gap_set_security_param(esp_ble_sm_param_t param_type,
        void *value, uint8_t len)
{
    if(param_type >= ESP_BLE_SM_MAX_PARAM) {
        return ESP_ERR_INVALID_ARG;
    }
    if((param_type != ESP_BLE_SM_CLEAR_STATIC_PASSKEY) && ( value == NULL || len < sizeof(uint8_t) || len > sizeof(uint32_t))) {
        return ESP_ERR_INVALID_ARG;
    }
    if(param_type == ESP_BLE_SM_SET_STATIC_PASSKEY) {
        uint32_t passkey = 0;
        for(uint8_t i = 0; i < len; i++)
        {
            passkey += (((uint8_t *)value)[i]<<(8*i));
        }
        if(passkey > 999999) {
            return ESP_ERR_INVALID_ARG;
        }
    }
    if (param_type == ESP_BLE_APP_ENC_KEY_SIZE) {
        LOG_ERROR("ESP_BLE_APP_ENC_KEY_SIZE is deprecated, use ESP_GATT_PERM_ENCRYPT_KEY_SIZE in characteristic definition");
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (param_type == ESP_BLE_SM_MAX_KEY_SIZE || param_type == ESP_BLE_SM_MIN_KEY_SIZE) {
        if (((uint8_t *)value)[0] > 16 || ((uint8_t *)value)[0] < 7) {
            return ESP_ERR_INVALID_ARG;
        }
    }
    btc_msg_t msg = {0};
    btc_ble_gap_args_t arg;

    ESP_BLUEDROID_STATUS_CHECK(ESP_BLUEDROID_STATUS_ENABLED);

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_GAP_BLE;
    msg.act = BTC_GAP_BLE_SET_SECURITY_PARAM_EVT;
    arg.set_security_param.param_type = param_type;
    arg.set_security_param.len = len;
    arg.set_security_param.value = value;

    return (btc_transfer_context(&msg, &arg, sizeof(btc_ble_gap_args_t), btc_gap_ble_arg_deep_copy,
                btc_gap_ble_arg_deep_free) == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}

esp_err_t esp_ble_set_encryption(esp_bd_addr_t bd_addr, esp_ble_sec_act_t sec_act)
{
    btc_msg_t msg = {0};
    btc_ble_gap_args_t arg;

    ESP_BLUEDROID_STATUS_CHECK(ESP_BLUEDROID_STATUS_ENABLED);

    if (bd_addr == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_GAP_BLE;
    msg.act = BTC_GAP_BLE_SET_ENCRYPTION_EVT;
    arg.set_encryption.sec_act = sec_act;
    memcpy(arg.set_encryption.bd_addr, bd_addr, ESP_BD_ADDR_LEN);

    return (btc_transfer_context(&msg, &arg, sizeof(btc_ble_gap_args_t), NULL, NULL)
            == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}

esp_err_t esp_ble_gap_security_rsp(esp_bd_addr_t bd_addr, bool accept)
{
    btc_msg_t msg = {0};
    btc_ble_gap_args_t arg;

    ESP_BLUEDROID_STATUS_CHECK(ESP_BLUEDROID_STATUS_ENABLED);

    if (bd_addr == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_GAP_BLE;
    msg.act = BTC_GAP_BLE_SECURITY_RSP_EVT;
    arg.sec_rsp.accept = accept;
    memcpy(arg.sec_rsp.bd_addr, bd_addr, ESP_BD_ADDR_LEN);

    return (btc_transfer_context(&msg, &arg, sizeof(btc_ble_gap_args_t), NULL, NULL)
            == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);

}

esp_err_t esp_ble_passkey_reply(esp_bd_addr_t bd_addr, bool accept, uint32_t passkey)
{
    btc_msg_t msg = {0};
    btc_ble_gap_args_t arg;

    ESP_BLUEDROID_STATUS_CHECK(ESP_BLUEDROID_STATUS_ENABLED);

    if (bd_addr == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_GAP_BLE;
    msg.act = BTC_GAP_BLE_PASSKEY_REPLY_EVT;
    arg.enc_passkey_replay.accept = accept;
    arg.enc_passkey_replay.passkey = passkey;
    memcpy(arg.enc_passkey_replay.bd_addr, bd_addr, ESP_BD_ADDR_LEN);

    return (btc_transfer_context(&msg, &arg, sizeof(btc_ble_gap_args_t), NULL, NULL)
            == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}

esp_err_t esp_ble_confirm_reply(esp_bd_addr_t bd_addr, bool accept)
{
    btc_msg_t msg = {0};
    btc_ble_gap_args_t arg;

    ESP_BLUEDROID_STATUS_CHECK(ESP_BLUEDROID_STATUS_ENABLED);

    if (bd_addr == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_GAP_BLE;
    msg.act = BTC_GAP_BLE_CONFIRM_REPLY_EVT;
    arg.enc_comfirm_replay.accept = accept;
    memcpy(arg.enc_comfirm_replay.bd_addr, bd_addr, ESP_BD_ADDR_LEN);

    return (btc_transfer_context(&msg, &arg, sizeof(btc_ble_gap_args_t), NULL, NULL)
            == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}

esp_err_t esp_ble_remove_bond_device(esp_bd_addr_t bd_addr)
{
    btc_msg_t msg = {0};
    btc_ble_gap_args_t arg;

    if (bd_addr == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_GAP_BLE;
    msg.act = BTC_GAP_BLE_REMOVE_BOND_DEV_EVT;
    memcpy(arg.remove_bond_device.bd_addr, bd_addr, ESP_BD_ADDR_LEN);

    return (btc_transfer_context(&msg, &arg, sizeof(btc_ble_gap_args_t), NULL, NULL)
            == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}

int esp_ble_get_bond_device_num(void)
{
    if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
        return ESP_FAIL;
    }

    return btc_storage_get_num_ble_bond_devices();
}

esp_err_t esp_ble_get_bond_device_list(int *dev_num, esp_ble_bond_dev_t *dev_list)
{
    int ret;
    int dev_num_total;

    if (dev_num == NULL || dev_list == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_BLUEDROID_STATUS_CHECK(ESP_BLUEDROID_STATUS_ENABLED);

    dev_num_total = btc_storage_get_num_ble_bond_devices();
    if (*dev_num > dev_num_total) {
        *dev_num = dev_num_total;
    }

    ret = btc_storage_get_bonded_ble_devices_list(dev_list, *dev_num);

    return (ret == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}

esp_err_t esp_ble_oob_req_reply(esp_bd_addr_t bd_addr, uint8_t *TK, uint8_t len)
{
    if(len != ESP_BT_OCTET16_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    if (bd_addr == NULL || TK == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    btc_msg_t msg = {0};
    btc_ble_gap_args_t arg;

    ESP_BLUEDROID_STATUS_CHECK(ESP_BLUEDROID_STATUS_ENABLED);

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_GAP_BLE;
    msg.act = BTC_GAP_BLE_OOB_REQ_REPLY_EVT;
    memcpy(arg.oob_req_reply.bd_addr, bd_addr, ESP_BD_ADDR_LEN);
    arg.oob_req_reply.len = len;
    arg.oob_req_reply.p_value = TK;

    return (btc_transfer_context(&msg, &arg, sizeof(btc_ble_gap_args_t), btc_gap_ble_arg_deep_copy,
                btc_gap_ble_arg_deep_free) == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}

esp_err_t esp_ble_sc_oob_req_reply(esp_bd_addr_t bd_addr, uint8_t p_c[16], uint8_t p_r[16])
{
    if (!bd_addr || !p_c || !p_r) {
        return ESP_ERR_INVALID_ARG;
    }

    btc_msg_t msg = {0};
    btc_ble_gap_args_t arg;

    ESP_BLUEDROID_STATUS_CHECK(ESP_BLUEDROID_STATUS_ENABLED);

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_GAP_BLE;
    msg.act = BTC_GAP_BLE_SC_OOB_REQ_REPLY_EVT;
    memcpy(arg.sc_oob_req_reply.bd_addr, bd_addr, ESP_BD_ADDR_LEN);
    arg.sc_oob_req_reply.p_c = p_c;
    arg.sc_oob_req_reply.p_r = p_r;

    return (btc_transfer_context(&msg, &arg, sizeof(btc_ble_gap_args_t), btc_gap_ble_arg_deep_copy,
                btc_gap_ble_arg_deep_free) == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}

esp_err_t esp_ble_create_sc_oob_data(void)
{
    btc_msg_t msg = {0};

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_GAP_BLE;
    msg.act = BTC_GAP_BLE_SC_CR_OOB_DATA_EVT;

    return (btc_transfer_context(&msg, NULL, 0, NULL, NULL) == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}

esp_err_t esp_ble_gap_get_local_irk(uint8_t local_irk[16])
{
    if (local_irk == NULL) {
        ESP_LOGE(__func__, "local_irk is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
        ESP_LOGE(__func__, "Bluedroid is not enabled");
        return ESP_ERR_INVALID_STATE;
    }

    /* Use BTM API to safely retrieve local IRK */
    if (BTM_GetLocalIRK(local_irk)) {
        ESP_LOGD(__func__, "Local IRK retrieved successfully");
        return ESP_OK;
    } else {
        ESP_LOGW(__func__, "Local IRK not available");
        return ESP_ERR_INVALID_STATE;
    }
}

esp_err_t esp_ble_gap_disconnect(esp_bd_addr_t remote_device)
{
    btc_msg_t msg = {0};
    btc_ble_gap_args_t arg;

    ESP_BLUEDROID_STATUS_CHECK(ESP_BLUEDROID_STATUS_ENABLED);

    if (remote_device == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_GAP_BLE;
    msg.act = BTC_GAP_BLE_DISCONNECT_EVT;
    memcpy(arg.disconnect.remote_device, remote_device, ESP_BD_ADDR_LEN);

    return (btc_transfer_context(&msg, &arg, sizeof(btc_ble_gap_args_t), NULL, NULL) == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}

esp_err_t esp_ble_get_current_conn_params(esp_bd_addr_t bd_addr, esp_gap_conn_params_t *conn_params)
{
    if(!bd_addr || !conn_params) {
        return ESP_ERR_INVALID_ARG;
    }
    if(BTM_GetCurrentConnParams(bd_addr, &conn_params->interval, &conn_params->latency, &conn_params->timeout)) {
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t esp_gap_ble_set_channels(esp_gap_ble_channels channels)
{
    btc_msg_t msg = {0};
    btc_ble_gap_args_t arg;

    if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
        return ESP_ERR_INVALID_STATE;
    }

    if (channels == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_GAP_BLE;
    msg.act = BTC_GAP_BLE_SET_AFH_CHANNELS;

    memcpy(&arg.set_channels.channels, channels, ESP_GAP_BLE_CHANNELS_LEN);
    arg.set_channels.channels[ESP_GAP_BLE_CHANNELS_LEN -1] &= 0x1F;
    return (btc_transfer_context(&msg, &arg, sizeof(btc_ble_gap_args_t), NULL, NULL) == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}

esp_err_t esp_ble_gap_read_channel_map(esp_bd_addr_t bd_addr)
{
    if (!bd_addr) {
        return ESP_ERR_INVALID_ARG;
    }

    btc_msg_t msg;
    btc_ble_gap_args_t arg;

    ESP_BLUEDROID_STATUS_CHECK(ESP_BLUEDROID_STATUS_ENABLED);

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_GAP_BLE;
    msg.act = BTC_GAP_BLE_READ_CHANNEL_MAP;

    memcpy(arg.read_channel_map.bd_addr, bd_addr, ESP_BD_ADDR_LEN);
    return (btc_transfer_context(&msg, &arg, sizeof(btc_ble_gap_args_t), NULL, NULL)
            == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}

esp_err_t esp_gap_ble_set_authorization(esp_bd_addr_t bd_addr, bool authorize)
{
    if (!bd_addr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (BTM_Ble_Authorization(bd_addr, authorize)) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t esp_ble_gap_set_privacy_mode(esp_ble_addr_type_t addr_type, esp_bd_addr_t addr, esp_ble_privacy_mode_t mode)
{
    btc_msg_t msg;
    btc_ble_gap_args_t arg;

    ESP_BLUEDROID_STATUS_CHECK(ESP_BLUEDROID_STATUS_ENABLED);

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_GAP_BLE;
    msg.act = BTC_GAP_BLE_SET_PRIVACY_MODE;

    arg.set_privacy_mode.addr_type = addr_type;
    memcpy(arg.set_privacy_mode.addr, addr, sizeof(esp_bd_addr_t));
    arg.set_privacy_mode.privacy_mode = mode;

    return (btc_transfer_context(&msg, &arg, sizeof(btc_ble_gap_args_t), NULL, NULL)
            == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}

