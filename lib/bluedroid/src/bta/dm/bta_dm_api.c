/******************************************************************************
 *
 *  Copyright (C) 2003-2014 Broadcom Corporation
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
 *  This is the API implementation file for the BTA device manager.
 *
 ******************************************************************************/

#include "bta/bta_sys.h"
#include "bta/bta_api.h"
#include "bta_dm_int.h"
#include "bta_sys_int.h"
#include "stack/btm_api.h"
#include "btm_int.h"
#include <string.h>
#include <assert.h>
#include "bta/utl.h"
#include "osi/allocator.h"

/*****************************************************************************
**  Constants
*****************************************************************************/

static const tBTA_SYS_REG bta_dm_reg = {
    bta_dm_sm_execute,
    bta_dm_sm_disable
};

static const tBTA_SYS_REG bta_dm_search_reg = {
    bta_dm_search_sm_execute,
    bta_dm_search_sm_disable
};

/*******************************************************************************
**
** Function         BTA_EnableBluetooth
**
** Description      Enables bluetooth service.  This function must be
**                  called before any other functions in the BTA API are called.
**
**
** Returns          tBTA_STATUS
**
*******************************************************************************/
tBTA_STATUS BTA_EnableBluetooth(tBTA_DM_SEC_CBACK *p_cback)
{

    tBTA_DM_API_ENABLE    *p_msg;

    /* Bluetooth disabling is in progress */
    if (bta_dm_cb.disabling) {
        return BTA_FAILURE;
    }

    memset(&bta_dm_cb, 0, sizeof(bta_dm_cb));

    bta_sys_register (BTA_ID_DM, &bta_dm_reg );
    bta_sys_register (BTA_ID_DM_SEARCH, &bta_dm_search_reg );

    /* if UUID list is not provided as static data */
    bta_sys_eir_register(bta_dm_eir_update_uuid);

    if ((p_msg = (tBTA_DM_API_ENABLE *) osi_malloc(sizeof(tBTA_DM_API_ENABLE))) != NULL) {
        p_msg->hdr.event = BTA_DM_API_ENABLE_EVT;
        p_msg->p_sec_cback = p_cback;
        bta_sys_sendmsg(p_msg);
        return BTA_SUCCESS;
    }
    return BTA_FAILURE;

}

/*******************************************************************************
**
** Function         BTA_DisableBluetooth
**
** Description      Disables bluetooth service.  This function is called when
**                  the application no longer needs bluetooth service
**
** Returns          void
**
*******************************************************************************/
tBTA_STATUS BTA_DisableBluetooth(void)
{

    BT_HDR    *p_msg;

    if ((p_msg = (BT_HDR *) osi_malloc(sizeof(BT_HDR))) != NULL) {
        p_msg->event = BTA_DM_API_DISABLE_EVT;
        bta_sys_sendmsg(p_msg);
    } else {
        return BTA_FAILURE;
    }

    return BTA_SUCCESS;
}

/*******************************************************************************
**
** Function         BTA_DmSetDeviceName
**
** Description      This function sets the Bluetooth name of local device
**
**
** Returns          void
**
*******************************************************************************/
void BTA_DmSetDeviceName(const char *p_name, tBT_DEVICE_TYPE name_type)
{

    tBTA_DM_API_SET_NAME    *p_msg;

    if ((p_msg = (tBTA_DM_API_SET_NAME *) osi_malloc(sizeof(tBTA_DM_API_SET_NAME))) != NULL) {
        p_msg->hdr.event = BTA_DM_API_SET_NAME_EVT;
        /* truncate the name if needed */
        BCM_STRNCPY_S((char *)p_msg->name, p_name, BD_NAME_LEN);
        p_msg->name[BD_NAME_LEN] = '\0';
        p_msg->name_type = name_type;

        bta_sys_sendmsg(p_msg);
    }
}

/*******************************************************************************
**
** Function         BTA_DmGetDeviceName
**
** Description      This function gets the Bluetooth name of local device
**
**
** Returns          void
**
*******************************************************************************/
void BTA_DmGetDeviceName(tBTA_GET_DEV_NAME_CBACK *p_cback, tBT_DEVICE_TYPE name_type)
{
    tBTA_DM_API_GET_NAME *p_msg;

    if ((p_msg = (tBTA_DM_API_GET_NAME *) osi_malloc(sizeof(tBTA_DM_API_GET_NAME))) != NULL) {
        p_msg->hdr.event = BTA_DM_API_GET_NAME_EVT;
        p_msg->p_cback = p_cback;
        p_msg->name_type = name_type;
        bta_sys_sendmsg(p_msg);
    }
}

/*******************************************************************************
**
** Function         BTA_DmCfgCoexStatus
**
** Description      This function configures the coexist status
**
**
** Returns          void
**
*******************************************************************************/

