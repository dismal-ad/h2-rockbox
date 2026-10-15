/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "bta/bta_api.h"
#include "btc/btc_config.h"
#include "device/bdaddr.h"
#include "btc/btc_ble_storage.h"
#include "bta/bta_gatts_co.h"
#include "btc/btc_util.h"

//the maximum number of bonded devices
#define BONED_DEVICES_MAX_COUNT (BTM_SEC_MAX_BONDS)

static void _btc_storage_save(void)
{
    uint16_t addr_section_count = 0;
    bt_bdaddr_t bd_addr;

    const btc_config_section_iter_t *need_remove_iter = NULL;
    const btc_config_section_iter_t *iter = btc_config_section_begin();

    while (iter != btc_config_section_end()) {
        //store the next iter, if remove section, then will not loss the point

        const char *section = btc_config_section_name(iter);

        if (string_is_bdaddr(section) &&
                !btc_config_exist(section, BTC_BLE_STORAGE_DEV_TYPE_STR) &&
                !btc_config_exist(section, BTC_BLE_STORAGE_ADDR_TYPE_STR) &&
                !btc_config_exist(section, BTC_BLE_STORAGE_LINK_KEY_STR) &&
                !btc_config_exist(section, BTC_BLE_STORAGE_LE_KEY_PENC_STR) &&
                !btc_config_exist(section, BTC_BLE_STORAGE_LE_KEY_PID_STR) &&
                !btc_config_exist(section, BTC_BLE_STORAGE_LE_KEY_PCSRK_STR) &&
                !btc_config_exist(section, BTC_BLE_STORAGE_LE_KEY_LENC_STR) &&
                !btc_config_exist(section, BTC_BLE_STORAGE_LE_KEY_LCSRK_STR) &&
                !btc_config_exist(section, BTC_BLE_STORAGE_GATT_CL_SUPP_FEAT_STR) &&
                !btc_config_exist(section, BTC_BLE_STORAGE_GATT_DB_HASH_STR)) {
            iter = btc_config_section_next(iter);
            btc_config_remove_section(section);
            continue;
        }

        if (!string_is_bdaddr(section)) {
            iter = btc_config_section_next(iter);
            continue;
        }

        if(addr_section_count == BONED_DEVICES_MAX_COUNT) {
            need_remove_iter = iter;
        }
        addr_section_count ++;
        iter = btc_config_section_next(iter);
    }
    /*exceeded the maximum number of bonded devices, delete them */
    if (need_remove_iter) {
        while(need_remove_iter != btc_config_section_end()) {
            const char *need_remove_section = btc_config_section_name(need_remove_iter);
            if (!string_is_bdaddr(need_remove_section)) {
                need_remove_iter = btc_config_section_next(need_remove_iter);
                continue;
            }
            need_remove_iter = btc_config_section_next(need_remove_iter);
            //delete device info
            string_to_bdaddr(need_remove_section, &bd_addr);
            BTA_DmRemoveDevice(bd_addr.address, BT_TRANSPORT_LE);
            BTA_DmRemoveDevice(bd_addr.address, BT_TRANSPORT_BR_EDR);
            //delete config info
            if (btc_config_remove_section(need_remove_section)) {
                // The need_remove_section has been freed
                BTIF_TRACE_WARNING("Exceeded the maximum number of bonded devices. Deleting the last device info: %02x:%02x:%02x:%02x:%02x:%02x",
                                bd_addr.address[0], bd_addr.address[1], bd_addr.address[2], bd_addr.address[3], bd_addr.address[4], bd_addr.address[5]);
            }
        }
    }
    btc_config_flush();
}

void btc_storage_save(void)
{
    btc_config_lock();
    _btc_storage_save();
    btc_config_unlock();
}

