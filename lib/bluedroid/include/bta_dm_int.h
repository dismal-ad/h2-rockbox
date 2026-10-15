/******************************************************************************
 *
 *  Copyright (C) 2003-2012 Broadcom Corporation
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
 *  This is the private interface file for the BTA device manager.
 *
 ******************************************************************************/
#ifndef BTA_DM_INT_H
#define BTA_DM_INT_H

#include "common/bt_target.h"
#include "bta/bta_sys.h"



/*****************************************************************************
**  Constants and data types
*****************************************************************************/


#define BTA_COPY_DEVICE_CLASS(coddst, codsrc)   {((UINT8 *)(coddst))[0] = ((UINT8 *)(codsrc))[0]; \
                                                 ((UINT8 *)(coddst))[1] = ((UINT8 *)(codsrc))[1];  \
                                                 ((UINT8 *)(coddst))[2] = ((UINT8 *)(codsrc))[2];}


#define BTA_DM_MSG_LEN 50

#define BTA_SERVICE_ID_TO_SERVICE_MASK(id)       (1 << (id))

/* DM events */
enum {
    /* device manager local device API events */
    BTA_DM_API_ENABLE_EVT = BTA_SYS_EVT_START(BTA_ID_DM),
    BTA_DM_API_DISABLE_EVT,
    BTA_DM_API_SET_NAME_EVT,
    BTA_DM_API_GET_NAME_EVT,
    BTA_DM_API_CONFIG_EIR_EVT,
    BTA_DM_API_PAGE_TO_SET_EVT,
    BTA_DM_API_PAGE_TO_GET_EVT,
    BTA_DM_API_SET_ACL_PKT_TYPES_EVT,
    BTA_DM_API_SET_MIN_ENC_KEY_SIZE_EVT,
    BTA_DM_API_SET_AFH_CHANNELS_EVT,
    BTA_DM_API_GET_REMOTE_NAME_EVT,
    BTA_DM_API_SET_VISIBILITY_EVT,

    BTA_DM_ACL_CHANGE_EVT,
    BTA_DM_API_ADD_DEVICE_EVT,
    /* security API events */
    BTA_DM_API_BOND_EVT,
    BTA_DM_API_BOND_CANCEL_EVT,
    BTA_DM_API_SET_PIN_TYPE_EVT,
    BTA_DM_API_PIN_REPLY_EVT,
    /* power manager events */
    BTA_DM_PM_BTM_STATUS_EVT,
    BTA_DM_PM_TIMER_EVT,
    /* Quality of Service set events */
    BTA_DM_API_QOS_SET_EVT,
    /* simple pairing events */
    BTA_DM_API_CONFIRM_EVT,
    BTA_DM_API_KEY_REQ_EVT,
    BTA_DM_API_SET_ENCRYPTION_EVT,
    BTA_DM_API_LOC_OOB_EVT,
    BTA_DM_API_OOB_REPLY_EVT,
    BTA_DM_API_SC_OOB_REPLY_EVT,
    BTA_DM_API_SC_CR_OOB_DATA_EVT,
    BTA_DM_CI_IO_REQ_EVT,
    BTA_DM_CI_RMT_OOB_EVT,


    BTA_DM_API_REMOVE_DEVICE_EVT,
    BTA_DM_API_BLE_SET_CHANNELS_EVT,
    BTA_DM_API_UPDATE_WHITE_LIST_EVT,
    BTA_DM_API_CLEAR_WHITE_LIST_EVT,
    BTA_DM_API_READ_RSSI_EVT,
    BTA_DM_API_READ_BREDR_TX_PWR_LVL_EVT,
    BTA_DM_API_WRITE_BREDR_TX_PWR_LVL_EVT,
    BTA_DM_MAX_EVT
};


/* DM search events */
enum {
    /* DM search API events */
    BTA_DM_API_SEARCH_EVT = BTA_SYS_EVT_START(BTA_ID_DM_SEARCH),
    BTA_DM_API_SEARCH_CANCEL_EVT,
    BTA_DM_API_DISCOVER_EVT,
    BTA_DM_INQUIRY_CMPL_EVT,
    BTA_DM_REMT_NAME_EVT,
    BTA_DM_SDP_RESULT_EVT,
    BTA_DM_SEARCH_CMPL_EVT,
    BTA_DM_DISCOVERY_RESULT_EVT,
    BTA_DM_API_DI_DISCOVER_EVT,
    BTA_DM_DISC_CLOSE_TOUT_EVT,
};

/* data type for BTA_DM_API_ENABLE_EVT */
typedef struct {
    BT_HDR              hdr;
    tBTA_DM_SEC_CBACK *p_sec_cback;
} tBTA_DM_API_ENABLE;

