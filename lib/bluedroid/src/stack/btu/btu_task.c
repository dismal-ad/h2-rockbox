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
#include <string.h>


#include "osi/alarm.h"
#include "osi/thread.h"
#include "common/bt_target.h"
#include "common/bt_trace.h"
#include "stack/bt_types.h"
#include "osi/allocator.h"
#include "osi/mutex.h"
#include "stack/btm_api.h"
#include "btm_int.h"
#include "stack/btu.h"
#include "osi/hash_map.h"
#include "stack/hcimsgs.h"
#include "l2c_int.h"
#include "osi/osi.h"
#include "sdpint.h"

#include "stack/port_api.h"
#include "stack/port_ext.h"

#include "gap_int.h"

#include "avdt_int.h"

#if (defined(BTA_INCLUDED) && BTA_INCLUDED == TRUE)
#include "bta/bta_sys.h"
#endif

typedef struct {
    uint32_t sig;
    void *param;
} btu_thread_evt_t;

//#if (defined(BT_APP_DEMO) && BT_APP_DEMO == TRUE)
//#include "bt_app_common.h"
//#endif

extern bt_status_t BTE_InitStack(void);
extern void BTE_DeinitStack(void);

/* Define BTU storage area
*/
tBTU_CB  btu_cb;

extern hash_map_t *btu_general_alarm_hash_map;
extern osi_mutex_t btu_general_alarm_lock;

// Oneshot timer queue.
extern hash_map_t *btu_oneshot_alarm_hash_map;
extern osi_mutex_t btu_oneshot_alarm_lock;

// l2cap timer queue.
extern hash_map_t *btu_l2cap_alarm_hash_map;
extern osi_mutex_t btu_l2cap_alarm_lock;

extern void *btu_thread;

extern bluedroid_init_done_cb_t bluedroid_init_done_cb;

/* Define a function prototype to allow a generic timeout handler */
typedef void (tUSER_TIMEOUT_FUNC) (TIMER_LIST_ENT *p_tle);

static void btu_l2cap_alarm_process(void *param);
static void btu_general_alarm_process(void *param);
static void btu_hci_msg_process(void *param);

#if (defined(BTA_INCLUDED) && BTA_INCLUDED == TRUE)
static void btu_bta_alarm_process(void *param);
#endif

static void btu_hci_msg_process(void *param)
{
    /* Determine the input message type. */
    BT_HDR *p_msg = (BT_HDR *)param;

    switch (p_msg->event & BT_EVT_MASK) {
    case BTU_POST_TO_TASK_NO_GOOD_HORRIBLE_HACK: // TODO(zachoverflow): remove this
      {
        post_to_task_hack_t *ph = (post_to_task_hack_t *) &p_msg->data[0];
        ph->callback(p_msg);
        break;
      }
    case BT_EVT_TO_BTU_HCI_ACL:
        /* All Acl Data goes to L2CAP */
        l2c_rcv_acl_data (p_msg);
        break;

    case BT_EVT_TO_BTU_L2C_SEG_XMIT:
        /* L2CAP segment transmit complete */
        l2c_link_segments_xmitted (p_msg);
        break;

    case BT_EVT_TO_BTU_HCI_SCO:

    case BT_EVT_TO_BTU_HCI_EVT:
        btu_hcif_process_event ((UINT8)(p_msg->event & BT_SUB_EVT_MASK), p_msg);
        osi_free(p_msg);

        break;

    case BT_EVT_TO_BTU_HCI_CMD:
        btu_hcif_send_cmd ((UINT8)(p_msg->event & BT_SUB_EVT_MASK), p_msg);
        break;

    default:;
        int i = 0;
        uint16_t mask = (UINT16) (p_msg->event & BT_EVT_MASK);
        BOOLEAN handled = FALSE;

        for (; !handled && i < BTU_MAX_REG_EVENT; i++) {
            if (btu_cb.event_reg[i].event_cb == NULL) {
                continue;
            }

            if (mask == btu_cb.event_reg[i].event_range) {
                if (btu_cb.event_reg[i].event_cb) {
                    btu_cb.event_reg[i].event_cb(p_msg);
                    handled = TRUE;
                }
            }
        }

        if (handled == FALSE) {
            osi_free (p_msg);
        }

        break;
    }

}

#if (defined(BTA_INCLUDED) && BTA_INCLUDED == TRUE)
static void btu_bta_alarm_process(void *param)
{
    TIMER_LIST_ENT *p_tle = (TIMER_LIST_ENT *)param;
    // call timer callback
    if (p_tle->p_cback) {
        (*p_tle->p_cback)(p_tle);
    } else if (p_tle->event) {
        BT_HDR *p_msg;
        if ((p_msg = (BT_HDR *) osi_malloc(sizeof(BT_HDR))) != NULL) {
            p_msg->event = p_tle->event;
            p_msg->layer_specific = 0;
            //osi_free(p_msg);
            bta_sys_sendmsg(p_msg);
        }
    }
}
#endif