void BTA_DmConfigEir(tBTA_DM_EIR_CONF *eir_config)
{
    tBTA_DM_API_CONFIG_EIR    *p_msg;

    UINT8 eir_manufac_spec_len = eir_config->bta_dm_eir_manufac_spec_len;
    UINT8 eir_url_len = eir_config->bta_dm_eir_url_len;

    if (eir_manufac_spec_len > HCI_EXT_INQ_RESPONSE_LEN) {
        APPL_TRACE_WARNING ("%s: Manufacturer data is too long(%d), cut it to %d\n",
                            __func__, eir_manufac_spec_len, HCI_EXT_INQ_RESPONSE_LEN);
        eir_manufac_spec_len = HCI_EXT_INQ_RESPONSE_LEN;
    }
    if (eir_url_len > HCI_EXT_INQ_RESPONSE_LEN) {
        APPL_TRACE_WARNING ("%s: URL is too long(%d), cut it to %d\n",
                            __func__, eir_url_len, HCI_EXT_INQ_RESPONSE_LEN);
        eir_url_len = HCI_EXT_INQ_RESPONSE_LEN;
    }

    if ((p_msg = (tBTA_DM_API_CONFIG_EIR *) osi_malloc(sizeof(tBTA_DM_API_CONFIG_EIR) + eir_manufac_spec_len + eir_url_len)) != NULL) {
        p_msg->hdr.event = BTA_DM_API_CONFIG_EIR_EVT;

        p_msg->eir_fec_required = eir_config->bta_dm_eir_fec_required;
        p_msg->eir_included_name = eir_config->bta_dm_eir_included_name;
        p_msg->eir_included_tx_power = eir_config->bta_dm_eir_included_tx_power;
        p_msg->eir_included_uuid = eir_config->bta_dm_eir_included_uuid;
        p_msg->eir_flags = eir_config->bta_dm_eir_flags;
        p_msg->eir_manufac_spec_len = eir_manufac_spec_len;
        p_msg->eir_manufac_spec = p_msg->data;
        p_msg->eir_url_len = eir_url_len;
        p_msg->eir_url = p_msg->data + eir_manufac_spec_len;

        if (eir_manufac_spec_len > 0) {
            memcpy(p_msg->eir_manufac_spec, eir_config->bta_dm_eir_manufac_spec, eir_manufac_spec_len);
        }

        if (eir_url_len > 0) {
            memcpy(p_msg->eir_url, eir_config->bta_dm_eir_url, eir_url_len);
        }

        bta_sys_sendmsg(p_msg);
    }
}


/*******************************************************************************
**
** Function         BTA_DmSetAfhChannels
**
** Description      This function sets the AFH channels
**
**
** Returns          void
**
*******************************************************************************/
void BTA_DmSetAfhChannels(const uint8_t *channels, tBTA_CMPL_CB  *set_afh_cb)
{
    tBTA_DM_API_SET_AFH_CHANNELS *p_msg;

    if ((p_msg = (tBTA_DM_API_SET_AFH_CHANNELS *) osi_malloc(sizeof(tBTA_DM_API_SET_AFH_CHANNELS))) != NULL) {
        p_msg->hdr.event = BTA_DM_API_SET_AFH_CHANNELS_EVT;

        p_msg->set_afh_cb = set_afh_cb;
        memcpy(p_msg->channels, channels, AFH_CHANNELS_LEN);

        bta_sys_sendmsg(p_msg);
    }
}

/*******************************************************************************
**
** Function         BTA_DmSetPageTimeout
**
** Description      This function sets the Bluetooth page timeout.
**
**
** Returns          void
**
*******************************************************************************/
void BTA_DmSetPageTimeout(UINT16 page_to, tBTM_CMPL_CB *p_cb)
{
    tBTA_DM_API_PAGE_TO_SET *p_msg;

    if ((p_msg = (tBTA_DM_API_PAGE_TO_SET *) osi_malloc(sizeof(tBTA_DM_API_PAGE_TO_SET))) != NULL) {
        p_msg->hdr.event = BTA_DM_API_PAGE_TO_SET_EVT;
        p_msg->page_to = page_to;
        p_msg->set_page_to_cb = p_cb;

        bta_sys_sendmsg(p_msg);
    }
}

/*******************************************************************************
**
** Function         BTA_DmGetPageTimeout
**
** Description      This function gets the Bluetooth page timeout.
**
**
** Returns          void
**
*******************************************************************************/
void BTA_DmGetPageTimeout(tBTM_CMPL_CB *p_cb)
{
    tBTA_DM_API_PAGE_TO_GET *p_msg;

    if ((p_msg = (tBTA_DM_API_PAGE_TO_GET *) osi_malloc(sizeof(tBTA_DM_API_PAGE_TO_GET))) != NULL) {
        p_msg->hdr.event = BTA_DM_API_PAGE_TO_GET_EVT;
        p_msg->get_page_to_cb = p_cb;

        bta_sys_sendmsg(p_msg);
    }
}

/*******************************************************************************
**
** Function         BTA_DmSetAclPktTypes
**
** Description      This function sets the packet types used for ACL traffic.
**
**
** Returns          void
**
*******************************************************************************/
void BTA_DmSetAclPktTypes(BD_ADDR remote_addr, UINT16 pkt_types, tBTM_CMPL_CB *p_cb)
{
    tBTA_DM_API_SET_ACL_PKT_TYPES *p_msg;

    if ((p_msg = (tBTA_DM_API_SET_ACL_PKT_TYPES *) osi_malloc(sizeof(tBTA_DM_API_SET_ACL_PKT_TYPES))) != NULL) {
        p_msg->hdr.event = BTA_DM_API_SET_ACL_PKT_TYPES_EVT;
        bdcpy(p_msg->rmt_addr, remote_addr);
        p_msg->pkt_types = pkt_types;
        p_msg->set_acl_pkt_types_cb = p_cb;

        bta_sys_sendmsg(p_msg);
    }
}

