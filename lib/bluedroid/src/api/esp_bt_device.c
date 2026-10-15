/*
 * SPDX-FileCopyrightText: 2015-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "device/controller.h"
#include "btc/btc_task.h"
#include "btc/btc_dev.h"
#include "btc/btc_config.h"
#include "btc/btc_manage.h"

esp_err_t esp_bt_dev_register_callback(esp_bt_dev_cb_t callback)
{
    if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
        return ESP_ERR_INVALID_STATE;
    }

    if (callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    btc_profile_cb_set(BTC_PID_DEV, callback);
    return ESP_OK;
}

const uint8_t *esp_bt_dev_get_address(void)
{
    if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
	return NULL;
    }
    return controller_get_interface()->get_address()->address;
}

esp_err_t esp_bt_config_file_path_get(char *file_path)
{
    return btc_config_file_path_get(file_path);
}

esp_err_t esp_bt_config_file_path_update(const char *file_path)
{
    ESP_BLUEDROID_STATUS_CHECK(ESP_BLUEDROID_STATUS_UNINITIALIZED);

    return btc_config_file_path_update(file_path);
}
