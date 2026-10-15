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

tBTM_BLE_CTE_CBACK ble_cte_cb;

extern void btm_ble_inter_set(bool extble_inter);

void BTM_BleCteRegisterCallback(tBTM_BLE_CTE_CBACK cb)
{
    if (cb) {
        ble_cte_cb = cb;
    } else {
        BTM_TRACE_ERROR("%s, register fail, the cb function is NULL.", __func__);
    }
}

void BTM_CteBleCallbackTrigger(tBTM_BLE_5_GAP_EVENT event, tBTM_BLE_CTE_CB_PARAMS *params)
{
    BTM_TRACE_DEBUG("%s event %x", __func__, event);

    if(params && params->status == BTM_SUCCESS) {
        btm_ble_inter_set(true);
    }
    if (ble_cte_cb) {
        ble_cte_cb(event, params);
    }
}

tBTM_STATUS BTM_BleCteReadAntInfor(void)
{
    tHCI_STATUS err = HCI_SUCCESS;
    tBTM_STATUS status = BTM_SUCCESS;

    if ((err = btsnd_hcic_ble_read_antenna_info()) != HCI_SUCCESS) {
        BTM_TRACE_ERROR("cte read ant information, cmd err=0x%x", err);
        status = BTM_HCI_ERROR | err;
    }

    return status;
}

void btm_ble_cte_read_ant_infor_complete(UINT8 *p)
{
    tBTM_BLE_CTE_CB_PARAMS cb_params = {0};

    STREAM_TO_UINT8(cb_params.cte_read_ant_infor_cmpl.status, p);
    STREAM_TO_UINT8(cb_params.cte_read_ant_infor_cmpl.supported_switching_sampling_rates, p);
    STREAM_TO_UINT8(cb_params.cte_read_ant_infor_cmpl.num_ant, p);
    STREAM_TO_UINT8(cb_params.cte_read_ant_infor_cmpl.max_switching_pattern_len, p);
    STREAM_TO_UINT8(cb_params.cte_read_ant_infor_cmpl.max_cte_len, p);

    if (cb_params.cte_read_ant_infor_cmpl.status != HCI_SUCCESS) {
        cb_params.cte_read_ant_infor_cmpl.status = (BTM_HCI_ERROR | cb_params.cte_read_ant_infor_cmpl.status);
    }

    BTM_CteBleCallbackTrigger(BTM_BLE_CTE_READ_ANT_INFOR_EVT, &cb_params);
}