/*******************************************************************************
**
** Function         BTA_DmSetMinEncKeySize
**
** Description      This function sets the minimal size of encryption key.
**
**
** Returns          void
**
*******************************************************************************/
void BTA_DmSetMinEncKeySize(UINT8 key_size, tBTM_CMPL_CB *p_cb)
{
    tBTA_DM_API_SET_MIN_ENC_KEY_SIZE *p_msg;

    if ((p_msg = (tBTA_DM_API_SET_MIN_ENC_KEY_SIZE *) osi_malloc(sizeof(tBTA_DM_API_SET_MIN_ENC_KEY_SIZE))) != NULL) {
        p_msg->hdr.event = BTA_DM_API_SET_MIN_ENC_KEY_SIZE_EVT;
        p_msg->key_size = key_size;
        p_msg->set_min_enc_key_size_cb = p_cb;

        bta_sys_sendmsg(p_msg);
    }
}

/*******************************************************************************
**
** Function         BTA_DmGetRemoteName
**
** Description      This function gets the peer device's Bluetooth name.
**
**
** Returns          void
**
*******************************************************************************/
void BTA_DmGetRemoteName(BD_ADDR remote_addr, tBTA_CMPL_CB *rmt_name_cb)
{
    tBTA_DM_API_GET_REMOTE_NAME *p_msg;

    if ((p_msg = (tBTA_DM_API_GET_REMOTE_NAME *) osi_malloc(sizeof(tBTA_DM_API_GET_REMOTE_NAME))) != NULL) {
        p_msg->hdr.event = BTA_DM_API_GET_REMOTE_NAME_EVT;
        p_msg->rmt_name_cb = rmt_name_cb;
        bdcpy(p_msg->rmt_addr, remote_addr);
        bta_sys_sendmsg(p_msg);
    }
}

void BTA_DmReadRSSI(BD_ADDR remote_addr, tBTA_TRANSPORT transport, tBTA_CMPL_CB *cmpl_cb)
{
    tBTA_DM_API_READ_RSSI *p_msg;
    if ((p_msg = (tBTA_DM_API_READ_RSSI *)osi_malloc(sizeof(tBTA_DM_API_READ_RSSI))) != NULL) {
        p_msg->hdr.event = BTA_DM_API_READ_RSSI_EVT;
        memcpy(p_msg->remote_addr, remote_addr, sizeof(BD_ADDR));
        p_msg->transport = transport;
        p_msg->read_rssi_cb = cmpl_cb;
        bta_sys_sendmsg(p_msg);
    }
}

void BTA_DmReadBredrTxPwrLvl(tBTM_TX_PWR_LVL_TYPE type, tBTA_CMPL_CB *cmpl_cb)
{
    tBTA_DM_API_READ_BREDR_TX_PWR_LVL *p_msg;
    if ((p_msg = (tBTA_DM_API_READ_BREDR_TX_PWR_LVL *)osi_malloc(sizeof(tBTA_DM_API_READ_BREDR_TX_PWR_LVL))) != NULL) {
        p_msg->hdr.event = BTA_DM_API_READ_BREDR_TX_PWR_LVL_EVT;
        p_msg->type = type;
        p_msg->read_bredr_tx_pwr_lvl_cb = cmpl_cb;
        bta_sys_sendmsg(p_msg);
    }
}

void BTA_DmWriteBredrTxPwrLvl(tBTM_TX_PWR_LVL_TYPE type, INT8 tx_power, tBTA_CMPL_CB *cmpl_cb)
{
    tBTA_DM_API_WRITE_BREDR_TX_PWR_LVL *p_msg;
    if ((p_msg = (tBTA_DM_API_WRITE_BREDR_TX_PWR_LVL *)osi_malloc(sizeof(tBTA_DM_API_WRITE_BREDR_TX_PWR_LVL))) != NULL) {
        p_msg->hdr.event = BTA_DM_API_WRITE_BREDR_TX_PWR_LVL_EVT;
        p_msg->type = type;
        p_msg->tx_power = tx_power;
        p_msg->write_bredr_tx_pwr_lvl_cb = cmpl_cb;
        bta_sys_sendmsg(p_msg);
    }
}

/*******************************************************************************
**
** Function         BTA_DmSetVisibility
**
** Description      This function sets the Bluetooth connectable,
**                  discoverable, pairable and conn paired only modes of local device
**
**
** Returns          void
**
*******************************************************************************/
void BTA_DmSetVisibility(tBTA_DM_DISC disc_mode, tBTA_DM_CONN conn_mode, UINT8 pairable_mode, UINT8 conn_filter )
{

    tBTA_DM_API_SET_VISIBILITY    *p_msg;

    if ((p_msg = (tBTA_DM_API_SET_VISIBILITY *) osi_malloc(sizeof(tBTA_DM_API_SET_VISIBILITY))) != NULL) {
        p_msg->hdr.event = BTA_DM_API_SET_VISIBILITY_EVT;
        p_msg->disc_mode = disc_mode;
        p_msg->conn_mode = conn_mode;
        p_msg->pair_mode = pairable_mode;
        p_msg->conn_paired_only = conn_filter;


        bta_sys_sendmsg(p_msg);
    }


}

