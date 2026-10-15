/*
 * SPDX-FileCopyrightText: 2015-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __BTC_MANAGE_H__
#define __BTC_MANAGE_H__

#include "btc/btc_task.h"

extern void *btc_profile_cb_tab[BTC_PID_NUM];
/* reset gatt callback table */
void esp_profile_cb_reset(void);

int btc_profile_cb_set(btc_pid_t profile_id, void *cb);
void *btc_profile_cb_get(btc_pid_t profile_id);

#endif /* __BTC_MANAGE_H__ */
