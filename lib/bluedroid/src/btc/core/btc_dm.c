/*
 * SPDX-FileCopyrightText: 2015-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "common/bt_target.h"
#include <stdlib.h>
#include <string.h>
#include "btc/btc_common.h"
#include "btc/btc_dm.h"
#include "btc/btc_main.h"
#include "btc/btc_util.h"
#include "common/bt_trace.h"
#include "common/bt_target.h"
#include "btc/btc_storage.h"
#include "btc/btc_ble_storage.h"
#include "btc_gap_ble.h"
#include "btm_int.h"
#include "bta/bta_api.h"
#include "bta/bta_gatt_api.h"
#include "osi/allocator.h"
#include "btc/btc_manage.h"


#include "btc_gap_bt.h"
/******************************************************************************
**  Constants & Macros
******************************************************************************/
#define BTA_SERVICE_ID_TO_SERVICE_MASK(id)       (1 << (id))

/******************************************************************************
**  Static variables
******************************************************************************/
btc_dm_cb_t btc_dm_cb = {0};

/******************************************************************************
**  Static functions
******************************************************************************/
/******************************************************************************
**  Externs
******************************************************************************/
extern bt_status_t btc_av_source_execute_service(BOOLEAN b_enable);
extern bt_status_t btc_av_sink_execute_service(BOOLEAN b_enable);
/******************************************************************************
**  Functions
******************************************************************************/
static void btc_dm_sec_arg_deep_free(btc_msg_t *msg)
{
    btc_dm_sec_args_t *arg = (btc_dm_sec_args_t *)(msg->arg);
    if (msg->act == BTA_DM_BLE_KEY_EVT) {
        osi_free(arg->sec.ble_key.p_key_value);
    }
}

void btc_dm_sec_arg_deep_copy(btc_msg_t *msg, void *dst, void *src)
{
    tBTA_DM_SEC *dst_dm_sec = (tBTA_DM_SEC *)dst;
    tBTA_DM_SEC *src_dm_sec = (tBTA_DM_SEC *)src;

    if (!src_dm_sec) {
        return;
    }

    assert(dst_dm_sec);
    memcpy(dst_dm_sec, src_dm_sec, sizeof(tBTA_DM_SEC));

    if (msg->act == BTA_DM_BLE_KEY_EVT) {
        dst_dm_sec->ble_key.p_key_value = osi_malloc(sizeof(tBTM_LE_KEY_VALUE));
        assert(src_dm_sec->ble_key.p_key_value);
        assert(dst_dm_sec->ble_key.p_key_value);
        memcpy(dst_dm_sec->ble_key.p_key_value, src_dm_sec->ble_key.p_key_value, sizeof(tBTM_LE_KEY_VALUE));
    }
}

/*******************************************************************************
**
** Function         btc_dm_evt
**
** Description      Switches context from BTE to BTC for all DM events
**
** Returns          void
**
*******************************************************************************/

void btc_dm_sec_evt(tBTA_DM_SEC_EVT event, tBTA_DM_SEC *data)
{
    btc_msg_t msg = {0};

    msg.sig = BTC_SIG_API_CB;
    msg.pid = BTC_PID_DM_SEC;
    msg.act = event;

    btc_transfer_context(&msg, (btc_dm_sec_args_t *)data,
                            data == NULL ? 0 : sizeof(btc_dm_sec_args_t),
                            btc_dm_sec_arg_deep_copy, btc_dm_sec_arg_deep_free);
}

static void btc_enable_bluetooth_evt(tBTA_STATUS status)
{
    if (status == BTA_SUCCESS) {
        future_ready(*btc_main_get_future_p(BTC_MAIN_ENABLE_FUTURE), FUTURE_SUCCESS);
    } else {
        future_ready(*btc_main_get_future_p(BTC_MAIN_ENABLE_FUTURE), FUTURE_FAIL);
    }
}

static void btc_disable_bluetooth_evt(void)
{
    BTC_TRACE_DEBUG("%s", __FUNCTION__);

    future_ready(*btc_main_get_future_p(BTC_MAIN_DISABLE_FUTURE), FUTURE_SUCCESS);
}