/*******************************************************************************
**
** Function         BTA_DmSearch
**
** Description      This function searches for peer Bluetooth devices. It performs
**                  an inquiry and gets the remote name for devices. Service
**                  discovery is done if services is non zero
**
**
** Returns          void
**
*******************************************************************************/
void BTA_DmSearch(tBTA_DM_INQ *p_dm_inq, tBTA_SERVICE_MASK services, tBTA_DM_SEARCH_CBACK *p_cback)
{

    tBTA_DM_API_SEARCH    *p_msg;

    if ((p_msg = (tBTA_DM_API_SEARCH *) osi_malloc(sizeof(tBTA_DM_API_SEARCH))) != NULL) {
        memset(p_msg, 0, sizeof(tBTA_DM_API_SEARCH));

        p_msg->hdr.event = BTA_DM_API_SEARCH_EVT;
        memcpy(&p_msg->inq_params, p_dm_inq, sizeof(tBTA_DM_INQ));
        p_msg->services = services;
        p_msg->p_cback = p_cback;
        p_msg->rs_res  = BTA_DM_RS_NONE;
        bta_sys_sendmsg(p_msg);
    }

}


/*******************************************************************************
**
** Function         BTA_DmSearchCancel
**
** Description      This function  cancels a search initiated by BTA_DmSearch
**
**
** Returns          void
**
*******************************************************************************/
void BTA_DmSearchCancel(void)
{
    BT_HDR    *p_msg;

    if ((p_msg = (BT_HDR *) osi_malloc(sizeof(BT_HDR))) != NULL) {
        p_msg->event = BTA_DM_API_SEARCH_CANCEL_EVT;
        bta_sys_sendmsg(p_msg);
    }

}

/*******************************************************************************
**
** Function         BTA_DmDiscover
**
** Description      This function does service discovery for services of a
**                  peer device
**
**
** Returns          void
**
*******************************************************************************/
void BTA_DmDiscover(BD_ADDR bd_addr, tBTA_SERVICE_MASK services,
                    tBTA_DM_SEARCH_CBACK *p_cback, BOOLEAN sdp_search)
{
    tBTA_DM_API_DISCOVER    *p_msg;

    if ((p_msg = (tBTA_DM_API_DISCOVER *) osi_malloc(sizeof(tBTA_DM_API_DISCOVER))) != NULL) {
        memset(p_msg, 0, sizeof(tBTA_DM_API_DISCOVER));

        p_msg->hdr.event = BTA_DM_API_DISCOVER_EVT;
        bdcpy(p_msg->bd_addr, bd_addr);
        p_msg->services = services;
        p_msg->p_cback = p_cback;
        p_msg->sdp_search = sdp_search;
        bta_sys_sendmsg(p_msg);
    }
}

/*******************************************************************************
**
** Function         BTA_DmDiscoverUUID
**
** Description      This function does service discovery for services of a
**                  peer device
**
**
** Returns          void
**
*******************************************************************************/
void BTA_DmDiscoverUUID(BD_ADDR bd_addr, tSDP_UUID *uuid,
                        tBTA_DM_SEARCH_CBACK *p_cback, BOOLEAN sdp_search)
{
    tBTA_DM_API_DISCOVER    *p_msg;

    if ((p_msg = (tBTA_DM_API_DISCOVER *) osi_malloc(sizeof(tBTA_DM_API_DISCOVER))) != NULL) {
        p_msg->hdr.event = BTA_DM_API_DISCOVER_EVT;
        bdcpy(p_msg->bd_addr, bd_addr);
        p_msg->services = BTA_USER_SERVICE_MASK; //Not exposed at API level
        p_msg->p_cback = p_cback;
        p_msg->sdp_search = sdp_search;

        memcpy( &p_msg->uuid, uuid, sizeof(tSDP_UUID) );
        bta_sys_sendmsg(p_msg);
    }
}

/*******************************************************************************
**
** Function         BTA_DmBond
**
** Description      This function initiates a bonding procedure with a peer
**                  device
**
**
** Returns          void
**
*******************************************************************************/
void BTA_DmBond(BD_ADDR bd_addr)
{
    tBTA_DM_API_BOND    *p_msg;

    p_msg = (tBTA_DM_API_BOND *) osi_malloc(sizeof(tBTA_DM_API_BOND));
    if (p_msg != NULL) {
        p_msg->hdr.event = BTA_DM_API_BOND_EVT;
        bdcpy(p_msg->bd_addr, bd_addr);
        p_msg->transport = BTA_TRANSPORT_UNKNOWN;
        bta_sys_sendmsg(p_msg);
    }
}

/*******************************************************************************
**
** Function         BTA_DmBondByTransports
**
** Description      This function initiates a bonding procedure with a peer
**                  device
**
**
** Returns          void
**
*******************************************************************************/
void BTA_DmBondByTransport(BD_ADDR bd_addr, tBTA_TRANSPORT transport)
{
    tBTA_DM_API_BOND    *p_msg;

    if ((p_msg = (tBTA_DM_API_BOND *) osi_malloc(sizeof(tBTA_DM_API_BOND))) != NULL) {
        p_msg->hdr.event = BTA_DM_API_BOND_EVT;
        bdcpy(p_msg->bd_addr, bd_addr);
        p_msg->transport = transport;
        bta_sys_sendmsg(p_msg);
    }


}

/*******************************************************************************
**
** Function         BTA_DmBondCancel
**
** Description      This function cancels the bonding procedure with a peer
**                  device
**
**
** Returns          void
**
*******************************************************************************/
void BTA_DmBondCancel(BD_ADDR bd_addr)
{
    tBTA_DM_API_BOND_CANCEL    *p_msg;

    if ((p_msg = (tBTA_DM_API_BOND_CANCEL *) osi_malloc(sizeof(tBTA_DM_API_BOND_CANCEL))) != NULL) {
        p_msg->hdr.event = BTA_DM_API_BOND_CANCEL_EVT;
        bdcpy(p_msg->bd_addr, bd_addr);
        bta_sys_sendmsg(p_msg);
    }
}

