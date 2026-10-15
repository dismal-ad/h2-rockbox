/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_ble_iso_api.h"
#include "btc_iso_ble.h"
#include "btc/btc_manage.h"


esp_err_t esp_ble_iso_register_callback(esp_ble_iso_cb_t callback)
{
    ESP_BLUEDROID_STATUS_CHECK(ESP_BLUEDROID_STATUS_ENABLED);

    return (btc_profile_cb_set(BTC_PID_ISO_BLE, callback) == 0 ? ESP_OK : ESP_FAIL);
}

esp_ble_iso_cb_t esp_ble_iso_get_callback(void)
{
    return (esp_ble_iso_cb_t) btc_profile_cb_get(BTC_PID_ISO_BLE);
}

esp_err_t esp_ble_iso_set_iso_data_path(esp_ble_iso_set_data_path_params_t *data_path_params)
{
    btc_msg_t msg = {0};
    btc_ble_iso_args_t arg;

    if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
        return ESP_ERR_INVALID_STATE;
    }

    if (data_path_params == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (data_path_params->codec_cfg_len && (!data_path_params->codec_cfg)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (data_path_params->codec_cfg && (!data_path_params->codec_cfg_len)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (data_path_params->data_path_dir > ESP_BLE_ISO_DATA_PATH_DIR_OUTPUT) {
        return ESP_ERR_INVALID_ARG;
    }

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_ISO_BLE;
    msg.act = BTC_ISO_ACT_SET_DATA_PATH;

    arg.iso_set_data_path_params.conn_handle = data_path_params->iso_handle;
    arg.iso_set_data_path_params.data_path_dir = data_path_params->data_path_dir;
    arg.iso_set_data_path_params.data_path_id = data_path_params->data_path_id;
    arg.iso_set_data_path_params.coding_fmt = data_path_params->coding_fmt;
    arg.iso_set_data_path_params.company_id = data_path_params->company_id;
    arg.iso_set_data_path_params.vs_codec_id = data_path_params->vs_codec_id;
    arg.iso_set_data_path_params.controller_delay = data_path_params->controller_delay;
    arg.iso_set_data_path_params.codec_cfg_len = data_path_params->codec_cfg_len;
    arg.iso_set_data_path_params.codec_cfg = data_path_params->codec_cfg;

    return (btc_transfer_context(&msg, &arg, sizeof(btc_ble_iso_args_t), btc_iso_ble_arg_deep_copy, btc_iso_ble_arg_deep_free)
                == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}

esp_err_t esp_ble_iso_remove_iso_data_path(esp_ble_iso_remove_data_path_params_t *data_path_params)
{
    btc_msg_t msg = {0};
    btc_ble_iso_args_t arg;

    if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
        return ESP_ERR_INVALID_STATE;
    }

    if (data_path_params == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (data_path_params->data_path_dir == 0 || data_path_params->data_path_dir > ESP_BLE_ISO_DATA_PATH_DIR_ALL_BIT_MASK) {
        return ESP_ERR_INVALID_ARG;
    }

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_ISO_BLE;
    msg.act = BTC_ISO_ACT_REMOVE_DATA_PATH;

    arg.iso_remove_data_path_params.conn_handle = data_path_params->iso_handle;
    arg.iso_remove_data_path_params.data_path_dir = data_path_params->data_path_dir;

    return (btc_transfer_context(&msg, &arg, sizeof(btc_ble_iso_args_t), NULL, NULL)
                == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}

esp_err_t esp_ble_iso_read_iso_tx_sync(uint16_t iso_handle)
{
    btc_msg_t msg = {0};
    btc_ble_iso_args_t arg;

    if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
        return ESP_ERR_INVALID_STATE;
    }

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_ISO_BLE;
    msg.act = BTC_ISO_ACT_READ_ISO_TX_SYNC;

    arg.iso_read_tx_sync_params.iso_handle = iso_handle;

    return (btc_transfer_context(&msg, &arg, sizeof(btc_ble_iso_args_t), NULL, NULL) == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}

esp_err_t esp_ble_iso_read_link_quality(uint16_t iso_handle)
{
    btc_msg_t msg = {0};
    btc_ble_iso_args_t arg;

    if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
        return ESP_ERR_INVALID_STATE;
    }

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_ISO_BLE;
    msg.act = BTC_ISO_ACT_READ_ISO_LINK_QUALITY;

    arg.iso_read_link_quality_params.iso_handle = iso_handle;

    return (btc_transfer_context(&msg, &arg, sizeof(btc_ble_iso_args_t), NULL, NULL) == BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL);
}