static void btc_dm_link_up_evt(tBTA_DM_LINK_UP *p_link_up)
{
    BD_ADDR bd_addr;
    bt_bdaddr_t bt_bdaddr;


    if (p_link_up->sc_downgrade == 1) {
        memcpy(bt_bdaddr.address, p_link_up->bd_addr, sizeof(BD_ADDR));
        if (btc_storage_remove_bonded_device(&bt_bdaddr) == BT_STATUS_SUCCESS) {
            memcpy(bd_addr, p_link_up->bd_addr, sizeof(BD_ADDR));
            if (BTA_DmRemoveDevice(bd_addr, BT_TRANSPORT_BR_EDR) == BTA_SUCCESS) {
                BTC_TRACE_EVENT(" %s() Bonding information removed.", __FUNCTION__);
            } else {
                BTC_TRACE_ERROR(" %s() BTA_DmRemoveDevice error", __FUNCTION__);
            }
        } else {
            BTC_TRACE_ERROR(" %s() btc_storage_remove_bonded_device error", __FUNCTION__);
        }
    }
}

static void btc_dm_auth_cmpl_evt (tBTA_DM_AUTH_CMPL *p_auth_cmpl)
{
    /* Save link key, if not temporary */
    bt_bdaddr_t bd_addr;
    bt_status_t status;
    BTC_TRACE_DEBUG("%s: bond state success %d, present %d, type%d\n", __func__, p_auth_cmpl->success,
              p_auth_cmpl->key_present, p_auth_cmpl->key_type);

    bdcpy(bd_addr.address, p_auth_cmpl->bd_addr);
    if ( (p_auth_cmpl->success == TRUE) && (p_auth_cmpl->key_present) ) {
            if (1) {
                bt_status_t ret __attribute__((unused));
                BTC_TRACE_DEBUG("%s: Storing link key. key_type=0x%x",
                          __FUNCTION__, p_auth_cmpl->key_type);
                ret = btc_storage_add_bonded_device(&bd_addr,
                                                    p_auth_cmpl->key, p_auth_cmpl->key_type,
                                                    16, p_auth_cmpl->sc_support);
                BTC_ASSERTC(ret == BT_STATUS_SUCCESS, "storing link key failed", ret);
            } else {
                BTC_TRACE_DEBUG("%s: Temporary key. Not storing. key_type=0x%x",
                          __FUNCTION__, p_auth_cmpl->key_type);
            }
    }

    // Skip SDP for certain  HID Devices
    if (p_auth_cmpl->success) {
        status = BT_STATUS_SUCCESS;
    } else {
        // Map the HCI fail reason  to  bt status
        switch (p_auth_cmpl->fail_reason) {
        case HCI_ERR_PAGE_TIMEOUT:
            BTC_TRACE_WARNING("%s() - Pairing timeout; retrying () ...", __FUNCTION__);
            return;
        /* Fall-through */
        case HCI_ERR_CONNECTION_TOUT:
            status =  BT_STATUS_RMT_DEV_DOWN;
            break;

        case HCI_ERR_PAIRING_NOT_ALLOWED:
            status = BT_STATUS_AUTH_REJECTED;
            break;

        case HCI_ERR_LMP_RESPONSE_TIMEOUT:
            status =  BT_STATUS_AUTH_FAILURE;
            break;

        /* map the auth failure codes, so we can retry pairing if necessary */
        case HCI_ERR_AUTH_FAILURE:
        case HCI_ERR_KEY_MISSING:
            btc_storage_remove_bonded_device(&bd_addr);
        case HCI_ERR_HOST_REJECT_SECURITY:
        case HCI_ERR_ENCRY_MODE_NOT_ACCEPTABLE:
        case HCI_ERR_UNIT_KEY_USED:
        case HCI_ERR_PAIRING_WITH_UNIT_KEY_NOT_SUPPORTED:
        case HCI_ERR_INSUFFCIENT_SECURITY:
        case HCI_ERR_PEER_USER:
        case HCI_ERR_UNSPECIFIED:
            BTC_TRACE_ERROR(" %s() Authentication fail reason %d",
                      __FUNCTION__, p_auth_cmpl->fail_reason);
            /* if autopair attempts are more than 1, or not attempted */
            status =  BT_STATUS_AUTH_FAILURE;
            break;
        default:
            status =  BT_STATUS_FAIL;
            break;
        }
    }
    esp_bt_gap_cb_param_t param;
    bt_status_t ret;
    btc_msg_t *msg;

    msg = (btc_msg_t *)osi_malloc(sizeof(btc_msg_t) + sizeof(esp_bt_gap_cb_param_t));
    if (msg == NULL) {
        BTC_TRACE_ERROR("%s malloc fail", __func__);
        return;
    }
    msg->sig = BTC_SIG_API_CB;
    msg->pid = BTC_PID_GAP_BT;
    msg->act = BTC_GAP_BT_AUTH_CMPL_EVT;
    param.auth_cmpl.stat = status;
    param.auth_cmpl.lk_type = p_auth_cmpl->key_type;
    memcpy(param.auth_cmpl.bda, p_auth_cmpl->bd_addr, ESP_BD_ADDR_LEN);
    memcpy(param.auth_cmpl.device_name, p_auth_cmpl->bd_name, ESP_BT_GAP_MAX_BDNAME_LEN + 1);
    memcpy(msg->arg, &param, sizeof(esp_bt_gap_cb_param_t));

    ret = btc_inter_profile_call(msg);
    osi_free(msg);

    if (ret != BT_STATUS_SUCCESS) {
        BTC_TRACE_ERROR("%s btc_inter_profile_call failed\n", __func__);
    }

    (void) status;
}