/*******************************************************************************
**
** Function         BTA_DMSetPinType
**
** Description      This function set pin type as BTM_PIN_TYPE_FIXED or BTM_PIN_TYPE_VARIABLE
**
**
** Returns          void
**
*******************************************************************************/
void BTA_DMSetPinType (UINT8 pin_type, UINT8 *pin_code, UINT8 pin_code_len)
{
    tBTA_DM_API_SET_PIN_TYPE    *p_msg;

    if ((p_msg = (tBTA_DM_API_SET_PIN_TYPE *) osi_malloc(sizeof(tBTA_DM_API_SET_PIN_TYPE))) != NULL) {
        p_msg->hdr.event = BTA_DM_API_SET_PIN_TYPE_EVT;
        p_msg->pin_type = pin_type;
        p_msg->pin_len = pin_code_len;
        memcpy(p_msg->p_pin, pin_code, pin_code_len);
        bta_sys_sendmsg(p_msg);
    }
}

/*******************************************************************************
**
** Function         BTA_DmPinReply
**
** Description      This function provides a pincode for a remote device when
**                  one is requested by DM through BTA_DM_PIN_REQ_EVT
**
**
** Returns          void
**
*******************************************************************************/
void BTA_DmPinReply(BD_ADDR bd_addr, BOOLEAN accept, UINT8 pin_len, UINT8 *p_pin)
{
    tBTA_DM_API_PIN_REPLY    *p_msg;

    if ((p_msg = (tBTA_DM_API_PIN_REPLY *) osi_malloc(sizeof(tBTA_DM_API_PIN_REPLY))) != NULL) {
        p_msg->hdr.event = BTA_DM_API_PIN_REPLY_EVT;
        bdcpy(p_msg->bd_addr, bd_addr);
        p_msg->accept = accept;
        if (accept) {
            p_msg->pin_len = pin_len;
            memcpy(p_msg->p_pin, p_pin, pin_len);
        }
        bta_sys_sendmsg(p_msg);
    }

}

/*******************************************************************************
**
** Function         BTA_DmLocalOob
**
** Description      This function retrieves the OOB data from local controller.
**                  The result is reported by:
**                  - bta_dm_co_loc_oob_ext() if device supports secure
**                    connections (SC)
**                  - bta_dm_co_loc_oob() if device doesn't support SC
**
** Returns          void
**
*******************************************************************************/
void BTA_DmLocalOob(void)
{
    tBTA_DM_API_LOC_OOB    *p_msg;

    if ((p_msg = (tBTA_DM_API_LOC_OOB *) osi_malloc(sizeof(tBTA_DM_API_LOC_OOB))) != NULL) {
        p_msg->hdr.event = BTA_DM_API_LOC_OOB_EVT;
        bta_sys_sendmsg(p_msg);
    }
}

/*******************************************************************************
**
** Function         BTA_DmOobReply
**
**                  This function is called to provide the OOB data for
**                  SMP in response to BTA_LE_OOB_REQ_EVT
**
** Parameters:      bd_addr     - Address of the peer device
**                  len         - length of simple pairing Randomizer  C
**                  p_value     - simple pairing Randomizer  C.
**
** Returns          void
**
*******************************************************************************/
void BTA_DmOobReply(BD_ADDR bd_addr, UINT8 len, UINT8 *p_value)
{
    tBTA_DM_API_OOB_REPLY    *p_msg;

    if ((p_msg = (tBTA_DM_API_OOB_REPLY *) osi_malloc(sizeof(tBTA_DM_API_OOB_REPLY))) != NULL) {
        p_msg->hdr.event = BTA_DM_API_OOB_REPLY_EVT;
        if(p_value == NULL || len > BT_OCTET16_LEN) {
            osi_free(p_msg);
            return;
        }
        memcpy(p_msg->bd_addr, bd_addr, BD_ADDR_LEN);
        p_msg->len = len;
        memcpy(p_msg->value, p_value, len);
        bta_sys_sendmsg(p_msg);
    }
}