/* data type for BTA_DM_API_SET_NAME_EVT */
typedef struct {
    BT_HDR              hdr;
    BD_NAME             name; /* max 248 bytes name, plus must be Null terminated */
    tBT_DEVICE_TYPE     name_type; /* name for BLE, name for BT or name for BTDM */
} tBTA_DM_API_SET_NAME;

typedef struct {
    BT_HDR                  hdr;
    tBTA_GET_DEV_NAME_CBACK *p_cback;
    tBT_DEVICE_TYPE         name_type;
} tBTA_DM_API_GET_NAME;

typedef struct {
    BT_HDR    hdr;
    UINT16    opcode;
    UINT8    param_len;
    UINT8     *p_param_buf;
    tBTA_SEND_VENDOR_HCI_CMPL_CBACK *vendor_hci_cb;
}tBTA_DM_API_SEND_VENDOR_HCI_CMD;

/* data type for BTA_DM_API_CONFIG_EIR_EVT */
typedef struct {
    BT_HDR              hdr;
    BOOLEAN             eir_fec_required;
    BOOLEAN             eir_included_name;
    BOOLEAN             eir_included_tx_power;
    BOOLEAN             eir_included_uuid;
    UINT8               eir_flags;
    UINT8               eir_manufac_spec_len;
    UINT8               *eir_manufac_spec;
    UINT8               eir_url_len;
    UINT8               *eir_url;
    UINT8               data[];
}tBTA_DM_API_CONFIG_EIR;

/* data type for BTA_DM_API_SET_AFH_CHANNELS_EVT */
typedef struct {
    BT_HDR              hdr;
    AFH_CHANNELS        channels;
    tBTA_CMPL_CB        *set_afh_cb;
}tBTA_DM_API_SET_AFH_CHANNELS;

/* data type for BTA_DM_API_PAGE_TO_SET_EVT */
typedef struct {
    BT_HDR              hdr;
    UINT16              page_to;
    tBTM_CMPL_CB        *set_page_to_cb;
} tBTA_DM_API_PAGE_TO_SET;

/* data type for BTA_DM_API_PAGE_TO_GET_EVT */
typedef struct {
    BT_HDR              hdr;
    tBTM_CMPL_CB        *get_page_to_cb;
} tBTA_DM_API_PAGE_TO_GET;

/* data type for BTA_DM_API_SET_ACL_PKT_TYPES_EVT */
typedef struct {
    BT_HDR              hdr;
    BD_ADDR             rmt_addr;
    UINT16              pkt_types;
    tBTM_CMPL_CB        *set_acl_pkt_types_cb;
} tBTA_DM_API_SET_ACL_PKT_TYPES;

/* data type for BTA_DM_API_SET_MIN_ENC_KEY_SIZE_EVT */
typedef struct {
    BT_HDR              hdr;
    UINT8               key_size;
    tBTM_CMPL_CB        *set_min_enc_key_size_cb;
} tBTA_DM_API_SET_MIN_ENC_KEY_SIZE;

/* data type for BTA_DM_API_GET_REMOTE_NAME_EVT */
typedef struct {
    BT_HDR         hdr;
    BD_ADDR        rmt_addr;
    BD_NAME        rmt_name;
    tBTA_TRANSPORT transport;
    tBTA_CMPL_CB   *rmt_name_cb;
} tBTA_DM_API_GET_REMOTE_NAME;

typedef struct {
    BT_HDR        hdr;
    BD_ADDR       remote_addr;
    tBTA_TRANSPORT  transport;
    tBTA_CMPL_CB  *read_rssi_cb;
}tBTA_DM_API_READ_RSSI;

typedef struct {
    BT_HDR        hdr;
    tBTM_TX_PWR_LVL_TYPE type;
    tBTA_CMPL_CB  *read_bredr_tx_pwr_lvl_cb;
} tBTA_DM_API_READ_BREDR_TX_PWR_LVL;

typedef struct {
    BT_HDR        hdr;
    tBTM_TX_PWR_LVL_TYPE type;
    INT8          tx_power;
    tBTA_CMPL_CB  *write_bredr_tx_pwr_lvl_cb;
} tBTA_DM_API_WRITE_BREDR_TX_PWR_LVL;

typedef struct {
    BT_HDR            hdr;
    BD_ADDR          remote_addr;
    tBTA_CMPL_CB    *read_ch_map_cb;
} tBTA_DM_API_READ_CH_MAP;

/* data type for BTA_DM_API_SET_VISIBILITY_EVT */
typedef struct {
    BT_HDR              hdr;
    tBTA_DM_DISC    disc_mode;
    tBTA_DM_CONN    conn_mode;
    UINT8           pair_mode;
    UINT8           conn_paired_only;
} tBTA_DM_API_SET_VISIBILITY;