static void btc_dm_enc_chg_evt (tBTA_DM_ENC_CHG *p_enc_chg)
{
    esp_bt_gap_cb_param_t param;
    bt_status_t ret;
    btc_msg_t *msg;

    msg = (btc_msg_t *)osi_malloc(sizeof(btc_msg_t) + sizeof(esp_bt_gap_cb_param_t));
    if (msg == NULL) {
        BTC_TRACE_ERROR("%s malloc fail", __func__);
        return;
    }
    msg->sig = BTC_SIG_API_CB;
    msg->pid = BTC_PID_GAP_BT;
    msg->act = BTC_GAP_BT_ENC_CHG_EVT;
    param.enc_chg.enc_mode = p_enc_chg->enc_mode;
    memcpy(param.enc_chg.bda, p_enc_chg->bd_addr, ESP_BD_ADDR_LEN);
    memcpy(msg->arg, &param, sizeof(esp_bt_gap_cb_param_t));

    ret = btc_inter_profile_call(msg);
    osi_free(msg);

    if (ret != BT_STATUS_SUCCESS) {
        BTC_TRACE_ERROR("%s btc_inter_profile_call failed\n", __func__);
    }
}

static void btc_dm_pin_req_evt(tBTA_DM_PIN_REQ *p_pin_req)
{
    esp_bt_gap_cb_param_t param;
    bt_status_t ret;
    btc_msg_t *msg;

    msg = (btc_msg_t *)osi_malloc(sizeof(btc_msg_t) + sizeof(esp_bt_gap_cb_param_t));
    if (msg == NULL) {
        BTC_TRACE_ERROR("%s malloc fail", __func__);
        return;
    }
    msg->sig = BTC_SIG_API_CB;
    msg->pid = BTC_PID_GAP_BT;
    msg->act = BTC_GAP_BT_PIN_REQ_EVT;
    param.pin_req.min_16_digit = p_pin_req->min_16_digit;
    memcpy(param.pin_req.bda, p_pin_req->bd_addr, ESP_BD_ADDR_LEN);
    memcpy(msg->arg, &param, sizeof(esp_bt_gap_cb_param_t));

    ret = btc_inter_profile_call(msg);
    osi_free(msg);

    if (ret != BT_STATUS_SUCCESS) {
        BTC_TRACE_ERROR("%s btc_inter_profile_call failed\n", __func__);
    }
}

