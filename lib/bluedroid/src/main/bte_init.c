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
 *  This module contains the routines that initialize the stack components.
 *  It must be called before the BTU task is started.
 *
 ******************************************************************************/

#include "common/bt_target.h"
#include <string.h>


/* Stack Configuration Related Init Definaton
 * TODO: Now Just Unmask these definition until stack layer is OK
 */

#ifndef BTA_INCLUDED
#define BTA_INCLUDED FALSE
#endif

// Include initialization functions definitions
#include "stack/port_api.h"

#include "stack/gap_api.h"

#include "stack/avrc_api.h"

#include "stack/a2d_api.h"

#include "avdt_int.h"

#include "avct_int.h"

#include "stack/obex_api.h"

#include "stack/goep_common.h"
#include "stack/goepc_api.h"

//BTA Modules


/*****************************************************************************
**                          F U N C T I O N S                                *
******************************************************************************/

/*****************************************************************************
**
** Function         BTE_DeinitStack
**
** Description      Deinitialize control block memory for each component.
**
**                  Note: This API must be called
**                      after freeing the BTU Task.
**
** Returns          void
**
******************************************************************************/
void BTE_DeinitStack(void)
{
    //BTA Modules

    GOEPC_Deinit();

    OBEX_Deinit();

    GAP_Deinit();

    AVRC_Deinit();

    A2D_Deinit();

    RFCOMM_Deinit();
}

/*****************************************************************************
**
** Function         BTE_InitStack
**
** Description      Initialize control block memory for each component.
**
**                  Note: The core stack components must be called
**                      before creating the BTU Task.  The rest of the
**                      components can be initialized at a later time if desired
**                      as long as the component's init function is called
**                      before accessing any of its functions.
**
** Returns          status
**
******************************************************************************/
bt_status_t BTE_InitStack(void)
{
    //Initialize the optional stack components
    if (RFCOMM_Init() != BT_STATUS_SUCCESS) {
        goto error_exit;
    }

    //BNEP and its profiles

    //AVDT and its profiles
    if (A2D_Init() != BT_STATUS_SUCCESS) {
        goto error_exit;
    }

    if (AVRC_Init() != BT_STATUS_SUCCESS) {
        goto error_exit;
    }

    if (GAP_Init() != BT_STATUS_SUCCESS) {
        goto error_exit;
    }

    if (OBEX_Init() != OBEX_SUCCESS) {
        goto error_exit;
    }

    if (GOEPC_Init() != GOEP_SUCCESS) {
        goto error_exit;
    }

    //BTA Modules
    return BT_STATUS_SUCCESS;

error_exit:;
    LOG_ERROR("%s failed due to no memory", __func__);
    BTE_DeinitStack();
    return BT_STATUS_NOMEM;
}