enum {
    BTA_DM_RS_NONE,     /* straight API call */
    BTA_DM_RS_OK,       /* the role switch result - successful */
    BTA_DM_RS_FAIL      /* the role switch result - failed */
};
typedef UINT8 tBTA_DM_RS_RES;

/* data type for BTA_DM_API_SEARCH_EVT */
typedef struct {
    BT_HDR      hdr;
    tBTA_DM_INQ inq_params;
    tBTA_SERVICE_MASK services;
    tBTA_DM_SEARCH_CBACK *p_cback;
    tBTA_DM_RS_RES  rs_res;
} tBTA_DM_API_SEARCH;

/* data type for BTA_DM_API_DISCOVER_EVT */
typedef struct {
    BT_HDR      hdr;
    BD_ADDR bd_addr;
    tBTA_SERVICE_MASK services;
    tBTA_DM_SEARCH_CBACK *p_cback;
    BOOLEAN         sdp_search;
    tBTA_TRANSPORT  transport;
    tSDP_UUID    uuid;
} tBTA_DM_API_DISCOVER;

/* data type for BTA_DM_API_DI_DISC_EVT */
typedef struct {
    BT_HDR              hdr;
    BD_ADDR             bd_addr;
    tBTA_DISCOVERY_DB   *p_sdp_db;
    UINT32              len;
    tBTA_DM_SEARCH_CBACK *p_cback;
} tBTA_DM_API_DI_DISC;

/* data type for BTA_DM_API_BOND_EVT */
typedef struct {
    BT_HDR      hdr;
    BD_ADDR bd_addr;
    tBTA_TRANSPORT transport;
} tBTA_DM_API_BOND;

/* data type for BTA_DM_API_BOND_CANCEL_EVT */
typedef struct {
    BT_HDR          hdr;
    BD_ADDR         bd_addr;
    tBTA_TRANSPORT  transport;
} tBTA_DM_API_BOND_CANCEL;

/* data type for BTA_DM_API_SET_PIN_TYPE_EVT */
typedef struct {
    BT_HDR      hdr;
    UINT8       pin_type;
    UINT8       pin_len;
    UINT8       p_pin[PIN_CODE_LEN];
} tBTA_DM_API_SET_PIN_TYPE;

/* data type for BTA_DM_API_PIN_REPLY_EVT */
typedef struct {
    BT_HDR      hdr;
    BD_ADDR bd_addr;
    BOOLEAN accept;
    UINT8 pin_len;
    UINT8 p_pin[PIN_CODE_LEN];
} tBTA_DM_API_PIN_REPLY;

/* data type for BTA_DM_API_LOC_OOB_EVT */
typedef struct {
    BT_HDR      hdr;
} tBTA_DM_API_LOC_OOB;

/* data type for BTA_DM_API_OOB_REPLY_EVT */
typedef struct {
    BT_HDR      hdr;
    BD_ADDR bd_addr;
    UINT8       len;
    UINT8       value[BT_OCTET16_LEN];
    UINT8       c[BT_OCTET16_LEN];
    UINT8       r[BT_OCTET16_LEN];
} tBTA_DM_API_OOB_REPLY;

/* data type for BTA_DM_API_SC_OOB_REPLY_EVT */
typedef struct {
    BT_HDR      hdr;
    BD_ADDR     bd_addr;
    UINT8       c[BT_OCTET16_LEN];
    UINT8       r[BT_OCTET16_LEN];
} tBTA_DM_API_SC_OOB_REPLY;

/* data type for BTA_DM_API_SC_CR_OOB_DATA_EVT */
typedef struct {
    BT_HDR      hdr;
} tBTA_DM_API_SC_CR_OOB_DATA;

/* data type for BTA_DM_API_CONFIRM_EVT */
typedef struct {
    BT_HDR      hdr;
    BD_ADDR     bd_addr;
    BOOLEAN     accept;
} tBTA_DM_API_CONFIRM;

/* data type for BTA_DM_API_KEY_REQ_EVT */
typedef struct {
    BT_HDR      hdr;
    BD_ADDR     bd_addr;
    BOOLEAN     accept;
    UINT32      passkey;
} tBTA_DM_API_KEY_REQ;

/* data type for BTA_DM_CI_IO_REQ_EVT */
typedef struct {
    BT_HDR          hdr;
    BD_ADDR         bd_addr;
    tBTA_IO_CAP     io_cap;
    tBTA_OOB_DATA   oob_data;
    tBTA_AUTH_REQ   auth_req;
} tBTA_DM_CI_IO_REQ;

/* data type for BTA_DM_CI_RMT_OOB_EVT */
typedef struct {
    BT_HDR      hdr;
    BD_ADDR     bd_addr;
    BT_OCTET16  c;
    BT_OCTET16  r;
    BOOLEAN     accept;
} tBTA_DM_CI_RMT_OOB;