static void btc_dm_sp_cfm_req_evt(tBTA_DM_SP_CFM_REQ *p_cfm_req)
{
    /* Just Works requests go to the app as well: a connectable device must
     * not pair with anything that asks. It needn't show them to the user. */

    esp_bt_gap_cb_param_t param;
    bt_status_t ret;
    btc_msg_t *msg;

    msg = (btc_msg_t *)osi_malloc(sizeof(btc_msg_t) + sizeof(esp_bt_gap_cb_param_t));
    if (msg == NULL) {
        BTC_TRACE_ERROR("%s malloc fail", __func__);
        return;
    }
    msg->sig = BTC_SIG_API_CB;
    msg->pid = BTC_PID_GAP_BT;
    msg->act = BTC_GAP_BT_CFM_REQ_EVT;
    param.cfm_req.num_val = p_cfm_req->num_val;
    memcpy(param.cfm_req.bda, p_cfm_req->bd_addr, ESP_BD_ADDR_LEN);
    memcpy(msg->arg, &param, sizeof(esp_bt_gap_cb_param_t));

    ret = btc_inter_profile_call(msg);
    osi_free(msg);

    if (ret != BT_STATUS_SUCCESS) {
        BTC_TRACE_ERROR("%s btc_inter_profile_call failed\n", __func__);
    }
}

static void btc_dm_sp_key_notif_evt(tBTA_DM_SP_KEY_NOTIF *p_key_notif)
{
    esp_bt_gap_cb_param_t param;
    bt_status_t ret;
    btc_msg_t *msg;

    msg = (btc_msg_t *)osi_malloc(sizeof(btc_msg_t) + sizeof(esp_bt_gap_cb_param_t));
    if (msg == NULL) {
        BTC_TRACE_ERROR("%s malloc fail", __func__);
        return;
    }
    msg->sig = BTC_SIG_API_CB;
    msg->pid = BTC_PID_GAP_BT;
    msg->act = BTC_GAP_BT_KEY_NOTIF_EVT;
    param.key_notif.passkey = p_key_notif->passkey;
    memcpy(param.key_notif.bda, p_key_notif->bd_addr, ESP_BD_ADDR_LEN);
    memcpy(msg->arg, &param, sizeof(esp_bt_gap_cb_param_t));

    ret = btc_inter_profile_call(msg);
    osi_free(msg);

    if (ret != BT_STATUS_SUCCESS) {
        BTC_TRACE_ERROR("%s btc_inter_profile_call failed\n", __func__);
    }
}

static void btc_dm_sp_key_req_evt(tBTA_DM_SP_KEY_REQ *p_key_req)
{
    esp_bt_gap_cb_param_t param;
    bt_status_t ret;
    btc_msg_t *msg;

    msg = (btc_msg_t *)osi_malloc(sizeof(btc_msg_t) + sizeof(esp_bt_gap_cb_param_t));
    if (msg == NULL) {
        BTC_TRACE_ERROR("%s malloc fail", __func__);
        return;
    }
    msg->sig = BTC_SIG_API_CB;
    msg->pid = BTC_PID_GAP_BT;
    msg->act = BTC_GAP_BT_KEY_REQ_EVT;
    memcpy(param.key_req.bda, p_key_req->bd_addr, ESP_BD_ADDR_LEN);
    memcpy(msg->arg, &param, sizeof(esp_bt_gap_cb_param_t));

    ret = btc_inter_profile_call(msg);
    osi_free(msg);

    if (ret != BT_STATUS_SUCCESS) {
        BTC_TRACE_ERROR("%s btc_inter_profile_call failed\n", __func__);
    }
}

static void btc_dm_dev_unpaired_evt(tBTA_DM_LINK_DOWN *p_link_down)
{
    esp_bt_gap_cb_param_t param;
    BTC_TRACE_DEBUG("%s",__func__);
    memcpy(param.remove_bond_dev_cmpl.bda, p_link_down->bd_addr, ESP_BD_ADDR_LEN);
    btm_set_bond_type_dev(p_link_down->bd_addr, BOND_TYPE_UNKNOWN);
    if (p_link_down->status == HCI_SUCCESS) {
        //remove the bonded key in the config and nvs flash.
        param.remove_bond_dev_cmpl.status = btc_storage_remove_bonded_device((bt_bdaddr_t *)param.remove_bond_dev_cmpl.bda);
    } else {
        param.remove_bond_dev_cmpl.status = ESP_BT_STATUS_FAIL;
    }

    bt_status_t ret;
    btc_msg_t *msg;

    msg = (btc_msg_t *)osi_malloc(sizeof(btc_msg_t) + sizeof(esp_bt_gap_cb_param_t));
    if (msg == NULL) {
        BTC_TRACE_ERROR("%s malloc fail", __func__);
        return;
    }
    msg->sig = BTC_SIG_API_CB;
    msg->pid = BTC_PID_GAP_BT;
    msg->act = BTC_GAP_BT_REMOVE_BOND_DEV_COMPLETE_EVT;
    memcpy(msg->arg, &param, sizeof(esp_bt_gap_cb_param_t));

    ret = btc_inter_profile_call(msg);
    osi_free(msg);

    if (ret != BT_STATUS_SUCCESS) {
        BTC_TRACE_ERROR("%s btc_inter_profile_call failed\n", __func__);
    }
}


