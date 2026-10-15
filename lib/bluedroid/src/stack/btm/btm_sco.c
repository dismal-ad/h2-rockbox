/******************************************************************************
 *
 *  Copyright (C) 2000-2012 Broadcom Corporation
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
 *  This file contains functions that handle SCO connections. This includes
 *  operations such as connect, disconnect, change supported packet types.
 *
 ******************************************************************************/

#include <string.h>
#include "stack/bt_types.h"
#include "common/bt_target.h"
#include "stack/bt_types.h"
#include "stack/hcimsgs.h"
#include "stack/btu.h"
#include "stack/btm_api.h"
#include "osi/allocator.h"
#include "btm_int.h"
#include "stack/hcidefs.h"
//#include "bt_utils.h"

tBTM_STATUS BTM_CreateSco (BD_ADDR remote_bda, BOOLEAN is_orig,
                           UINT16 pkt_types, UINT16 *p_sco_inx,
                           tBTM_SCO_CB *p_conn_cb,
                           tBTM_SCO_CB *p_disc_cb)
{
    return (BTM_NO_RESOURCES);
}
tBTM_STATUS BTM_RemoveSco (UINT16 sco_inx)
{
    return (BTM_NO_RESOURCES);
}
tBTM_STATUS BTM_SetScoPacketTypes (UINT16 sco_inx, UINT16 pkt_types)
{
    return (BTM_NO_RESOURCES);
}
UINT16 BTM_ReadScoPacketTypes (UINT16 sco_inx)
{
    return (0);
}
UINT16 BTM_ReadDeviceScoPacketTypes (void)
{
    return (0);
}
UINT16 BTM_ReadScoHandle (UINT16 sco_inx)
{
    return (BTM_INVALID_HCI_HANDLE);
}
UINT8 *BTM_ReadScoBdAddr(UINT16 sco_inx)
{
    return ((UINT8 *) NULL);
}
tBTM_STATUS BTM_SetEScoMode (tBTM_SCO_TYPE sco_mode, tBTM_ESCO_PARAMS *p_parms)
{
    return (BTM_MODE_UNSUPPORTED);
}
tBTM_STATUS BTM_RegForEScoEvts (UINT16 sco_inx, tBTM_ESCO_CBACK *p_esco_cback)
{
    return (BTM_ILLEGAL_VALUE);
}
tBTM_STATUS BTM_ReadEScoLinkParms (UINT16 sco_inx, tBTM_ESCO_DATA *p_parms)
{
    return (BTM_MODE_UNSUPPORTED);
}
tBTM_STATUS BTM_ChangeEScoLinkParms (UINT16 sco_inx, tBTM_CHG_ESCO_PARAMS *p_parms)
{
    return (BTM_MODE_UNSUPPORTED);
}
void BTM_EScoConnRsp (UINT16 sco_inx, UINT8 hci_status, tBTM_ESCO_PARAMS *p_parms) {}
UINT8 BTM_GetNumScoLinks (void)
{
    return (0);
}

