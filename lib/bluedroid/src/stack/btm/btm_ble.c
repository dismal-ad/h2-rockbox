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

/******************************************************************************
 *
 *  This file contains functions for BLE device control utilities, and LE
 *  security functions.
 *
 ******************************************************************************/
#include "common/bt_target.h"

#include <string.h>

#include "stack/bt_types.h"
#include "stack/hcimsgs.h"
#include "stack/btu.h"
#include "btm_int.h"
#include "stack/btm_ble_api.h"
#include "stack/smp_api.h"
#include "l2c_int.h"
#include "stack/gap_api.h"
//#include "bt_utils.h"
#include "device/controller.h"

//#define LOG_TAG "bt_btm_ble"
//#include "osi/include/log.h"

/*******************************************************************************
**
** Function         BTM_ReadConnectedTransportAddress
**
** Description      This function is called to read the paired device/address type of other device paired
**                  corresponding to the BD_address
**
** Parameter        remote_bda: remote device address, carry out the transport address
**                  transport: active transport
**
** Return           TRUE if an active link is identified; FALSE otherwise
**
*******************************************************************************/
BOOLEAN BTM_ReadConnectedTransportAddress(BD_ADDR remote_bda, tBT_TRANSPORT transport)
{
    tBTM_SEC_DEV_REC *p_dev_rec = btm_find_dev(remote_bda);

    /* if no device can be located, return */
    if (p_dev_rec == NULL) {
        memset(remote_bda, 0, BD_ADDR_LEN);
        return FALSE;
    }

    if (transport == BT_TRANSPORT_BR_EDR) {
        if (btm_bda_to_acl(p_dev_rec->bd_addr, transport) != NULL) {
            memcpy(remote_bda, p_dev_rec->bd_addr, BD_ADDR_LEN);
            return TRUE;
        } else if (p_dev_rec->device_type & BT_DEVICE_TYPE_BREDR) {
            memcpy(remote_bda, p_dev_rec->bd_addr, BD_ADDR_LEN);
        } else {
            memset(remote_bda, 0, BD_ADDR_LEN);
        }
        return FALSE;
    }
    return FALSE;
}

/*******************************************************************************
**
** Function         btm_ble_get_enc_key_type
**
** Description      This function is to get the BLE key type that has been exchanged
**                  in between local device and peer device.
**
** Returns          p_key_type: output parameter to carry the key type value.
**
*******************************************************************************/