static void btc_dm_pm_mode_chg_evt(tBTA_DM_MODE_CHG *p_mode_chg)
{
    esp_bt_gap_cb_param_t param;
    bt_status_t ret;
    btc_msg_t *msg;

    msg = (btc_msg_t *)osi_malloc(sizeof(btc_msg_t) + sizeof(esp_bt_gap_cb_param_t));
    if (msg == NULL) {
        BTC_TRACE_ERROR("%s malloc fail", __func__);
        return;
    }
    msg->sig = BTC_SIG_API_CB;
    msg->pid = BTC_PID_GAP_BT;
    msg->act = BTC_GAP_BT_MODE_CHG_EVT;
    memcpy(param.mode_chg.bda, p_mode_chg->bd_addr, ESP_BD_ADDR_LEN);
    param.mode_chg.mode = p_mode_chg->mode;
    param.mode_chg.interval= p_mode_chg->interval;
    memcpy(msg->arg, &param, sizeof(esp_bt_gap_cb_param_t));

    ret = btc_inter_profile_call(msg);
    osi_free(msg);

    if (ret != BT_STATUS_SUCCESS) {
        BTC_TRACE_ERROR("%s btc_inter_profile_call failed\n", __func__);
    }
}

tBTA_SERVICE_MASK btc_get_enabled_services_mask(void)
{
    return btc_dm_cb.btc_enabled_services;
}

void btc_clear_services_mask(void)
{
    btc_dm_cb.btc_enabled_services = 0;
}

