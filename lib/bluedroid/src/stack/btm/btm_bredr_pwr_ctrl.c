/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/******************************************************************************
 *
 *  This file contains functions for BR/EDR power control.
 *
 ******************************************************************************/

#include <string.h>

#include "stack/bt_types.h"
#include "common/bt_target.h"
#include "stack/btu.h"
#include "stack/btm_api.h"
#include "btm_int.h"
#include "stack/hcidefs.h"

#define BTM_BREDR_PWR_CTRL_REPLY_TIMEOUT   3       /* 3 second timeout waiting for responses */

tBTM_STATUS BTM_ReadBredrTxPwrLvl(tBTM_TX_PWR_LVL_TYPE type, tBTM_CMPL_CB *p_cb)
{

    switch (type) {
    case BTM_TX_PWR_LVL_ISCAN:
        // standard HCI
        return BTM_ReadInquiryRspTxPower(p_cb);
    default:
        BTM_TRACE_ERROR("BTM_ReadBredrTxPwrLvl invalid type: %d", type);
        return BTM_ILLEGAL_VALUE;
    }

}

tBTM_STATUS BTM_WriteBredrTxPwrLvl(tBTM_TX_PWR_LVL_TYPE type, INT8 tx_power, tBTM_CMPL_CB *p_cb)
{

    switch (type) {
    case BTM_TX_PWR_LVL_INQ:
        // standard HCI
        return BTM_WriteInquiryTxPower(tx_power, p_cb);
    default:
        BTM_TRACE_ERROR("BTM_WriteBredrTxPwrLvl invalid type: %d", type);
        return BTM_ILLEGAL_VALUE;
    }

}

/*******************************************************************************
**
** Function         btm_bredr_pwr_ctrl_timeout
**
** Description      This function processes a BR/EDR power control vsc timeout.
**
** Returns          void
**
*******************************************************************************/
void btm_bredr_pwr_ctrl_timeout(TIMER_LIST_ENT *p_tle)
{
    if (p_tle == &btm_cb.devcb.write_inq_txpwer_timer) {
        btm_write_inq_tx_power_complete(NULL);
    } else if (p_tle == &btm_cb.devcb.read_iscan_txpwer_timer) {
        btm_read_iscan_tx_power_complete(NULL);

    }
}