static bt_status_t _btc_storage_set_ble_dev_auth_mode(bt_bdaddr_t *remote_bd_addr, uint8_t auth_mode, bool flush)
{
    int ret;
    bdstr_t bdstr;

    bdaddr_to_string(remote_bd_addr, bdstr, sizeof(bdstr_t));
    ret = btc_config_set_int(bdstr, BTC_BLE_STORAGE_LE_AUTH_MODE_STR, (int)auth_mode);
    if (ret == false) {
        return BT_STATUS_FAIL;
    }

    if (flush) {
        _btc_storage_save();
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t btc_storage_set_ble_dev_auth_mode(bt_bdaddr_t *remote_bd_addr, uint8_t auth_mode, bool flush)
{
    bt_status_t ret;

    btc_config_lock();
    ret = _btc_storage_set_ble_dev_auth_mode(remote_bd_addr, auth_mode, flush);
    btc_config_unlock();

    return ret;
}

static bt_status_t _btc_storage_get_ble_dev_auth_mode(bt_bdaddr_t *remote_bd_addr, int* auth_mode)
{
    bdstr_t bdstr;
    bdaddr_to_string(remote_bd_addr, bdstr, sizeof(bdstr));
    int ret = btc_config_get_int(bdstr, BTC_BLE_STORAGE_LE_AUTH_MODE_STR, auth_mode);
    return ret ? BT_STATUS_SUCCESS : BT_STATUS_FAIL;
}

bt_status_t btc_storage_get_ble_dev_auth_mode(bt_bdaddr_t *remote_bd_addr, int* auth_mode)
{
    bt_status_t ret;

    btc_config_lock();
    ret = _btc_storage_get_ble_dev_auth_mode(remote_bd_addr, auth_mode);
    btc_config_unlock();

    return ret;
}

static bt_status_t _btc_storage_remove_ble_dev_auth_mode(bt_bdaddr_t *remote_bd_addr, bool flush)
{
    bool ret = true;
    bdstr_t bdstr;
    uint32_t auth_mode = 0;

    bdaddr_to_string(remote_bd_addr, bdstr, sizeof(bdstr));

    ret = btc_config_get_int(bdstr, BTC_BLE_STORAGE_LE_AUTH_MODE_STR, (int *)&auth_mode);
    if (ret == false) {
        //cannot find the key, just return SUCCESS, indicate already removed
        return BT_STATUS_SUCCESS;
    }

    ret = btc_config_remove(bdstr, BTC_BLE_STORAGE_LE_AUTH_MODE_STR);
    if (ret == false) {
        return BT_STATUS_FAIL;
    }

    if (flush) {
        _btc_storage_save();
    }

    return  BT_STATUS_SUCCESS;
}

bt_status_t btc_storage_remove_ble_dev_auth_mode(bt_bdaddr_t *remote_bd_addr, bool flush)
{
    bt_status_t ret;

    btc_config_lock();
    ret = _btc_storage_remove_ble_dev_auth_mode(remote_bd_addr, flush);
    btc_config_unlock();

    return ret;
}

static bt_status_t _btc_storage_set_remote_addr_type(bt_bdaddr_t *remote_bd_addr, uint8_t addr_type, bool flush)
{
    int ret;
    bdstr_t bdstr;

    bdaddr_to_string(remote_bd_addr, bdstr, sizeof(bdstr_t));
    ret = btc_config_set_int(bdstr, BTC_BLE_STORAGE_ADDR_TYPE_STR, (int)addr_type);
    if (ret == false) {
        return BT_STATUS_FAIL;
    }

    if (flush) {
        _btc_storage_save();
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t btc_storage_set_remote_addr_type(bt_bdaddr_t *remote_bd_addr, uint8_t addr_type, bool flush)
{
    bt_status_t ret;

    btc_config_lock();
    ret = _btc_storage_set_remote_addr_type(remote_bd_addr, addr_type, flush);
    btc_config_unlock();

    return ret;
}

static bt_status_t _btc_storage_remove_remote_addr_type(bt_bdaddr_t *remote_bd_addr, bool flush)
{
    bool ret = true;
    bdstr_t bdstr;
    uint32_t dev_type = 0;

    bdaddr_to_string(remote_bd_addr, bdstr, sizeof(bdstr));

    ret = btc_config_get_int(bdstr, BTC_BLE_STORAGE_ADDR_TYPE_STR, (int *)&dev_type);
    if (ret == false) {
        //cannot find the key, just return SUCCESS, indicate already removed
        return BT_STATUS_SUCCESS;
    }

    ret = btc_config_remove(bdstr, BTC_BLE_STORAGE_ADDR_TYPE_STR);
    if (ret == false) {
        return BT_STATUS_FAIL;
    }

    if (flush) {
        _btc_storage_save();
    }

    return  BT_STATUS_SUCCESS;
}

bt_status_t btc_storage_remove_remote_addr_type(bt_bdaddr_t *remote_bd_addr, bool flush)
{
    bt_status_t ret;

    btc_config_lock();
    ret = _btc_storage_remove_remote_addr_type(remote_bd_addr, flush);
    btc_config_unlock();

    return ret;
}

static bt_status_t _btc_storage_get_remote_addr_type(bt_bdaddr_t *remote_bd_addr,
                                             int*addr_type)
{
    bdstr_t bdstr;
    bdaddr_to_string(remote_bd_addr, bdstr, sizeof(bdstr));
    int ret = btc_config_get_int(bdstr, BTC_BLE_STORAGE_ADDR_TYPE_STR, addr_type);
    return ret ? BT_STATUS_SUCCESS : BT_STATUS_FAIL;
}

bt_status_t btc_storage_get_remote_addr_type(bt_bdaddr_t *remote_bd_addr,
                                             int*addr_type)
{
    bt_status_t ret;

    btc_config_lock();
    ret = _btc_storage_get_remote_addr_type(remote_bd_addr, addr_type);
    btc_config_unlock();

    return ret;
}

