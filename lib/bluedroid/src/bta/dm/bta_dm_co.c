/******************************************************************************
 *
 *  Copyright (C) 2009-2012 Broadcom Corporation
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
#include <stdio.h>
#include <stdlib.h>

#include "bta/bta_api.h"
#include "bta/bta_sys.h"
#include "bta/bta_dm_co.h"
#include "bta/bta_dm_ci.h"
#include "btc/btc_dm.h"

#include "btm_int.h"

/*******************************************************************************
**
** Function         bta_dm_co_get_compress_memory
**
** Description      This callout function is executed by DM to get memory for compression

** Parameters       id  -  BTA SYS ID
**                  memory_p - memory return by callout
**                  memory_size - memory size
**
** Returns          TRUE for success, FALSE for fail.
**
*******************************************************************************/
BOOLEAN bta_dm_co_get_compress_memory(tBTA_SYS_ID id, UINT8 **memory_p, UINT32 *memory_size)
{
    UNUSED(id);
    UNUSED(memory_p);
    UNUSED(memory_size);
    return TRUE;
}

/*******************************************************************************
**
** Function         bta_dm_co_bt_set_io_cap
**
** Description      This function is used to set IO capabilities
**
** Parameters       bt_io_cap  - IO capabilities
**
** @return          - ESP_BT_STATUS_SUCCESS : success
**                  - other  : failed
**
*******************************************************************************/
esp_err_t bta_dm_co_bt_set_io_cap(UINT8 bt_io_cap)
{
    esp_err_t ret = ESP_BT_STATUS_SUCCESS;

    if(bt_io_cap < BTM_IO_CAP_MAX ) {
        btm_cb.devcb.loc_io_caps = bt_io_cap;
        ret = ESP_BT_STATUS_SUCCESS;
    } else {
        ret = ESP_BT_STATUS_FAIL;
        APPL_TRACE_ERROR("%s error:Invalid io cap value.",__func__);
    }

    return ret;
}

/*******************************************************************************
**
** Function         bta_dm_co_io_req
**
** Description      This callout function is executed by DM to get IO capabilities
**                  of the local device for the Simple Pairing process
**
** Parameters       bd_addr  - The peer device
**                  *p_io_cap - The local Input/Output capabilities
**                  *p_oob_data - TRUE, if OOB data is available for the peer device.
**                  *p_auth_req - TRUE, if MITM protection is required.
**
** Returns          void.
**
*******************************************************************************/
void bta_dm_co_io_req(BD_ADDR bd_addr, tBTA_IO_CAP *p_io_cap, tBTA_OOB_DATA *p_oob_data,
                      tBTA_AUTH_REQ *p_auth_req, BOOLEAN is_orig)
{
    UNUSED(bd_addr);
    BTIF_TRACE_DEBUG("bta_dm_co_io_req: func not ported\n");
    BTIF_TRACE_DEBUG("bta_dm_co_io_req *p_oob_data = %d", *p_oob_data);
    BTIF_TRACE_DEBUG("bta_dm_co_io_req *p_io_cap = %d", *p_io_cap);
    BTIF_TRACE_DEBUG("bta_dm_co_io_req *p_auth_req = %d", *p_auth_req);
    BTIF_TRACE_DEBUG("bta_dm_co_io_req is_orig = %d", is_orig);
}

/*******************************************************************************
**
** Function         bta_dm_co_io_rsp
**
** Description      This callout function is executed by DM to report IO capabilities
**                  of the peer device for the Simple Pairing process
**
** Parameters       bd_addr  - The peer device
**                  io_cap - The remote Input/Output capabilities
**                  oob_data - TRUE, if OOB data is available for the peer device.
**                  auth_req - TRUE, if MITM protection is required.
**
** Returns          void.
**
*******************************************************************************/
void bta_dm_co_io_rsp(BD_ADDR bd_addr, tBTA_IO_CAP io_cap,
                      tBTA_OOB_DATA oob_data, tBTA_AUTH_REQ auth_req)
{
    BTIF_TRACE_DEBUG("bta_dm_co_io_rsp: func not ported\n");
}

/*******************************************************************************
**
** Function         bta_dm_co_lk_upgrade
**
** Description      This callout function is executed by DM to check if the
**                  platform wants allow link key upgrade
**
** Parameters       bd_addr  - The peer device
**                  *p_upgrade - TRUE, if link key upgrade is desired.
**
** Returns          void.
**
*******************************************************************************/
void  bta_dm_co_lk_upgrade(BD_ADDR bd_addr, BOOLEAN *p_upgrade )
{
    UNUSED(bd_addr);
    UNUSED(p_upgrade);
}

/*******************************************************************************
**
** Function         bta_dm_co_loc_oob
**
** Description      This callout function is executed by DM to report the OOB
**                  data of the local device for the Simple Pairing process
**
** Parameters       valid - TRUE, if the local OOB data is retrieved from LM
**                  c     - Simple Pairing Hash C
**                  r     - Simple Pairing Randomnizer R
**
** Returns          void.
**
*******************************************************************************/
void bta_dm_co_loc_oob(BOOLEAN valid, BT_OCTET16 c, BT_OCTET16 r)
{
    BTIF_TRACE_DEBUG("bta_dm_co_loc_oob: func not ported\n");
}

/*******************************************************************************
**
** Function         bta_dm_co_rmt_oob
**
** Description      This callout function is executed by DM to request the OOB
**                  data for the remote device for the Simple Pairing process
**                  Need to call bta_dm_ci_rmt_oob() in response
**
** Parameters       bd_addr  - The peer device
**
** Returns          void.
**
*******************************************************************************/
void bta_dm_co_rmt_oob(BD_ADDR bd_addr)
{
    BT_OCTET16 p_c = {0};
    BT_OCTET16 p_r = {0};
    BOOLEAN result = FALSE;

#ifdef BTIF_DM_OOB_TEST
    BTIF_TRACE_DEBUG("bta_dm_rmt_oob: func not ported\n");
#endif

    BTIF_TRACE_DEBUG("bta_dm_co_rmt_oob: result=%d", result);
    bta_dm_ci_rmt_oob(result, bd_addr, p_c, p_r);
}


// REMOVE FOR BLUEDROID ?