/*******************************************************************************
**
** Function         BTA_DmSecureConnectionOobReply
**
**                  This function is called to provide the OOB data for
**                  SMP in response to BTA_LE_OOB_REQ_EVT
**
** Parameters:      bd_addr     - Address of the peer device
**                  p_c         - Pointer to Confirmation
**                  p_r         - Pointer to Randomizer
**
** Returns          void
**
*******************************************************************************/
void BTA_DmSecureConnectionOobReply(BD_ADDR bd_addr, UINT8 *p_c, UINT8 *p_r)
{
    tBTA_DM_API_SC_OOB_REPLY    *p_msg;

    if ((p_msg = (tBTA_DM_API_SC_OOB_REPLY *) osi_malloc(sizeof(tBTA_DM_API_OOB_REPLY))) != NULL) {
        p_msg->hdr.event = BTA_DM_API_SC_OOB_REPLY_EVT;
        if((p_c == NULL) || (p_r == NULL)) {
            return;
        }
        memcpy(p_msg->bd_addr, bd_addr, BD_ADDR_LEN);
        memcpy(p_msg->c, p_c, BT_OCTET16_LEN);
        memcpy(p_msg->r, p_r, BT_OCTET16_LEN);
        bta_sys_sendmsg(p_msg);
    }
}
/*******************************************************************************
**
** Function         BTA_DmSecureConnectionCreateOobData
**
**                  This function is called to create the OOB data for
**                  SMP when secure connection
**
** Returns          void
**
*******************************************************************************/
void BTA_DmSecureConnectionCreateOobData(void)
{
    tBTA_DM_API_SC_CR_OOB_DATA *p_msg;

    if ((p_msg = (tBTA_DM_API_SC_CR_OOB_DATA *) osi_malloc(sizeof(tBTA_DM_API_SC_CR_OOB_DATA))) != NULL) {
        p_msg->hdr.event = BTA_DM_API_SC_CR_OOB_DATA_EVT;
        bta_sys_sendmsg(p_msg);
    }
}
/*******************************************************************************
**
** Function         BTA_DmConfirm
**
** Description      This function accepts or rejects the numerical value of the
**                  Simple Pairing process on BTA_DM_SP_CFM_REQ_EVT
**
** Returns          void
**
*******************************************************************************/
void BTA_DmConfirm(BD_ADDR bd_addr, BOOLEAN accept)
{
    tBTA_DM_API_CONFIRM    *p_msg;

    if ((p_msg = (tBTA_DM_API_CONFIRM *) osi_malloc(sizeof(tBTA_DM_API_CONFIRM))) != NULL) {
        p_msg->hdr.event = BTA_DM_API_CONFIRM_EVT;
        bdcpy(p_msg->bd_addr, bd_addr);
        p_msg->accept = accept;
        bta_sys_sendmsg(p_msg);
    }
}

/*******************************************************************************
**
** Function         BTA_DmPasskeyReqReply
**
** Description      This function is called to provide the passkey for
**                  Simple Pairing in response to BTA_DM_SP_KEY_REQ_EVT
**
** Returns          void
**
*******************************************************************************/
void BTA_DmPasskeyReqReply(BOOLEAN accept, BD_ADDR bd_addr, UINT32 passkey)
{
    tBTA_DM_API_KEY_REQ    *p_msg;
    if ((p_msg = (tBTA_DM_API_KEY_REQ *) osi_malloc(sizeof(tBTA_DM_API_KEY_REQ))) != NULL) {
        p_msg->hdr.event = BTA_DM_API_KEY_REQ_EVT;
        bdcpy(p_msg->bd_addr, bd_addr);
        p_msg->accept = accept;
        p_msg->passkey = passkey;
        bta_sys_sendmsg(p_msg);
    }
}
/*******************************************************************************
**
** Function         BTA_DmAddDevice
**
** Description      This function adds a device to the security database list of
**                  peer device
**
**
** Returns          void
**
*******************************************************************************/
void BTA_DmAddDevice(BD_ADDR bd_addr, DEV_CLASS dev_class, LINK_KEY link_key,
                     tBTA_SERVICE_MASK trusted_mask, BOOLEAN is_trusted,
                     UINT8 key_type, tBTA_IO_CAP io_cap, UINT8 pin_length,
                     UINT8 sc_support)
{

    tBTA_DM_API_ADD_DEVICE *p_msg;

    if ((p_msg = (tBTA_DM_API_ADD_DEVICE *) osi_malloc(sizeof(tBTA_DM_API_ADD_DEVICE))) != NULL) {
        memset (p_msg, 0, sizeof(tBTA_DM_API_ADD_DEVICE));

        p_msg->hdr.event = BTA_DM_API_ADD_DEVICE_EVT;
        bdcpy(p_msg->bd_addr, bd_addr);
        p_msg->tm = trusted_mask;
        p_msg->is_trusted = is_trusted;
        p_msg->io_cap = io_cap;
        p_msg->sc_support = sc_support;

        if (link_key) {
            p_msg->link_key_known = TRUE;
            p_msg->key_type = key_type;
            memcpy(p_msg->link_key, link_key, LINK_KEY_LEN);
        }

        /* Load device class if specified */
        if (dev_class) {
            p_msg->dc_known = TRUE;
            memcpy (p_msg->dc, dev_class, DEV_CLASS_LEN);
        }

        memset (p_msg->bd_name, 0, BD_NAME_LEN + 1);
        memset (p_msg->features, 0, sizeof (p_msg->features));
        p_msg->pin_length = pin_length;

        bta_sys_sendmsg(p_msg);
    }
}


/*******************************************************************************
**
** Function         BTA_DmRemoveDevice
**
** Description      This function removes a device from the security database list of
**                  peer device. It manages unpairing even while connected.
**
**
** Returns          void
**
*******************************************************************************/
tBTA_STATUS BTA_DmRemoveDevice(BD_ADDR bd_addr, tBT_TRANSPORT transport)
{
    tBTA_DM_API_REMOVE_DEVICE *p_msg;

    if ((p_msg = (tBTA_DM_API_REMOVE_DEVICE *) osi_malloc(sizeof(tBTA_DM_API_REMOVE_DEVICE))) != NULL) {
        memset (p_msg, 0, sizeof(tBTA_DM_API_REMOVE_DEVICE));

        p_msg->hdr.event = BTA_DM_API_REMOVE_DEVICE_EVT;
        bdcpy(p_msg->bd_addr, bd_addr);
        p_msg->transport = transport;
        bta_sys_sendmsg(p_msg);
    } else {
        return BTA_FAILURE;
    }

    return BTA_SUCCESS;
}
// #endif  ///SMP_INCLUDED == TRUE