/* data type for BTA_DM_REMT_NAME_EVT */
typedef struct {
    BT_HDR      hdr;
    tBTA_DM_SEARCH  result;
} tBTA_DM_REM_NAME;

/* data type for tBTA_DM_DISC_RESULT */
typedef struct {
    BT_HDR      hdr;
    tBTA_DM_SEARCH  result;
} tBTA_DM_DISC_RESULT;


/* data type for BTA_DM_INQUIRY_CMPL_EVT */
typedef struct {
    BT_HDR      hdr;
    UINT8       num;
} tBTA_DM_INQUIRY_CMPL;

/* data type for BTA_DM_SDP_RESULT_EVT */
typedef struct {
    BT_HDR      hdr;
    UINT16 sdp_result;
} tBTA_DM_SDP_RESULT;

/* data type for BTA_DM_ACL_CHANGE_EVT */
typedef struct {
    BT_HDR          hdr;
    tBTM_BL_EVENT   event;
    UINT8           busy_level;
    UINT8           busy_level_flags;
    BOOLEAN         is_new;
    UINT8           new_role;
    BD_ADDR         bd_addr;
    UINT8           hci_status;
    BOOLEAN         sc_downgrade;
    tBT_TRANSPORT   transport;
} tBTA_DM_ACL_CHANGE;

/* data type for BTA_DM_PM_BTM_STATUS_EVT */
typedef struct {

    BT_HDR          hdr;
    BD_ADDR         bd_addr;
    tBTM_PM_STATUS  status;
    UINT16          value;
    UINT8           hci_status;

} tBTA_DM_PM_BTM_STATUS;

/* data type for BTA_DM_PM_TIMER_EVT */
typedef struct {
    BT_HDR          hdr;
    BD_ADDR         bd_addr;
    tBTA_DM_PM_ACTION  pm_request;
} tBTA_DM_PM_TIMER;

/* data type for BTA_DM_API_QOS_SET_EVT */
typedef struct {
    BT_HDR          hdr;
    BD_ADDR         bd_addr;
    UINT32          t_poll;
    tBTM_CMPL_CB    *p_cb;
} tBTA_DM_API_QOS_SET;

/* data type for BTA_DM_API_ADD_DEVICE_EVT */
typedef struct {
    BT_HDR              hdr;
    BD_ADDR             bd_addr;
    DEV_CLASS           dc;
    LINK_KEY            link_key;
    tBTA_SERVICE_MASK   tm;
    BOOLEAN             is_trusted;
    UINT8               key_type;
    tBTA_IO_CAP         io_cap;
    BOOLEAN             link_key_known;
    BOOLEAN             dc_known;
    BD_NAME             bd_name;
    UINT8               features[BTA_FEATURE_BYTES_PER_PAGE * (BTA_EXT_FEATURES_PAGE_MAX + 1)];
    UINT8               pin_length;
    UINT8               sc_support;
} tBTA_DM_API_ADD_DEVICE;

/* data type for BTA_DM_API_REMOVE_ACL_EVT */
typedef struct {
    BT_HDR              hdr;
    BD_ADDR             bd_addr;
    UINT8               transport;
} tBTA_DM_API_REMOVE_DEVICE;

/* data type for tBTA_DM_API_SET_ENCRYPTION */
typedef struct {
    BT_HDR                    hdr;
    tBTA_TRANSPORT            transport;
    tBTA_DM_ENCRYPT_CBACK     *p_callback;
    tBTA_DM_BLE_SEC_ACT       sec_act;
    BD_ADDR                   bd_addr;
} tBTA_DM_API_SET_ENCRYPTION;

typedef struct {
    BT_HDR      hdr;
    BD_ADDR     bd_addr;
    UINT16      min_int;
    UINT16      max_int;
    UINT16      latency;
    UINT16      timeout;
} tBTA_DM_API_UPDATE_CONN_PARAM;