bool btu_task_post(uint32_t sig, void *param, uint32_t timeout)
{
    bool status = false;

    switch (sig) {
        case SIG_BTU_START_UP:
            status = osi_thread_post(btu_thread, btu_task_start_up, param, 0, timeout);
            break;
        case SIG_BTU_HCI_MSG:
            status = osi_thread_post(btu_thread, btu_hci_msg_process, param, 0, timeout);
            break;
        case SIG_BTU_HCI_ADV_RPT_MSG:
            osi_free(param);
            status = false;
            break;
#if (defined(BTA_INCLUDED) && BTA_INCLUDED == TRUE)
        case SIG_BTU_BTA_MSG:
            status = osi_thread_post(btu_thread, bta_sys_event, param, 0, timeout);
            break;
        case SIG_BTU_BTA_ALARM:
            status = osi_thread_post(btu_thread, btu_bta_alarm_process, param, 0, timeout);
            break;
#endif
        case SIG_BTU_GENERAL_ALARM:
        case SIG_BTU_ONESHOT_ALARM:
            status = osi_thread_post(btu_thread, btu_general_alarm_process, param, 0, timeout);
            break;
        case SIG_BTU_L2CAP_ALARM:
            status = osi_thread_post(btu_thread, btu_l2cap_alarm_process, param, 0, timeout);
            break;
        default:
            break;
    }

    return status;
}

void btu_task_start_up(void *param)
{
    UNUSED(param);
    /* Initialize the mandatory core stack control blocks
       (BTU, BTM, L2CAP, and SDP)
     */
    btu_init_core();

    /* Initialize any optional stack components */
    BTE_InitStack();

#if (defined(BTA_INCLUDED) && BTA_INCLUDED == TRUE)
    bta_sys_init();
#endif

    // Inform the bt jni thread initialization is ok.
    // btif_transfer_context(btif_init_ok, 0, NULL, 0, NULL);
#if(defined(BT_APP_DEMO) && BT_APP_DEMO == TRUE)
    if (bluedroid_init_done_cb) {
        bluedroid_init_done_cb();
    }
#endif
}

void btu_task_shut_down(void)
{
#if (defined(BTA_INCLUDED) && BTA_INCLUDED == TRUE)
    bta_sys_free();
#endif
    BTE_DeinitStack();

    btu_free_core();
}

/*******************************************************************************
**
** Function         btu_start_timer
**
** Description      Start a timer for the specified amount of time.
**                  NOTE: The timeout resolution is in SECONDS! (Even
**                          though the timer structure field is ticks)
**
** Returns          void
**
*******************************************************************************/
static void btu_general_alarm_process(void *param)
{
    TIMER_LIST_ENT *p_tle = (TIMER_LIST_ENT *)param;
    assert(p_tle != NULL);

    switch (p_tle->event) {
    case BTU_TTYPE_BTM_DEV_CTL:
        btm_dev_timeout(p_tle);
        break;

    case BTU_TTYPE_L2CAP_LINK:
    case BTU_TTYPE_L2CAP_CHNL:
    case BTU_TTYPE_L2CAP_HOLD:
    case BTU_TTYPE_L2CAP_INFO:
    case BTU_TTYPE_L2CAP_FCR_ACK:
    case BTU_TTYPE_L2CAP_UPDA_CONN_PARAMS:
        l2c_process_timeout (p_tle);
        break;
    case BTU_TTYPE_SDP:
        sdp_conn_timeout ((tCONN_CB *)p_tle->param);
        break;
    case BTU_TTYPE_BTM_RMT_NAME:
        btm_inq_rmt_name_failed();
        break;
    case BTU_TTYPE_RFCOMM_MFC:
    case BTU_TTYPE_RFCOMM_PORT:
        rfcomm_process_timeout (p_tle);
        break;


    case BTU_TTYPE_AVDT_SCB_DELAY_RPT:
    case BTU_TTYPE_AVDT_CCB_RET:
    case BTU_TTYPE_AVDT_CCB_RSP:
    case BTU_TTYPE_AVDT_CCB_IDLE:
    case BTU_TTYPE_AVDT_SCB_TC:
        avdt_process_timeout(p_tle);
        break;

    case BTU_TTYPE_USER_FUNC: {
        tUSER_TIMEOUT_FUNC  *p_uf = (tUSER_TIMEOUT_FUNC *)p_tle->param;
        (*p_uf)(p_tle);
    }
    break;

    case BTU_TTYPE_BTM_QOS:
        btm_qos_setup_timeout(p_tle);
        break;
    case BTU_TTYPE_BTM_SET_PAGE_TO:
        btm_page_to_setup_timeout(p_tle);
        break;
    case BTU_TTYPE_BTM_BREDR_PWR_CTRL:
        btm_bredr_pwr_ctrl_timeout(p_tle);
        break;
    default:
        for (int i = 0; i < BTU_MAX_REG_TIMER; i++) {
            if (btu_cb.timer_reg[i].timer_cb == NULL) {
                continue;
            }
            if (btu_cb.timer_reg[i].p_tle == p_tle) {
                btu_cb.timer_reg[i].timer_cb(p_tle);
                break;
            }
        }
        break;
    }
}