/*******************************************************************************
**
** Function         BTA_GetEirService
**
** Description      This function is called to get BTA service mask from EIR.
**
** Parameters       p_eir - pointer of EIR significant part
**                  p_services - return the BTA service mask
**
** Returns          None
**
*******************************************************************************/
extern const UINT16 bta_service_id_to_uuid_lkup_tbl [];
void BTA_GetEirService( UINT8 *p_eir, tBTA_SERVICE_MASK *p_services )
{
    UINT8 xx, yy;
    UINT8 num_uuid, max_num_uuid = 32;
    UINT8 uuid_list[32 * LEN_UUID_16];
    UINT16 *p_uuid16 = (UINT16 *)uuid_list;
    tBTA_SERVICE_MASK mask;

    BTM_GetEirUuidList( p_eir, LEN_UUID_16, &num_uuid, uuid_list, max_num_uuid);
    for ( xx = 0; xx < num_uuid; xx++ ) {
        mask = 1;
        for ( yy = 0; yy < BTA_MAX_SERVICE_ID; yy++ ) {
            if ( *(p_uuid16 + xx) == bta_service_id_to_uuid_lkup_tbl[yy] ) {
                *p_services |= mask;
                break;
            }
            mask <<= 1;
        }

        /* for HSP v1.2 only device */
        if (*(p_uuid16 + xx) == UUID_SERVCLASS_HEADSET_HS) {
            *p_services |= BTA_HSP_SERVICE_MASK;
        }

        if (*(p_uuid16 + xx) == UUID_SERVCLASS_HDP_SOURCE) {
            *p_services |= BTA_HL_SERVICE_MASK;
        }

        if (*(p_uuid16 + xx) == UUID_SERVCLASS_HDP_SINK) {
            *p_services |= BTA_HL_SERVICE_MASK;
        }
    }
}

/*******************************************************************************
**
** Function         BTA_DmGetConnectionState
**
** Description      Returns whether the remote device is currently connected.
**
** Returns          0 if the device is NOT connected.
**
*******************************************************************************/
UINT16 BTA_DmGetConnectionState( BD_ADDR bd_addr )
{
    tBTA_DM_PEER_DEVICE *p_dev = bta_dm_find_peer_device(bd_addr);
    return (p_dev && p_dev->conn_state == BTA_DM_CONNECTED);
}

/*******************************************************************************
**                   Device Identification (DI) Server Functions
*******************************************************************************/
/*******************************************************************************
**
** Function         BTA_DmSetLocalDiRecord
**
** Description      This function adds a DI record to the local SDP database.
**
** Returns          BTA_SUCCESS if record set successfully, otherwise error code.
**
*******************************************************************************/
tBTA_STATUS BTA_DmSetLocalDiRecord( tBTA_DI_RECORD *p_device_info,
                                    UINT32 *p_handle )
{
    tBTA_STATUS status = BTA_FAILURE;

    if (bta_dm_di_cb.di_num < BTA_DI_NUM_MAX) {
        if (SDP_SetLocalDiRecord((tSDP_DI_RECORD *)p_device_info, p_handle) == SDP_SUCCESS) {
            if (!p_device_info->primary_record) {
                for (uint8_t i = 1; i < BTA_DI_NUM_MAX; i++) {
                    if (!bta_dm_di_cb.di_handle[i]) {
                        bta_dm_di_cb.di_handle[i] = *p_handle;
                        break;
                    }
                }
                bta_dm_di_cb.di_num++;
            } else if (!bta_dm_di_cb.di_handle[0]) {
                bta_dm_di_cb.di_handle[0] = *p_handle;
                bta_dm_di_cb.di_num++;
            } else {
                assert(bta_dm_di_cb.di_handle[0] == (*p_handle));
            }

            if (!bta_dm_di_cb.uuid_added) {
                bta_sys_add_uuid(UUID_SERVCLASS_PNP_INFORMATION);
                bta_dm_di_cb.uuid_added = TRUE;
            }

            status = BTA_SUCCESS;
        }
    }

    return status;
}

/*******************************************************************************
**
** Function         BTA_DmRemoveLocalDiRecord
**
** Description      This function removes a DI record from the local SDP database.
**
** Returns          BTA_SUCCESS if record is removed successfully, otherwise error code.
**
*******************************************************************************/
tBTA_STATUS BTA_DmRemoveLocalDiRecord(UINT32 handle)
{
    tBTA_STATUS status = BTA_FAILURE;

    for (uint8_t i = 0; i < BTA_DI_NUM_MAX; i++) {
        if (bta_dm_di_cb.di_handle[i] == handle) {
            if (SDP_DeleteRecord(handle)) {
                bta_dm_di_cb.di_handle[i] = 0;
                bta_dm_di_cb.di_num--;
                status = BTA_SUCCESS;
                break;
            }
        }
    }

    if (bta_dm_di_cb.di_num == 0 && bta_dm_di_cb.uuid_added) {
        bta_sys_remove_uuid(UUID_SERVCLASS_PNP_INFORMATION);
    }

    return status;
}