/* union of all data types */
typedef union {
    /* event buffer header */
    BT_HDR              hdr;
    tBTA_DM_API_ENABLE  enable;

    tBTA_DM_API_SET_NAME set_name;
    tBTA_DM_API_GET_NAME get_name;
    tBTA_DM_API_SEND_VENDOR_HCI_CMD vendor_hci_cmd;
    tBTA_DM_API_CONFIG_EIR config_eir;

    tBTA_DM_API_SET_AFH_CHANNELS set_afh_channels;
    tBTA_DM_API_PAGE_TO_SET set_page_timeout;
    tBTA_DM_API_PAGE_TO_GET get_page_timeout;
    tBTA_DM_API_SET_ACL_PKT_TYPES set_acl_pkt_types;
    tBTA_DM_API_SET_MIN_ENC_KEY_SIZE set_min_enc_key_size;
    tBTA_DM_API_GET_REMOTE_NAME  get_rmt_name;

    tBTA_DM_API_READ_RSSI rssi;
    tBTA_DM_API_READ_BREDR_TX_PWR_LVL read_bredr_tx_pwr_lvl;
    tBTA_DM_API_WRITE_BREDR_TX_PWR_LVL write_bredr_tx_pwr_lvl;

    tBTA_DM_API_READ_CH_MAP ch_map;

    tBTA_DM_API_SET_VISIBILITY set_visibility;

    tBTA_DM_API_ADD_DEVICE  add_dev;

    tBTA_DM_API_REMOVE_DEVICE remove_dev;

    tBTA_DM_API_SEARCH search;
    tBTA_DM_API_DISCOVER discover;
    tBTA_DM_API_BOND bond;

    tBTA_DM_API_BOND_CANCEL bond_cancel;

    tBTA_DM_API_SET_PIN_TYPE set_pin_type;
    tBTA_DM_API_PIN_REPLY pin_reply;

    tBTA_DM_API_LOC_OOB     loc_oob;
    tBTA_DM_API_OOB_REPLY   oob_reply;
    tBTA_DM_API_SC_OOB_REPLY sc_oob_reply;
    tBTA_DM_API_CONFIRM     confirm;
    tBTA_DM_API_KEY_REQ     key_req;
    tBTA_DM_CI_IO_REQ       ci_io_req;
    tBTA_DM_CI_RMT_OOB      ci_rmt_oob;

    tBTA_DM_REM_NAME rem_name;

    tBTA_DM_DISC_RESULT disc_result;

    tBTA_DM_INQUIRY_CMPL inq_cmpl;

    tBTA_DM_SDP_RESULT sdp_event;

    tBTA_DM_ACL_CHANGE  acl_change;

    tBTA_DM_PM_BTM_STATUS pm_status;

    tBTA_DM_PM_TIMER pm_timer;

    /* Quality of Service set events */
    tBTA_DM_API_QOS_SET qos_set;

    tBTA_DM_API_DI_DISC     di_disc;
    tBTA_DM_API_SET_ENCRYPTION     set_encryption;

} tBTA_DM_MSG;


#define BTA_DM_NUM_PEER_DEVICE  MAX_ACL_CONNECTIONS

#define BTA_DM_NOT_CONNECTED  0
#define BTA_DM_CONNECTED      1
#define BTA_DM_UNPAIRING      2
typedef UINT8 tBTA_DM_CONN_STATE;


#define BTA_DM_DI_NONE          0x00       /* nothing special */
#define BTA_DM_DI_USE_SSR       0x10       /* set this bit if ssr is supported for this link */
#define BTA_DM_DI_AV_ACTIVE     0x20       /* set this bit if AV is active for this link */
#define BTA_DM_DI_SET_SNIFF     0x01       /* set this bit if call BTM_SetPowerMode(sniff) */
#define BTA_DM_DI_INT_SNIFF     0x02       /* set this bit if call BTM_SetPowerMode(sniff) & enter sniff mode */
#define BTA_DM_DI_ACP_SNIFF     0x04       /* set this bit if peer init sniff */
typedef UINT8 tBTA_DM_DEV_INFO;

/* set power mode request type */
#define BTA_DM_PM_RESTART       1
#define BTA_DM_PM_NEW_REQ       2
#define BTA_DM_PM_EXECUTE       3
typedef UINT8   tBTA_DM_PM_REQ;

typedef struct {
    BD_ADDR                     peer_bdaddr;
    UINT16                      link_policy;
    tBTA_DM_CONN_STATE          conn_state;
    tBTA_PREF_ROLES             pref_role;
    BOOLEAN                     in_use;
    tBTA_DM_DEV_INFO            info;
    tBTA_DM_ENCRYPT_CBACK      *p_encrypt_cback;
    tBTA_DM_PM_ACTION           pm_mode_attempted;
    tBTA_DM_PM_ACTION           pm_mode_failed;
    BOOLEAN                     remove_dev_pending;
    tBT_TRANSPORT               transport;
} tBTA_DM_PEER_DEVICE;



/* structure to store list of
  active connections */
typedef struct {
    tBTA_DM_PEER_DEVICE    peer_device[BTA_DM_NUM_PEER_DEVICE];
    UINT8                  count;
} tBTA_DM_ACTIVE_LINK;


typedef struct {
    BD_ADDR                 peer_bdaddr;
    tBTA_SYS_ID             id;
    UINT8                   app_id;
    tBTA_SYS_CONN_STATUS    state;
    BOOLEAN                 new_request;

} tBTA_DM_SRVCS;

