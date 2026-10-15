/*
 * SPDX-FileCopyrightText: 2015-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include "btc/btc_task.h"
#include "osi/thread.h"
#include "esp_log.h"
#include "bt_common.h"
#include "osi/allocator.h"
#include "btc/btc_alarm.h"

#include "btc/btc_manage.h"
#ifdef CONFIG_BT_BLUEDROID_ENABLED
#include "common/bt_target.h"
#include "btc/btc_main.h"
#include "btc/btc_dev.h"
#include "btc_gatts.h"
#include "btc_gattc.h"
#include "btc_gatt_common.h"
#include "btc_gap_ble.h"
#include "btc_iso_ble.h"
#include "btc_ble_cte.h"
#include "btc/btc_dm.h"
#include "bta/bta_gatt_api.h"
#include "btc/btc_profile_queue.h"
#include "btc_gap_bt.h"
#include "btc_av.h"
#include "btc_avrc.h"
#include "btc_av_co.h"
#endif

#define BTC_TASK_PINNED_TO_CORE         (TASK_PINNED_TO_CORE)
#define BTC_TASK_STACK_SIZE             (BT_BTC_TASK_STACK_SIZE + BT_TASK_EXTRA_STACK_SIZE)	//by menuconfig
#define BTC_TASK_NAME                   "BTC_TASK"
#define BTC_TASK_PRIO                   PRIORITY_BLUETOOTH // TODO(skyevg): think about priority more
#define BTC_TASK_WORKQUEUE_NUM          (2)
#define BTC_TASK_WORKQUEUE0_LEN         (0)
#define BTC_TASK_WORKQUEUE1_LEN         (5)

osi_thread_t *btc_thread;

static const btc_func_t profile_tab[BTC_PID_NUM] = {
#ifdef CONFIG_BT_BLUEDROID_ENABLED
    [BTC_PID_MAIN_INIT]   = {btc_main_call_handler,       NULL                    },
    [BTC_PID_DEV]         = {btc_dev_call_handler,        btc_dev_cb_handler      },
    [BTC_PID_GAP_BLE]     = {NULL, NULL},
    [BTC_PID_BLE_HID]     = {NULL, NULL},
    [BTC_PID_SPPLIKE]     = {NULL, NULL},
    [BTC_PID_DM_SEC]      = {NULL,                        btc_dm_sec_cb_handler   },
#endif
    [BTC_PID_ALARM]       = {btc_alarm_handler,           NULL                    },
#ifdef CONFIG_BT_BLUEDROID_ENABLED
    [BTC_PID_GAP_BT]    = {btc_gap_bt_call_handler,     btc_gap_bt_cb_handler   },
    [BTC_PID_PRF_QUE]     = {btc_profile_queue_handler,   NULL                    },
    [BTC_PID_A2DP]        = {btc_a2dp_call_handler,       btc_a2dp_cb_handler     },
    [BTC_PID_AVRC_CT]     = {btc_avrc_ct_call_handler,    NULL                    },
    [BTC_PID_AVRC_TG]     = {btc_avrc_tg_call_handler,    NULL                    },
#endif
};

/*****************************************************************************
**
** Function         btc_task
**
** Description      Process profile Task Thread.
******************************************************************************/
static void btc_thread_handler(void *arg)
{
    btc_msg_t *msg = (btc_msg_t *)arg;

    BTC_TRACE_DEBUG("%s msg %u %u %u %p\n", __func__, msg->sig, msg->pid, msg->act, msg->arg);
    switch (msg->sig) {
    case BTC_SIG_API_CALL:
        profile_tab[msg->pid].btc_call(msg);
        break;
    case BTC_SIG_API_CB:
        profile_tab[msg->pid].btc_cb(msg);
        break;
    default:
        break;
    }

    osi_free(msg);
}

static bt_status_t btc_task_post(btc_msg_t *msg, uint32_t timeout)
{
    if (osi_thread_post(btc_thread, btc_thread_handler, msg, 0, timeout) == false) {
        return BT_STATUS_BUSY;
    }

    return BT_STATUS_SUCCESS;
}

/**
 * transfer an message to another module in the different task.
 * @param  msg       message
 * @param  arg       parameter
 * @param  arg_len   length of parameter
 * @param  copy_func deep copy function
 * @param  free_func deep free function
 * @return           BT_STATUS_SUCCESS: success
 *                   others: fail
 */
bt_status_t btc_transfer_context(btc_msg_t *msg, void *arg, int arg_len, btc_arg_deep_copy_t copy_func,
                                    btc_arg_deep_free_t free_func)
{
    btc_msg_t* lmsg;
    bt_status_t ret;
    //                              arg XOR arg_len
    if ((msg == NULL) || ((arg == NULL) == !(arg_len == 0))) {
        BTC_TRACE_WARNING("%s Invalid parameters\n", __func__);
        return BT_STATUS_PARM_INVALID;
    }

    BTC_TRACE_DEBUG("%s msg %u %u %u %p\n", __func__, msg->sig, msg->pid, msg->act, arg);

    lmsg = (btc_msg_t *)osi_malloc(sizeof(btc_msg_t) + arg_len);
    if (lmsg == NULL) {
        BTC_TRACE_WARNING("%s No memory\n", __func__);
        return BT_STATUS_NOMEM;
    }

    memcpy(lmsg, msg, sizeof(btc_msg_t));
    if (arg) {
        memset(lmsg->arg, 0x00, arg_len);    //important, avoid arg which have no length
        memcpy(lmsg->arg, arg, arg_len);
        if (copy_func) {
            copy_func(lmsg, lmsg->arg, arg);
        }
    }

    ret = btc_task_post(lmsg, OSI_THREAD_MAX_TIMEOUT);
    if (ret != BT_STATUS_SUCCESS) {
        if (copy_func && free_func) {
            free_func(lmsg);
        }
        osi_free(lmsg);
    }

    return ret;
}

/**
 * transfer an message to another module in the same task.
 * @param  msg       message
 * @return           BT_STATUS_SUCCESS: success
 *                   others: fail
 */
bt_status_t btc_inter_profile_call(btc_msg_t *msg)
{
    if (msg == NULL) {
        return BT_STATUS_PARM_INVALID;
    }

    switch (msg->sig) {
    case BTC_SIG_API_CALL:
        profile_tab[msg->pid].btc_call(msg);
        break;
    case BTC_SIG_API_CB:
        profile_tab[msg->pid].btc_cb(msg);
        break;
    default:
        break;
    }
    return BT_STATUS_SUCCESS;
}


bt_status_t btc_init(void)
{
    const size_t workqueue_len[] = {BTC_TASK_WORKQUEUE0_LEN, BTC_TASK_WORKQUEUE1_LEN};
    btc_thread = osi_thread_create(BTC_TASK_NAME, BTC_TASK_STACK_SIZE, BTC_TASK_PRIO, BTC_TASK_PINNED_TO_CORE,
                                   BTC_TASK_WORKQUEUE_NUM, workqueue_len);
    if (btc_thread == NULL) {
        return BT_STATUS_NOMEM;
    }

    btc_gap_bt_init();

    /* TODO: initial the profile_tab */
    return BT_STATUS_SUCCESS;
}

void btc_deinit(void)
{
    btc_gap_bt_deinit();

    osi_thread_free(btc_thread);
    btc_thread = NULL;
}

int get_btc_work_queue_size(void)
{
    return osi_thread_queue_wait_size(btc_thread, 0);
}

osi_thread_t *btc_get_current_thread(void)
{
    return btc_thread;
}
