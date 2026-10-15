/*
 * SPDX-FileCopyrightText: 2015-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include "common/bt_target.h"
#include "esp_bt_main.h"
#include "btc/btc_task.h"
#include "btc/btc_main.h"
#include "osi/future.h"
#include "osi/allocator.h"
#include "config/stack_config.h"
#include "bt_common.h"
#include "btc_av.h"

static esp_bluedroid_status_t s_bt_host_state = ESP_BLUEDROID_STATUS_UNINITIALIZED;

esp_bluedroid_status_t esp_bluedroid_get_status(void)
{
    return s_bt_host_state;
}

esp_err_t esp_bluedroid_enable(void)
{
    btc_msg_t msg;
    future_t **future_p;

    if (s_bt_host_state == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
        LOG_ERROR("Bludroid not initialised\n");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_bt_host_state == ESP_BLUEDROID_STATUS_ENABLED) {
        LOG_ERROR("Bluedroid already enabled\n");
        return ESP_ERR_INVALID_STATE;
    }

    future_p = btc_main_get_future_p(BTC_MAIN_ENABLE_FUTURE);
    *future_p = future_new();
    if (*future_p == NULL) {
        LOG_ERROR("Bluedroid enable failed\n");
        return ESP_ERR_NO_MEM;
    }

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_MAIN_INIT;
    msg.act = BTC_MAIN_ACT_ENABLE;

    if (btc_transfer_context(&msg, NULL, 0, NULL, NULL) != BT_STATUS_SUCCESS) {
        LOG_ERROR("Bluedroid enable failed\n");
        return ESP_FAIL;
    }

    if (future_await(*future_p) == FUTURE_FAIL) {
        LOG_ERROR("Bluedroid enable failed\n");
        return ESP_FAIL;
    }

    s_bt_host_state = ESP_BLUEDROID_STATUS_ENABLED;
    return ESP_OK;
}

esp_err_t esp_bluedroid_disable(void)
{
    btc_msg_t msg;
    future_t **future_p;

    if (s_bt_host_state != ESP_BLUEDROID_STATUS_ENABLED) {
        LOG_ERROR("Bluedroid already disabled\n");
        return ESP_ERR_INVALID_STATE;
    }

    s_bt_host_state = ESP_BLUEDROID_STATUS_DISABLING;

    future_p = btc_main_get_future_p(BTC_MAIN_DISABLE_FUTURE);
    *future_p = future_new();
    if (*future_p == NULL) {
        LOG_ERROR("Bluedroid disable failed\n");
        s_bt_host_state = ESP_BLUEDROID_STATUS_ENABLED;
        return ESP_ERR_NO_MEM;
    }

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_MAIN_INIT;
    msg.act = BTC_MAIN_ACT_DISABLE;

    if (btc_transfer_context(&msg, NULL, 0, NULL, NULL) != BT_STATUS_SUCCESS) {
        LOG_ERROR("Bluedroid disable failed\n");
        s_bt_host_state = ESP_BLUEDROID_STATUS_ENABLED;
        return ESP_FAIL;
    }

    if (future_await(*future_p) == FUTURE_FAIL) {
        LOG_ERROR("Bluedroid disable failed\n");
        s_bt_host_state = ESP_BLUEDROID_STATUS_ENABLED;
        return ESP_FAIL;
    }

    s_bt_host_state = ESP_BLUEDROID_STATUS_INITIALIZED;
    return ESP_OK;
}

esp_err_t esp_bluedroid_init(void)
{
    esp_bluedroid_config_t cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    return esp_bluedroid_init_with_cfg(&cfg);
}

esp_err_t esp_bluedroid_init_with_cfg(esp_bluedroid_config_t *cfg)
{
    btc_msg_t msg;
    future_t **future_p;
    bt_status_t ret;

    if (!cfg) {
        LOG_ERROR("%s cfg is NULL", __func__);
        return ESP_ERR_INVALID_ARG;
    }

    if (cfg->sc_en) {

        if (!cfg->ssp_en) {
            LOG_ERROR("secure simple pairing should be enabled when secure connection host support is enabled\n");
            return ESP_ERR_INVALID_ARG;
        }

        LOG_WARN("Please make sure to clear the bond list before enabling the secure connection host support\n");
    }

    if (s_bt_host_state != ESP_BLUEDROID_STATUS_UNINITIALIZED) {
        LOG_ERROR("Bluedroid already initialised\n");
        return ESP_ERR_INVALID_STATE;
    }

    ret = bluedroid_config_init(cfg);
    if (ret != BT_STATUS_SUCCESS) {
        LOG_ERROR("Bluedroid stack initialize fail, ret:%d", ret);
        return ESP_FAIL;
    }

    /* A freshly started stack can't have A2DP up */
    btc_av_reset_stale_state();

    /*
     * BTC Init
     */
    ret = btc_init();
    if (ret != BT_STATUS_SUCCESS) {
        LOG_ERROR("Bluedroid Initialize Fail");
        return ESP_FAIL;
    }

    future_p = btc_main_get_future_p(BTC_MAIN_INIT_FUTURE);
    *future_p = future_new();
    if (*future_p == NULL) {
        LOG_ERROR("Bluedroid Initialize Fail!");
        return ESP_ERR_NO_MEM;
    }

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_MAIN_INIT;
    msg.act = BTC_MAIN_ACT_INIT;

    if (btc_transfer_context(&msg, NULL, 0, NULL, NULL) != BT_STATUS_SUCCESS) {
        LOG_ERROR("Bluedroid Initialize Fail");
        return ESP_FAIL;
    }

    if (future_await(*future_p) == FUTURE_FAIL) {
        LOG_ERROR("Bluedroid Initialize Fail");
        return ESP_FAIL;
    }

    s_bt_host_state = ESP_BLUEDROID_STATUS_INITIALIZED;

    return ESP_OK;
}

esp_err_t esp_bluedroid_deinit(void)
{
    btc_msg_t msg;
    future_t **future_p;

    if (s_bt_host_state == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
        LOG_ERROR("Bluedroid already de-initialised\n");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_bt_host_state == ESP_BLUEDROID_STATUS_ENABLED ||
        s_bt_host_state == ESP_BLUEDROID_STATUS_DISABLING) {
        LOG_ERROR("Bludroid still enabled or stopping, disable first\n");
        return ESP_ERR_INVALID_STATE;
    }

    future_p = btc_main_get_future_p(BTC_MAIN_DEINIT_FUTURE);
    *future_p = future_new();
    if (*future_p == NULL) {
        LOG_ERROR("Bluedroid de-initialise failed\n");
        return ESP_ERR_NO_MEM;
    }

    msg.sig = BTC_SIG_API_CALL;
    msg.pid = BTC_PID_MAIN_INIT;
    msg.act = BTC_MAIN_ACT_DEINIT;

    if (btc_transfer_context(&msg, NULL, 0, NULL, NULL) != BT_STATUS_SUCCESS) {
        LOG_ERROR("Bluedroid de-initialise failed\n");
        return ESP_FAIL;
    }

    if (future_await(*future_p) == FUTURE_FAIL) {
        LOG_ERROR("Bluedroid de-initialise failed\n");
        return ESP_FAIL;
    }

    btc_deinit();

    bluedroid_config_deinit();

    s_bt_host_state = ESP_BLUEDROID_STATUS_UNINITIALIZED;
    return ESP_OK;
}