#ifndef BTA_DM_NUM_CONN_SRVS
#define BTA_DM_NUM_CONN_SRVS   10
#endif

typedef struct {

    UINT8 count;
    tBTA_DM_SRVCS  conn_srvc[BTA_DM_NUM_CONN_SRVS];

}  tBTA_DM_CONNECTED_SRVCS;


typedef struct {
#define BTA_DM_PM_SNIFF_TIMER_IDX   0
#define BTA_DM_PM_PARK_TIMER_IDX    1
#define BTA_DM_PM_SUSPEND_TIMER_IDX 2
#define BTA_DM_PM_MODE_TIMER_MAX    3
    /*
     * Keep three different timers for PARK, SNIFF and SUSPEND if TBFC is
     * supported.
     */
    TIMER_LIST_ENT          timer[BTA_DM_PM_MODE_TIMER_MAX];

    UINT8                   srvc_id[BTA_DM_PM_MODE_TIMER_MAX];
    UINT8                   pm_action[BTA_DM_PM_MODE_TIMER_MAX];
    UINT8                   active;     /* number of active timer */

    BD_ADDR                 peer_bdaddr;
    BOOLEAN                 in_use;
} tBTA_PM_TIMER;

#define BTA_DM_NUM_PM_TIMER 7

/* DM control block */
typedef struct {
    BOOLEAN                     is_bta_dm_active;
    tBTA_DM_ACTIVE_LINK         device_list;
    tBTA_DM_SEC_CBACK           *p_sec_cback;
    UINT16                      state;
    BOOLEAN                     disabling;
    TIMER_LIST_ENT              disable_timer;
    UINT8                       pm_id;
    tBTA_PM_TIMER               pm_timer[BTA_DM_NUM_PM_TIMER];
    UINT32                      role_policy_mask;   /* the bits set indicates the modules that wants to remove role switch from the default link policy */
    UINT16                      cur_policy;         /* current default link policy */
    UINT16                      rs_event;           /* the event waiting for role switch */
    UINT8                       cur_av_count;       /* current AV connections */
    BOOLEAN                     disable_pair_mode;          /* disable pair mode or not */
    BOOLEAN                     conn_paired_only;   /* allow connectable to paired device only or not */
    tBTA_DM_API_SEARCH          search_msg;

    UINT16                      page_scan_interval;
    UINT16                      page_scan_window;
    UINT16                      inquiry_scan_interval;
    UINT16                      inquiry_scan_window;

    /* Storage for pin code request parameters */
    BD_ADDR                     pin_bd_addr;
    DEV_CLASS                   pin_dev_class;
    tBTA_DM_SEC_EVT             pin_evt;
    UINT32                      num_val;        /* the numeric value for comparison. If just_works, do not show this number to UI */
    BOOLEAN                     just_works;     /* TRUE, if "Just Works" association model */

    /* store UUID list for EIR */
    TIMER_LIST_ENT              app_ready_timer;
    UINT32                      eir_uuid[BTM_EIR_SERVICE_ARRAY_SIZE];
    tBT_UUID                    custom_uuid[BTA_EIR_SERVER_NUM_CUSTOM_UUID];


    tBTA_DM_ENCRYPT_CBACK      *p_encrypt_cback;
    TIMER_LIST_ENT              switch_delay_timer[BTA_DM_NUM_PEER_DEVICE];

} tBTA_DM_CB;

#ifndef BTA_DM_SDP_DB_SIZE
#define BTA_DM_SDP_DB_SIZE 250
#endif

/* DM search control block */
typedef struct {

    tBTA_DM_SEARCH_CBACK *p_search_cback;
    tBTM_INQ_INFO         *p_btm_inq_info;
    tBTA_SERVICE_MASK      services;
    tBTA_SERVICE_MASK      services_to_search;
    tBTA_SERVICE_MASK      services_found;
    tSDP_DISCOVERY_DB     *p_sdp_db;
    UINT16                 state;
    BD_ADDR                peer_bdaddr;
    BOOLEAN                name_discover_done;
    BD_NAME                peer_name;
    TIMER_LIST_ENT         search_timer;
    UINT8                  service_index;
    tBTA_DM_MSG           *p_search_queue;   /* search or discover commands during search cancel stored here */
    BOOLEAN                wait_disc;
    BOOLEAN                sdp_results;
    tSDP_UUID              uuid;
    UINT8                  peer_scn;
    BOOLEAN                sdp_search;
    BOOLEAN                cancel_pending; /* inquiry cancel is pending */
    tBTA_TRANSPORT         transport;


} tBTA_DM_SEARCH_CB;