static bt_status_t btc_in_execute_service_request(tBTA_SERVICE_ID service_id,
        BOOLEAN b_enable)
{
    BTC_TRACE_DEBUG("%s service_id: %d\n", __FUNCTION__, service_id);
    /* Check the service_ID and invoke the profile's BT state changed API */
    switch (service_id) {
    case BTA_A2DP_SOURCE_SERVICE_ID:
        btc_av_source_execute_service(b_enable);
        break;
    case BTA_A2DP_SINK_SERVICE_ID:
        btc_av_sink_execute_service(b_enable);
        break;
    default:
        BTC_TRACE_ERROR("%s: Unknown service being enabled\n", __FUNCTION__);
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

void btc_dm_execute_service_request(BOOLEAN enable, char *p_param)
{
    btc_in_execute_service_request(*((tBTA_SERVICE_ID *)p_param), enable);
}

bt_status_t btc_dm_enable_service(tBTA_SERVICE_ID service_id)
{
    tBTA_SERVICE_ID *p_id = &service_id;

    btc_dm_cb.btc_enabled_services |= (1 << service_id);

    BTC_TRACE_DEBUG("%s: current services:0x%x", __FUNCTION__, btc_dm_cb.btc_enabled_services);

    btc_dm_execute_service_request(TRUE, (char *)p_id);

    return BT_STATUS_SUCCESS;
}

bt_status_t btc_dm_disable_service(tBTA_SERVICE_ID service_id)
{
    tBTA_SERVICE_ID *p_id = &service_id;

    btc_dm_cb.btc_enabled_services &= (tBTA_SERVICE_MASK)(~(1 << service_id));

    BTC_TRACE_DEBUG("%s: Current Services:0x%x", __FUNCTION__, btc_dm_cb.btc_enabled_services);

    btc_dm_execute_service_request(FALSE, (char *)p_id);

    return BT_STATUS_SUCCESS;
}
static void btc_dm_acl_link_stat(tBTA_DM_ACL_LINK_STAT *p_acl_link_stat)
{
    esp_bt_gap_cb_param_t param;
    esp_bt_gap_cb_event_t event = ESP_BT_GAP_EVT_MAX;
    bt_bdaddr_t bt_addr;

    switch (p_acl_link_stat->event) {
    case BTA_ACL_LINK_STAT_CONN_CMPL: {
        event = ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT;
        param.acl_conn_cmpl_stat.stat = btc_hci_to_esp_status(p_acl_link_stat->link_act.conn_cmpl.status);
        param.acl_conn_cmpl_stat.handle = p_acl_link_stat->link_act.conn_cmpl.handle;
        memcpy(param.acl_conn_cmpl_stat.bda, p_acl_link_stat->link_act.conn_cmpl.bd_addr, ESP_BD_ADDR_LEN);
        break;
    }
    case BTA_ACL_LINK_STAT_DISCONN_CMPL: {
        event = ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT;
        param.acl_disconn_cmpl_stat.reason = btc_hci_to_esp_status(p_acl_link_stat->link_act.disconn_cmpl.reason);
        param.acl_disconn_cmpl_stat.handle = p_acl_link_stat->link_act.disconn_cmpl.handle;
        memcpy(param.acl_disconn_cmpl_stat.bda, p_acl_link_stat->link_act.disconn_cmpl.bd_addr, ESP_BD_ADDR_LEN);
        break;
    }
    default: {
        BTC_TRACE_WARNING("%s: invalid event %x", __FUNCTION__, event);
        return;
    }
    }

    if (p_acl_link_stat->event == BTA_ACL_LINK_STAT_CONN_CMPL &&
        p_acl_link_stat->link_act.conn_cmpl.status == HCI_SUCCESS) {
        memcpy(bt_addr.address, p_acl_link_stat->link_act.conn_cmpl.bd_addr, sizeof(bt_addr.address));
        if (btc_storage_update_active_device(&bt_addr)) {
            BTC_TRACE_EVENT("Device: %02x:%02x:%02x:%02x:%02x:%02x, is not in bond list",
                            bt_addr.address[0], bt_addr.address[1],
                            bt_addr.address[2], bt_addr.address[3],
                            bt_addr.address[4], bt_addr.address[5]);
        }
    }
    esp_bt_gap_cb_t cb = (esp_bt_gap_cb_t)btc_profile_cb_get(BTC_PID_GAP_BT);
    if (cb) {
        cb(event, &param);
    }
}

void btc_dm_sec_cb_handler(btc_msg_t *msg)
{
    btc_dm_sec_args_t *arg = (btc_dm_sec_args_t *)(msg->arg);
    tBTA_DM_SEC *p_data = &(arg->sec);
    esp_ble_gap_cb_param_t param = {0};
    btc_msg_t *ble_msg;
    bool rsp_app = false;
    bt_status_t ret = BT_STATUS_SUCCESS;

    ble_msg = (btc_msg_t *)osi_malloc(sizeof(btc_msg_t) + sizeof(esp_ble_gap_cb_param_t));
    if (ble_msg == NULL) {
        BTC_TRACE_ERROR("%s malloc fail", __func__);
        btc_dm_sec_arg_deep_free(msg);
        return;
    }
    ble_msg->sig = BTC_SIG_API_CB;
    ble_msg->pid = BTC_PID_GAP_BLE;
    // tBTA_SERVICE_MASK service_mask;
    BTC_TRACE_DEBUG("btc_dm_upstreams_cback  ev: %d\n", msg->act);
    BTC_TRACE_DEBUG("%s act %d", __func__, msg->act);

    switch (msg->act) {
    case BTA_DM_ENABLE_EVT: {
        btc_clear_services_mask();
        //load the bonding device to the btm layer
        btc_storage_load_bonded_devices();

        /* Set initial device name, it can be overwritten later */
        if (p_data->enable.status == BTA_SUCCESS) {
            const char *initial_device_name = "ESP32";
            BTA_DmSetDeviceName(initial_device_name, BT_DEVICE_TYPE_DUMO);
        }
        btc_enable_bluetooth_evt(p_data->enable.status);
        break;
    }
    case BTA_DM_DISABLE_EVT: {
        tBTA_SERVICE_MASK service_mask = btc_get_enabled_services_mask();
        for (int i = 0; i <= BTA_MAX_SERVICE_ID; i++) {
            if (service_mask &
                    (tBTA_SERVICE_MASK)(BTA_SERVICE_ID_TO_SERVICE_MASK(i))) {
                btc_in_execute_service_request(i, FALSE);
            }
        }
        btc_disable_bluetooth_evt();
        break;
    }
    case BTA_DM_PIN_REQ_EVT:
        BTC_TRACE_DEBUG("BTA_DM_PIN_REQ_EVT");
        btc_dm_pin_req_evt(&p_data->pin_req);
        break;
    case BTA_DM_AUTH_CMPL_EVT:
        btc_dm_auth_cmpl_evt(&p_data->auth_cmpl);
        break;
    case BTA_DM_ENC_CHG_EVT:
        btc_dm_enc_chg_evt(&p_data->enc_chg);
        break;
    case BTA_DM_BOND_CANCEL_CMPL_EVT:
        BTC_TRACE_DEBUG("BTA_DM_BOND_CANCEL_CMPL_EVT");
        break;
    case BTA_DM_SP_CFM_REQ_EVT:
        btc_dm_sp_cfm_req_evt(&p_data->cfm_req);
        break;
    case BTA_DM_SP_KEY_NOTIF_EVT:
        btc_dm_sp_key_notif_evt(&p_data->key_notif);
        break;
    case BTA_DM_SP_KEY_REQ_EVT:
        btc_dm_sp_key_req_evt(&p_data->key_req);
        break;
    case BTA_DM_SP_KEYPRESS_EVT:
        BTC_TRACE_DEBUG("BTA_DM_SP_KEYPRESS_EVT");
        break;
    case BTA_DM_SP_RMT_OOB_EVT:
        BTC_TRACE_DEBUG("BTA_DM_SP_RMT_OOB_EVT");
        break;
    case BTA_DM_ACL_LINK_STAT_EVT: {
        btc_gap_bt_acl_link_num_update(&p_data->acl_link_stat);
        btc_dm_acl_link_stat(&p_data->acl_link_stat);
        break;
    }
    case BTA_DM_DEV_UNPAIRED_EVT: {
        btc_dm_dev_unpaired_evt(&p_data->link_down);
        break;
    }
    case BTA_DM_BUSY_LEVEL_EVT:
        {
        if (p_data->busy_level.level_flags & BTM_BL_INQUIRY_PAGING_MASK) {
            btc_gap_bt_busy_level_updated(p_data->busy_level.level_flags);
        }
        break;
        }
    case BTA_DM_LINK_DOWN_EVT:
    case BTA_DM_HW_ERROR_EVT:
        BTC_TRACE_DEBUG( "btc_dm_sec_cback : unhandled event (%d)\n", msg->act );
        break;
    case BTA_DM_LINK_UP_EVT:
        btc_dm_link_up_evt(&p_data->link_up);
        break;

    case BTA_DM_PM_MODE_CHG_EVT:
        BTC_TRACE_DEBUG("BTA_DM_PM_MODE_CHG_EVT mode:%d", p_data->mode_chg.mode);
        btc_dm_pm_mode_chg_evt(&p_data->mode_chg);
        break;

    case BTA_DM_AUTHORIZE_EVT:
    case BTA_DM_SIG_STRENGTH_EVT:
    case BTA_DM_ROLE_CHG_EVT:
        BTC_TRACE_DEBUG( "btc_dm_sec_cback : unhandled event (%d)\n", msg->act );
        break;
    default:
        BTC_TRACE_DEBUG( "btc_dm_sec_cback : unhandled event (%d)\n", msg->act );
        break;
    }

    if (rsp_app) {
        memcpy(ble_msg->arg, &param, sizeof(esp_ble_gap_cb_param_t));
        ret = btc_inter_profile_call(ble_msg);

        if (ret != BT_STATUS_SUCCESS) {
            BTC_TRACE_ERROR("%s btc_inter_profile_call failed\n", __func__);
        }
    }
    osi_free(ble_msg);
    btc_dm_sec_arg_deep_free(msg);
}