/*******************************************************************************
**
** Function         BTA_DmAddBleKey
**
** Description      Add/modify LE device information.  This function will be
**                  normally called during host startup to restore all required
**                  information stored in the NVRAM.
**
** Parameters:      bd_addr          - BD address of the peer
**                  p_le_key         - LE key values.
**                  key_type         - LE SMP key type.
**
** Returns          BTA_SUCCESS if successful
**                  BTA_FAIL if operation failed.
**
*******************************************************************************/


/*******************************************************************************
**                      BLE ADV data management API
********************************************************************************/

/*******************************************************************************
**                      BLE ADV data management API
********************************************************************************/

/*******************************************************************************
**
** Function         bta_dm_discover_send_msg
**
** Description      This function send discover message to BTA task.
**
** Returns          void
**
*******************************************************************************/
/*******************************************************************************
**
** Function         BTA_DmDiscoverByTransport
**
** Description      This function does service discovery on particular transport
**                  for services of a
**                  peer device. When services.num_uuid is 0, it indicates all
**                  GATT based services are to be searched; otherwise a list of
**                  UUID of interested services should be provided through
**                  p_services->p_uuid.
**
**
**
** Returns          void
**
*******************************************************************************/
void BTA_DmDiscoverByTransport(BD_ADDR bd_addr, tBTA_SERVICE_MASK_EXT *p_services,
                               tBTA_DM_SEARCH_CBACK *p_cback, BOOLEAN sdp_search,
                               tBTA_TRANSPORT transport)
{
}


/*******************************************************************************
**
** Function         BTA_DmDiscoverExt
**
** Description      This function does service discovery for services of a
**                  peer device. When services.num_uuid is 0, it indicates all
**                  GATT based services are to be searched; other wise a list of
**                  UUID of interested services should be provided through
**                  p_services->p_uuid.
**
**
**
** Returns          void
**
*******************************************************************************/
void BTA_DmDiscoverExt(BD_ADDR bd_addr, tBTA_SERVICE_MASK_EXT *p_services,
                       tBTA_DM_SEARCH_CBACK *p_cback, BOOLEAN sdp_search)
{

}

/*******************************************************************************
**
** Function         BTA_DmSearchExt
**
** Description      This function searches for peer Bluetooth devices. It performs
**                  an inquiry and gets the remote name for devices. Service
**                  discovery is done if services is non zero
**
** Parameters       p_dm_inq: inquiry conditions
**                  p_services: if service is not empty, service discovery will be done.
**                            for all GATT based service condition, put num_uuid, and
**                            p_uuid is the pointer to the list of UUID values.
**                  p_cback: callback function when search is completed.
**
**
**
** Returns          void
**
*******************************************************************************/
void BTA_DmSearchExt(tBTA_DM_INQ *p_dm_inq, tBTA_SERVICE_MASK_EXT *p_services, tBTA_DM_SEARCH_CBACK *p_cback)
{
    UNUSED(p_dm_inq);
    UNUSED(p_services);
    UNUSED(p_cback);
}
/*******************************************************************************
**
** Function         BTA_DmBleUpdateConnectionParam
**
** Description      Update connection parameters, can only be used when connection is up.
**
** Parameters:      bd_addr          - BD address of the peer
**                  min_int   -     minimum connection interval, [0x0004~ 0x4000]
**                  max_int   -     maximum connection interval, [0x0004~ 0x4000]
**                  latency   -     slave latency [0 ~ 500]
**                  timeout   -     supervision timeout [0x000a ~ 0xc80]
**
** Returns          void
**
*******************************************************************************/
void BTA_DmBleUpdateConnectionParam(BD_ADDR bd_addr, UINT16 min_int,
                                    UINT16 max_int, UINT16 latency,
                                    UINT16 timeout)
{
}

/*******************************************************************************
**
** Function         BTA_DmSetEncryption
**
** Description      This function is called to ensure that connection is
**                  encrypted.  Should be called only on an open connection.
**                  Typically only needed for connections that first want to
**                  bring up unencrypted links, then later encrypt them.
**
** Parameters:      bd_addr       - Address of the peer device
**                  transport     - transport of the link to be encruypted
**                  p_callback    - Pointer to callback function to indicate the
**                                  link encryption status
**                  sec_act       - This is the security action to indicate
**                                  what kind of BLE security level is required for
**                                  the BLE link if the BLE is supported
**                                  Note: This parameter is ignored for the BR/EDR link
**                                        or the BLE is not supported
**
** Returns          void
**
*******************************************************************************/
void BTA_DmSetEncryption(BD_ADDR bd_addr, tBTA_TRANSPORT transport, tBTA_DM_ENCRYPT_CBACK *p_callback,
                         tBTA_DM_BLE_SEC_ACT sec_act)
{
    tBTA_DM_API_SET_ENCRYPTION   *p_msg;

    APPL_TRACE_API("BTA_DmSetEncryption"); //todo
    if ((p_msg = (tBTA_DM_API_SET_ENCRYPTION *) osi_malloc(sizeof(tBTA_DM_API_SET_ENCRYPTION))) != NULL) {
        memset(p_msg, 0, sizeof(tBTA_DM_API_SET_ENCRYPTION));

        p_msg->hdr.event = BTA_DM_API_SET_ENCRYPTION_EVT;

        memcpy(p_msg->bd_addr, bd_addr, BD_ADDR_LEN);
        p_msg->transport    = transport;
        p_msg->p_callback      = p_callback;
        p_msg->sec_act         = sec_act;

        bta_sys_sendmsg(p_msg);
    }
}