/* DI control block */
typedef struct {
    tSDP_DISCOVERY_DB     *p_di_db;     /* pointer to the DI discovery database */
    BOOLEAN             uuid_added;
    UINT8               di_num;         /* total local DI record number */
    UINT32              di_handle[BTA_DI_NUM_MAX];  /* local DI record handle, the first one is primary record */
} tBTA_DM_DI_CB;

/* DM search state */
enum {

    BTA_DM_SEARCH_IDLE,
    BTA_DM_SEARCH_ACTIVE,
    BTA_DM_SEARCH_CANCELLING,
    BTA_DM_DISCOVER_ACTIVE

};



typedef struct {
    DEV_CLASS      dev_class;          /* local device class */
    UINT16         policy_settings;    /* link policy setting hold, sniff, park, MS switch */
    UINT16         page_timeout;       /* timeout for page in slots */
    UINT16         link_timeout;       /* link supervision timeout in slots */
    BOOLEAN        avoid_scatter;      /* TRUE to avoid scatternet when av is streaming (be the master) */

} tBTA_DM_CFG;

extern const UINT32 bta_service_id_to_btm_srv_id_lkup_tbl[];


typedef struct {
    UINT8   id;
    UINT8   app_id;
    UINT8   cfg;

} tBTA_DM_RM ;

extern tBTA_DM_CFG *const p_bta_dm_cfg;
extern tBTA_DM_RM *const p_bta_dm_rm_cfg;

typedef struct {

    UINT8  id;
    UINT8  app_id;
    UINT8  spec_idx;  /* index of spec table to use */

} tBTA_DM_PM_CFG;


typedef struct {

    tBTA_DM_PM_ACTION   power_mode;
    UINT16              timeout;

} tBTA_DM_PM_ACTN;

typedef struct {

    UINT8  allow_mask;         /* mask of sniff/hold/park modes to allow */
    tBTA_DM_PM_ACTN actn_tbl [BTA_DM_PM_NUM_EVTS][2];

} tBTA_DM_PM_SPEC;

typedef struct {
    UINT16      max_lat;
    UINT16      min_rmt_to;
    UINT16      min_loc_to;
} tBTA_DM_SSR_SPEC;

typedef struct {
    UINT16 manufacturer;
    UINT16 lmp_sub_version;
    UINT8 lmp_version;
} tBTA_DM_LMP_VER_INFO;

extern tBTA_DM_PM_CFG *const p_bta_dm_pm_cfg;
extern tBTA_DM_PM_SPEC *const p_bta_dm_pm_spec;
extern tBTM_PM_PWR_MD *const p_bta_dm_pm_md;

/* update dynamic BRCM Aware EIR data */
extern tBTA_DM_EIR_CONF bta_dm_eir_cfg;
extern tBTA_DM_EIR_CONF *p_bta_dm_eir_cfg;

/* DM control block */
extern tBTA_DM_CB  bta_dm_cb;

/* DM search control block */
extern tBTA_DM_SEARCH_CB  bta_dm_search_cb;

/* DI control block */
extern tBTA_DM_DI_CB  bta_dm_di_cb;

extern tBTA_DM_CONNECTED_SRVCS  bta_dm_conn_srvcs;

/* Discovery raw data buffer */
#define MAX_DISC_RAW_DATA_BUF       (1024)

extern BOOLEAN bta_dm_sm_execute(BT_HDR *p_msg);
extern void bta_dm_sm_disable( void );
extern void bta_dm_sm_deinit(void);
extern BOOLEAN bta_dm_search_sm_execute(BT_HDR *p_msg);
extern void bta_dm_search_sm_disable( void );


extern void bta_dm_enable (tBTA_DM_MSG *p_data);
extern void bta_dm_disable (tBTA_DM_MSG *p_data);
extern void bta_dm_set_dev_name (tBTA_DM_MSG *p_data);
extern void bta_dm_get_dev_name (tBTA_DM_MSG *p_data);
extern void bta_dm_send_vendor_hci(tBTA_DM_MSG *p_data);
extern void bta_dm_config_eir (tBTA_DM_MSG *p_data);
extern void bta_dm_set_page_timeout (tBTA_DM_MSG *p_data);
extern void bta_dm_get_page_timeout (tBTA_DM_MSG *p_data);
extern void bta_dm_set_acl_pkt_types (tBTA_DM_MSG *p_data);
extern void bta_dm_set_min_enc_key_size (tBTA_DM_MSG *p_data);
extern void bta_dm_set_afh_channels (tBTA_DM_MSG *p_data);
extern void bta_dm_read_rmt_name(tBTA_DM_MSG *p_data);
extern void bta_dm_ble_set_channels (tBTA_DM_MSG *p_data);
extern void bta_dm_update_white_list(tBTA_DM_MSG *p_data);
extern void bta_dm_clear_white_list(tBTA_DM_MSG *p_data);
extern void bta_dm_ble_read_adv_tx_power(tBTA_DM_MSG *p_data);
extern void bta_dm_read_rssi(tBTA_DM_MSG *p_data);
extern void bta_dm_read_bredr_tx_pwr_lvl(tBTA_DM_MSG *p_data);
extern void bta_dm_write_bredr_tx_pwr_lvl(tBTA_DM_MSG *p_data);
extern void bta_dm_read_ble_channel_map(tBTA_DM_MSG *p_data);
extern void bta_dm_set_visibility (tBTA_DM_MSG *p_data);