void btu_general_alarm_cb(void *data)
{
    assert(data != NULL);
    TIMER_LIST_ENT *p_tle = (TIMER_LIST_ENT *)data;

    btu_task_post(SIG_BTU_GENERAL_ALARM, p_tle, OSI_THREAD_MAX_TIMEOUT);
}

void btu_start_timer(TIMER_LIST_ENT *p_tle, UINT16 type, UINT32 timeout_sec)
{
    osi_alarm_t *alarm = NULL;

    assert(p_tle != NULL);

    // Get the alarm for the timer list entry.
    osi_mutex_lock(&btu_general_alarm_lock, OSI_MUTEX_MAX_TIMEOUT);
    if (!hash_map_has_key(btu_general_alarm_hash_map, p_tle)) {
        alarm = osi_alarm_new("btu_gen", btu_general_alarm_cb, (void *)p_tle, 0);
        hash_map_set(btu_general_alarm_hash_map, p_tle, alarm);
    }
    osi_mutex_unlock(&btu_general_alarm_lock);

    alarm = hash_map_get(btu_general_alarm_hash_map, p_tle);
    if (alarm == NULL) {
        HCI_TRACE_ERROR("%s Unable to create alarm", __func__);
        return;
    }
    osi_alarm_cancel(alarm);

    p_tle->event = type;
    // NOTE: This value is in seconds but stored in a ticks field.
    p_tle->ticks = timeout_sec;
    p_tle->in_use = TRUE;
    osi_alarm_set(alarm, (period_ms_t)((period_ms_t)timeout_sec * 1000));
}


/*******************************************************************************
**
** Function         btu_stop_timer
**
** Description      Stop a timer.
**
** Returns          void
**
*******************************************************************************/
void btu_stop_timer(TIMER_LIST_ENT *p_tle)
{
    assert(p_tle != NULL);

    if (p_tle->in_use == FALSE) {
        return;
    }
    p_tle->in_use = FALSE;

    // Get the alarm for the timer list entry.
    osi_alarm_t *alarm = hash_map_get(btu_general_alarm_hash_map, p_tle);
    if (alarm == NULL) {
        HCI_TRACE_WARNING("%s Unable to find expected alarm in hashmap", __func__);
        return;
    }
    osi_alarm_cancel(alarm);
}

/*******************************************************************************
**
** Function         btu_free_timer
**
** Description      Stop and free a timer.
**
** Returns          void
**
*******************************************************************************/
void btu_free_timer(TIMER_LIST_ENT *p_tle)
{
    assert(p_tle != NULL);

    p_tle->in_use = FALSE;

    // Get the alarm for the timer list entry.
    osi_alarm_t *alarm = hash_map_get(btu_general_alarm_hash_map, p_tle);
    if (alarm == NULL) {
        HCI_TRACE_DEBUG("%s Unable to find expected alarm in hashmap", __func__);
        return;
    }
    osi_alarm_cancel(alarm);
    hash_map_erase(btu_general_alarm_hash_map, p_tle);
}

/*******************************************************************************
**
** Function         btu_start_quick_timer
**
** Description      Start a timer for the specified amount of time in ticks.
**
** Returns          void
**
*******************************************************************************/
static void btu_l2cap_alarm_process(void *param)
{
    TIMER_LIST_ENT *p_tle = (TIMER_LIST_ENT *)param;
    assert(p_tle != NULL);

    switch (p_tle->event) {
    case BTU_TTYPE_L2CAP_CHNL:      /* monitor or retransmission timer */
    case BTU_TTYPE_L2CAP_FCR_ACK:   /* ack timer */
        l2c_process_timeout (p_tle);
        break;

    default:
        break;
    }
}

static void btu_l2cap_alarm_cb(void *data)
{
    assert(data != NULL);
    TIMER_LIST_ENT *p_tle = (TIMER_LIST_ENT *)data;

    btu_task_post(SIG_BTU_L2CAP_ALARM, p_tle, OSI_THREAD_MAX_TIMEOUT);
}