extern void bta_dm_set_scan_config(tBTA_DM_MSG *p_data);
extern void bta_dm_vendor_spec_command(tBTA_DM_MSG *p_data);
extern void bta_dm_bond (tBTA_DM_MSG *p_data);
extern void bta_dm_bond_cancel (tBTA_DM_MSG *p_data);
extern void bta_dm_set_pin_type (tBTA_DM_MSG *p_data);
extern void bta_dm_pin_reply (tBTA_DM_MSG *p_data);
extern void bta_dm_acl_change(tBTA_DM_MSG *p_data);
extern void bta_dm_add_device (tBTA_DM_MSG *p_data);
extern void bta_dm_remove_device (tBTA_DM_MSG *p_data);
extern void bta_dm_close_acl(tBTA_DM_MSG *p_data);

extern void bta_dm_add_ampkey (tBTA_DM_MSG *p_data);

extern void bta_dm_set_encryption(tBTA_DM_MSG *p_data);
extern void bta_dm_confirm(tBTA_DM_MSG *p_data);
extern void bta_dm_key_req(tBTA_DM_MSG *p_data);

extern void bta_dm_loc_oob(tBTA_DM_MSG *p_data);
extern void bta_dm_oob_reply(tBTA_DM_MSG *p_data);
extern void bta_dm_sc_oob_reply(tBTA_DM_MSG *p_data);
extern void bta_dm_sc_create_oob_data(tBTA_DM_MSG *p_data);
extern void bta_dm_ci_io_req_act(tBTA_DM_MSG *p_data);
extern void bta_dm_ci_rmt_oob_act(tBTA_DM_MSG *p_data);

extern void bta_dm_init_pm(void);
extern void bta_dm_disable_pm(void);
extern void bta_dm_pm_active(BD_ADDR peer_addr);
extern void bta_dm_pm_btm_status(tBTA_DM_MSG *p_data);
extern void bta_dm_pm_timer(tBTA_DM_MSG *p_data);

extern void bta_dm_set_qos(tBTA_DM_MSG *p_data);

extern UINT8 bta_dm_get_av_count(void);
extern void bta_dm_search_start (tBTA_DM_MSG *p_data);
extern void bta_dm_search_cancel (tBTA_DM_MSG *p_data);
extern void bta_dm_discover (tBTA_DM_MSG *p_data);
extern void bta_dm_di_disc (tBTA_DM_MSG *p_data);
extern void bta_dm_inq_cmpl (tBTA_DM_MSG *p_data);
extern void bta_dm_rmt_name (tBTA_DM_MSG *p_data);
extern void bta_dm_sdp_result (tBTA_DM_MSG *p_data);
extern void bta_dm_search_cmpl (tBTA_DM_MSG *p_data);
extern void bta_dm_free_sdp_db (tBTA_DM_MSG *p_data);
extern void bta_dm_disc_result (tBTA_DM_MSG *p_data);
extern void bta_dm_search_result (tBTA_DM_MSG *p_data);
extern void bta_dm_discovery_cmpl (tBTA_DM_MSG *p_data);
extern void bta_dm_queue_search (tBTA_DM_MSG *p_data);
extern void bta_dm_queue_disc (tBTA_DM_MSG *p_data);
extern void bta_dm_search_clear_queue (tBTA_DM_MSG *p_data);
extern void bta_dm_search_cancel_cmpl (tBTA_DM_MSG *p_data);
extern void bta_dm_search_cancel_notify (tBTA_DM_MSG *p_data);
extern void bta_dm_search_cancel_transac_cmpl(tBTA_DM_MSG *p_data);
extern void bta_dm_disc_rmt_name (tBTA_DM_MSG *p_data);
extern tBTA_DM_PEER_DEVICE *bta_dm_find_peer_device(BD_ADDR peer_addr);
void bta_dm_eir_update_uuid(tBT_UUID uuid, BOOLEAN adding);

extern void bta_dm_enable_test_mode(tBTA_DM_MSG *p_data);
extern void bta_dm_disable_test_mode(tBTA_DM_MSG *p_data);
extern void bta_dm_execute_callback(tBTA_DM_MSG *p_data);


#endif /* BTA_DM_INT_H */