void btu_start_quick_timer(TIMER_LIST_ENT *p_tle, UINT16 type, UINT32 timeout_ticks)
{
    osi_alarm_t *alarm = NULL;

    assert(p_tle != NULL);

    // Get the alarm for the timer list entry.
    osi_mutex_lock(&btu_l2cap_alarm_lock, OSI_MUTEX_MAX_TIMEOUT);
    if (!hash_map_has_key(btu_l2cap_alarm_hash_map, p_tle)) {
        alarm = osi_alarm_new("btu_l2cap", btu_l2cap_alarm_cb, (void *)p_tle, 0);
        hash_map_set(btu_l2cap_alarm_hash_map, p_tle, (void *)alarm);
    }
    osi_mutex_unlock(&btu_l2cap_alarm_lock);

    alarm = hash_map_get(btu_l2cap_alarm_hash_map, p_tle);
    if (alarm == NULL) {
        HCI_TRACE_ERROR("%s Unable to create alarm", __func__);
        return;
    }
    osi_alarm_cancel(alarm);

    p_tle->event = type;
    p_tle->ticks = timeout_ticks;
    p_tle->in_use = TRUE;
    // The quick timer ticks are 100ms long.
    osi_alarm_set(alarm, (period_ms_t)(timeout_ticks * 100));
}

/*******************************************************************************
**
** Function         btu_stop_quick_timer
**
** Description      Stop a timer.
**
** Returns          void
**
*******************************************************************************/
void btu_stop_quick_timer(TIMER_LIST_ENT *p_tle)
{
    assert(p_tle != NULL);

    if (p_tle->in_use == FALSE) {
        return;
    }
    p_tle->in_use = FALSE;

    // Get the alarm for the timer list entry.
    osi_alarm_t *alarm = hash_map_get(btu_l2cap_alarm_hash_map, p_tle);
    if (alarm == NULL) {
        HCI_TRACE_WARNING("%s Unable to find expected alarm in hashmap", __func__);
        return;
    }
    osi_alarm_cancel(alarm);
}

void btu_free_quick_timer(TIMER_LIST_ENT *p_tle)
{
    assert(p_tle != NULL);

    p_tle->in_use = FALSE;

    // Get the alarm for the timer list entry.
    osi_alarm_t *alarm = hash_map_get(btu_l2cap_alarm_hash_map, p_tle);
    if (alarm == NULL) {
        HCI_TRACE_DEBUG("%s Unable to find expected alarm in hashmap", __func__);
        return;
    }
    osi_alarm_cancel(alarm);
    hash_map_erase(btu_l2cap_alarm_hash_map, p_tle);
}

void btu_oneshot_alarm_cb(void *data)
{
    assert(data != NULL);
    TIMER_LIST_ENT *p_tle = (TIMER_LIST_ENT *)data;

    btu_stop_timer_oneshot(p_tle);

    btu_task_post(SIG_BTU_ONESHOT_ALARM, p_tle, OSI_THREAD_MAX_TIMEOUT);
}

/*
 * Starts a oneshot timer with a timeout in seconds.
 */
void btu_start_timer_oneshot(TIMER_LIST_ENT *p_tle, UINT16 type, UINT32 timeout_sec)
{
    osi_alarm_t *alarm = NULL;

    assert(p_tle != NULL);

    // Get the alarm for the timer list entry.
    osi_mutex_lock(&btu_oneshot_alarm_lock, OSI_MUTEX_MAX_TIMEOUT);
    if (!hash_map_has_key(btu_oneshot_alarm_hash_map, p_tle)) {
        alarm = osi_alarm_new("btu_oneshot", btu_oneshot_alarm_cb, (void *)p_tle, 0);
        hash_map_set(btu_oneshot_alarm_hash_map, p_tle, alarm);
    }
    osi_mutex_unlock(&btu_oneshot_alarm_lock);

    alarm = hash_map_get(btu_oneshot_alarm_hash_map, p_tle);
    if (alarm == NULL) {
        HCI_TRACE_ERROR("%s Unable to create alarm", __func__);
        return;
    }
    osi_alarm_cancel(alarm);

    p_tle->event = type;
    p_tle->in_use = TRUE;
    // NOTE: This value is in seconds but stored in a ticks field.
    p_tle->ticks = timeout_sec;
    osi_alarm_set(alarm, (period_ms_t)(timeout_sec * 1000));
}

void btu_stop_timer_oneshot(TIMER_LIST_ENT *p_tle)
{
    assert(p_tle != NULL);

    if (p_tle->in_use == FALSE) {
        return;
    }
    p_tle->in_use = FALSE;

    // Get the alarm for the timer list entry.
    osi_alarm_t *alarm = hash_map_get(btu_oneshot_alarm_hash_map, p_tle);
    if (alarm == NULL) {
        HCI_TRACE_WARNING("%s Unable to find expected alarm in hashmap", __func__);
        return;
    }
    osi_alarm_cancel(alarm);
}

