/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __ESP_BLE_ISO_API_H__
#define __ESP_BLE_ISO_API_H__

#include <stdint.h>
#include <stdbool.h>

#include "esp_err.h"
#include "esp_bt_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/// ISO BLE callback event type
typedef enum {
    ESP_BLE_ISO_BIG_CREATE_CMPL_EVT = 0,                     /*!< When BIG create complete, the event comes */
    ESP_BLE_ISO_BIG_TERMINATE_CMPL_EVT,                      /*!< When BIG terminate complete, the event comes */
    ESP_BLE_ISO_BIG_SYNC_ESTABLISHED_EVT,                    /*!< When BIG sync established, the event comes */
    ESP_BLE_ISO_BIG_SYNC_LOST_EVT,                           /*!< When BIG sync lost, the event comes */
    ESP_BLE_ISO_BIG_SYNC_TERMINATE_CMPL_EVT,                 /*!< When BIG sync terminate complete, the event comes */
    ESP_BLE_ISO_BIGINFO_ADV_REPORT_EVT,                      /*!< When receiving an Advertising PDU that contained a BIGInfo field, the event comes */
    ESP_BLE_ISO_ISO_DATA_PATH_UPDATE_EVT,                    /*!< When ISO data path update complete, the event comes */
    ESP_BLE_ISO_READ_ISO_TX_SYNC_CMPL_EVT,                   /*!< When reading tx sync complete, the event comes */
    ESP_BLE_ISO_READ_LINK_QUALITY_CMPL_EVT,                  /*!< When reading link quality complete, the event comes */
    ESP_BLE_ISO_SET_CIG_PARAMS_CMPL_EVT,                     /*!< When CIG parameters set complete, the event comes */
    ESP_BLE_ISO_CREATE_CIS_CMPL_EVT,                         /*!< When CIS create complete, the event comes */
    ESP_BLE_ISO_REMOVE_CIG_CMPL_EVT,                         /*!< When CIG remove complete, the event comes */
    ESP_BLE_ISO_CIS_REQUEST_EVT,                             /*!< When Receiving CIS request event, the event comes */
    ESP_BLE_ISO_ACCEPT_CIS_REQ_CMPL_EVT,                     /*!< When CIS request event accept complete, the event comes */
    ESP_BLE_ISO_REJECT_CIS_REQ_CMPL_EVT,                     /*!< When CIS request event reject complete, the event comes */
    ESP_BLE_ISO_CIS_ESTABLISHED_EVT,                         /*!< When CIS is established, the event comes */
    ESP_BLE_ISO_CIS_DISCONNECTED_EVT,                        /*!< When CIS is disconnected, the event comes */
    ESP_BLE_ISO_EVT_MAX,                                     /*!< when maximum advertising event complete, the event comes */
} esp_ble_iso_cb_event_t;

typedef enum {
    ESP_BLE_ISO_DATA_PATH_UNKNOWN = 0,
    ESP_BLE_ISO_DATA_PATH_SETUP   = 1,
    ESP_BLE_ISO_DATA_PATH_REMOVE  = 2,
    ESP_BLE_ISO_DATA_PATH_MAX,
} ESP_BLE_ISO_DATA_PATH_UPDATE_TYPE;

#define ESP_BLE_ISO_DATA_PATH_DIR_INPUT            (0x00)
#define ESP_BLE_ISO_DATA_PATH_DIR_OUTPUT           (0x01)

#define ESP_BLE_ISO_DATA_PATH_DIR_INPUT_BIT_MASK  (1 << 0)
#define ESP_BLE_ISO_DATA_PATH_DIR_OUTPUT_BIT_MASK (1 << 1)
#define ESP_BLE_ISO_DATA_PATH_DIR_ALL_BIT_MASK    (ESP_BLE_ISO_DATA_PATH_DIR_INPUT_BIT_MASK | ESP_BLE_ISO_DATA_PATH_DIR_OUTPUT_BIT_MASK)

#define ESP_BLE_MAX_BIS_NUM      (31)
#define ESP_BLE_MAX_CIS_NUM      (31)

#define BLE_ISO_SDU_INT_MIN       (0x0000FF)
#define BLE_ISO_SDU_INT_MAX       (0x0FFFFF)

#define BLE_ISO_WORST_CASE_SCA_LEVEL_500_PPM    (0x00)
#define BLE_ISO_WORST_CASE_SCA_LEVEL_250_PPM    (0x01)
#define BLE_ISO_WORST_CASE_SCA_LEVEL_150_PPM    (0x02)
#define BLE_ISO_WORST_CASE_SCA_LEVEL_100_PPM    (0x03)
#define BLE_ISO_WORST_CASE_SCA_LEVEL_75_PPM     (0x04)
#define BLE_ISO_WORST_CASE_SCA_LEVEL_50_PPM     (0x05)
#define BLE_ISO_WORST_CASE_SCA_LEVEL_30_PPM     (0x06)
#define BLE_ISO_WORST_CASE_SCA_LEVEL_20_PPM     (0x07)

#define BLE_ISO_PACKING_SEQUENTIAL                      (0x00)
#define BLE_ISO_PACKING_INTERLEAVED                     (0x00)

#define BLE_ISO_FRAMING_UNFRAMED_PDU                    (0x00)
#define BLE_ISO_FRAMING_FRAMED_PDU_SEGMENTABLE_MODE     (0x01)
#define BLE_ISO_FRAMING_FRAMED_PDU_UNSEGMENTABLE_MODE   (0x02)

#define BLE_ISO_MAX_TRANSPORT_LATENCY_MIN               (0x0005)
#define BLE_ISO_MAX_TRANSPORT_LATENCY_MAX               (0x0FA0)

typedef struct {
    uint16_t iso_handle;       /*!< Connection handle of the CIS or BIS*/
    uint8_t data_path_dir;     /*!< The direction for which the data path is being configured, value 0x00 to 0x01 */
    uint8_t data_path_id;      /*!< The data transport path used, value 0x00 to 0xFE */
    uint8_t coding_fmt;        /*!< See Assigned Numbers for Coding Format */
    uint16_t company_id;       /*!< Company ID, see Assigned Numbers for Company Identifier. Shall be ignored if coding_fmt is not 0xFF*/
    uint16_t vs_codec_id;      /*!< Vendor-defined codec ID, Shall be ignored if coding_fmt is not 0xFF */
    uint32_t controller_delay; /*!< Controller delay in microseconds, range: 0x000000 to 0x3D0900, time range: 0 s to 4 s*/
    uint8_t codec_cfg_len;     /*!< Length of codec configuration */
    uint8_t *codec_cfg;        /*!< Codec-specific configuration data */
} esp_ble_iso_set_data_path_params_t;

typedef struct {
    uint16_t iso_handle;   /*!< Connection handle of the CIS or BIS. Range: 0x0000 to 0x0EFF */
    uint8_t data_path_dir; /*!< which directions are to have the data path removed */
} __attribute__((packed)) esp_ble_iso_remove_data_path_params_t;

struct ble_iso_cis_params {
    uint8_t cis_id;          /*!< Used to identify a CIS */
    uint16_t max_sdu_c_to_p; /*!< Maximum size, in octets, of the payload from the Central’s Host */
    uint16_t max_sdu_p_to_c; /*!< Maximum size, in octets, of the payload from the Peripheral’s Host */
    uint8_t phy_c_to_p;      /*!< The transmitter PHY of packets from the Central */
    uint8_t phy_p_to_c;      /*!< The transmitter PHY of packets from the Peripheral */
    uint8_t rtn_c_to_p;      /*!< Number of times every CIS Data PDU should be retransmitted from the Central to the Peripheral */
    uint8_t rtn_p_to_c;      /*!< Number of times every CIS Data PDU should be retransmitted from the Peripheral to the Central */
} __attribute__((packed));

struct ble_iso_cis_params_test {
    uint8_t cis_id;          /*!< Used to identify a CIS */
    uint8_t nse;             /*!< Maximum number of subevents in each CIS event */
    uint16_t max_sdu_c_to_p; /*!< Maximum size, in octets, of the payload from the Central’s Host */
    uint16_t max_sdu_p_to_c; /*!< Maximum size, in octets, of the payload from the Peripheral’s Host */
    uint16_t max_pdu_c_to_p; /*!< Maximum size, in octets, of the payload from the Central’s Link Layer to the Peripheral’s Link Layer */
    uint16_t max_pdu_p_to_c; /*!< Maximum size, in octets, of the payload from the Peripheral’s Link Layer to the Central’s Link Layer */
    uint8_t phy_c_to_p;      /*!< The transmitter PHY of packets from the Central */
    uint8_t phy_p_to_c;      /*!< The transmitter PHY of packets from the Peripheral */
    uint8_t bn_c_to_p;       /*!< The burst number for Central to Peripheral */
    uint8_t bn_p_to_c;       /*!< The burst number for Peripheral to Central */
} __attribute__((packed));

/**
 * @brief ISO callback parameters union
 */
typedef union {
    /**
     * @brief ESP_BLE_ISO_BIG_CREATE_CMPL_EVT
     */
    struct ble_big_create_cmpl_evt_param {
        esp_bt_status_t status;     /*!< Indicate the BIG was successfully created */
        uint8_t big_handle;         /*!< The identifier of the BIG */
        uint32_t big_sync_delay;    /*!< The maximum time, in microseconds, for transmission of PDUs of all BISes in a BIG event */
        uint32_t transport_latency; /*!< The actual transport latency, in microseconds */
        uint8_t phy;                /*!< The PHY used to create the BIG */
        uint8_t nse;                /*!< The number of subevents in each BIS event in the BIG. The value shall be an integer multiple of BN */
        uint8_t bn;                 /*!< The number of new payloads in each BIS event */
        uint8_t pto;                /*!< Offset used for pre-transmissions */
        uint8_t irc;                /*!< The number of times a payload is transmitted in a BIS event */
        uint16_t max_pdu;           /*!< Maximum size, in octets, of the payload */
        uint16_t iso_interval;      /*!< The time between two consecutive BIG anchor points, Time = N × 1.25 ms */
        uint8_t num_bis;            /*!< Total number of BISes in the BIG */
        uint16_t bis_handle[ESP_BLE_MAX_BIS_NUM]; /*!< Connection handle of a BIS */
    } create_big_cmpl;              /*!< Event parameter of ESP_BLE_ISO_BIG_CREATE_CMPL_EVT */
    /**
     * @brief ESP_BLE_ISO_BIG_TERMINATE_CMPL_EVT
     */
    struct ble_big_term_cmpl_evt_param {
        esp_bt_status_t status;/*!< Indicate BIG terminate complete */
        uint8_t big_handle;    /*!< The identifier of the BIG */
        uint8_t reason;        /*!< Reason for termination. See [Vol 1] Part F, Controller Error Codes for a list of error codes and descriptions */
    } term_big_cmpl;           /*!< Event parameter of ESP_BLE_ISO_BIG_TERMINATE_CMPL_EVT */
    /**
     * @brief ESP_BLE_ISO_BIG_SYNC_ESTABLISHED_EVT
     */
    struct ble_big_sync_estab_evt_param {
        esp_bt_status_t status;     /*!< Indicate synchronization to the BIG is completed */
        uint8_t big_handle;         /*!< The identifier of the BIG */
        uint32_t transport_latency; /*!< The actual transport latency, in microseconds */
        uint8_t nse;                /*!< The number of subevents in each BIS event in the BIG */
        uint8_t bn;                 /*!< The number of new payloads in each BIS event */
        uint8_t pto;                /*!< The offset in number of ISO_Intervals for pre transmissions of data packets */
        uint8_t irc;                /*!< The number of times a payload is transmitted in a BIS event */
        uint16_t max_pdu;           /*!< The maximum size of every BIS Data PDU for every BIS in the BIG */
        uint16_t iso_interval;      /*!< The time duration between two consecutive BIG anchor points */
        uint8_t num_bis;            /*!< The total number of BISes in the BIG */
        uint16_t bis_handle[ESP_BLE_MAX_BIS_NUM]; /*!< Connection handle of a BIS in the BIG */
    } big_sync_estab;               /*!< Event parameter of ESP_BLE_ISO_BIG_SYNC_ESTABLISHED_EVT */
    /**
     * @brief ESP_BLE_ISO_BIG_SYNC_LOST_EVT
     */
    struct ble_big_sync_lost_evt_param {
        uint8_t big_handle; /*!< The identifier of a BIG */
        uint8_t reason;     /*!< The synchronization to BIG is terminated. See [Vol 1] Part F, Controller Error Codes for a list of error codes and descriptions */
    } big_sync_lost;        /*!< Event parameter of ESP_BLE_ISO_BIG_SYNC_LOST_EVT */
    /**
     * @brief ESP_BLE_ISO_BIG_SYNC_TERMINATE_CMPL_EVT
     */
    struct ble_big_sync_term_cmpl_evt_param {
        esp_bt_status_t status;     /*!< Indicate BIG sync terminate success or not */
        uint8_t big_handle;         /*!< Identifier of the BIG */
    } term_big_sync;                /*!< Event parameter of ESP_BLE_ISO_BIG_SYNC_TERMINATE_CMPL_EVT */
    /**
     * @brief ESP_BLE_ISO_BIGINFO_ADV_REPORT_EVT
     */
    struct ble_biginfo_report_evt_param {
        uint16_t sync_handle;  /*!< Identify the periodic advertising train containing the BIGInfo field */
        uint8_t num_bis;       /*!< The total number of BISes in the BIG */
        uint8_t nse;           /*!< The total number of subevents that are used to transmit BIS Data PDUs for each BIS in a BIG event */
        uint16_t iso_interval; /*!< The time duration between two consecutive BIG anchor points */
        uint8_t bn;            /*!< The number of new payloads for each BIS */
        uint8_t pto;           /*!< The offset in number of ISO_Intervals for pre transmissions of data packets */
        uint8_t irc;           /*!< The number of times the scheduled data packet is transmitted */
        uint16_t max_pdu;      /*!< The maximum size of every BIS Data PDU for every BIS in the BIG */
        uint32_t sdu_interval; /*!< The time interval of the periodic SDUs */
        uint16_t max_sdu;      /*!< The maximum size of an SDU */
        uint8_t phy;           /*!< The transmitter PHY of packets */
        uint8_t framing;       /*!< The format of the BIS Data PDUs and the mode of framed PDUs that the Controller used on the specified BIG */
        uint8_t encryption;    /*!< The encryption mode of the BISes in the BIG */
    } biginfo_report;          /*!< Event parameter of ESP_BLE_ISO_BIGINFO_ADV_REPORT_EVT */
    /**
     * @brief ESP_BLE_ISO_ISO_DATA_PATH_UPDATE_EVT
     */
    struct ble_iso_data_path_update_evt_param {
        esp_bt_status_t status;     /*!< Indicate data path update success status */
        ESP_BLE_ISO_DATA_PATH_UPDATE_TYPE op_type; /*!< data path update type, setup or remove */
        uint16_t iso_hdl;   /*!< Connection handle of the CIS or BIS */
    } data_path;            /*!< Event parameter of ESP_BLE_ISO_ISO_DATA_PATH_UPDATE_EVT */
    /**
     * @brief ESP_BLE_ISO_READ_ISO_TX_SYNC_CMPL_EVT
     */
    struct ble_iso_read_tx_sync_evt_param {
        esp_bt_status_t status;  /*!< Indicate tx sync read success status */
        uint16_t iso_hdl;       /*!< Connection handle of the CIS or BIS */
        uint16_t pkt_seq_num;   /*!< The packet sequence number of an SDU */
        uint32_t tx_time_stamp; /*!< The CIG reference point or BIG anchor point of a transmitted SDU derived using the Controller's free running reference clock (in microseconds) */
        uint32_t time_offset;   /*!< The time offset, in microseconds, that is associated with a transmitted SDU */
    } read_tx_sync;             /*!< Event parameter of ESP_BLE_ISO_READ_ISO_TX_SYNC_CMPL_EVT */
    /**
     * @brief ESP_BLE_ISO_READ_LINK_QUALITY_CMPL_EVT
     */
    struct ble_iso_read_link_quality_evt_param {
        esp_bt_status_t status;       /*!< Indicate link quality read success status */
        uint16_t iso_hdl;             /*!< The identifier of the BIS or CIS */
        uint32_t tx_unacked_pkts;     /*!< for CIS, The Link Layer does not receive an acknowledgment for a CIS Data PDU that it transmitted at least once by its flush point*/
        uint32_t tx_flushed_pkts;     /*!< for CIS, The Link Layer does not transmit a specific payload by its flush point */
        uint32_t tx_last_subevt_pkts; /*!< for CIS in Peripheral role, The Link Layer transmits a CIS Data PDU in the last subevent of a CIS event */
        uint32_t retransmitted_pkts;  /*!< for CIS, The Link Layer retransmits a CIS Data PDU */
        uint32_t crc_error_pkts;      /*!< for CIS and BIS, The Link Layer receives a packet with a CRC error */
        uint32_t rx_unreceived_pkts;  /*!< for CIS and BIS, The Link Layer does not receive a specific payload by its flush point (on a CIS) or the end of the event */
        uint32_t duplicate_pkts;      /*!< for CIS, The Link Layer receives a retransmission of a CIS Data PDU */
    } read_link_quality;              /*!< Event parameter of ESP_BLE_ISO_READ_LINK_QUALITY_CMPL_EVT */
    /**
     * @brief ESP_BLE_ISO_SET_CIG_PARAMS_CMPL_EVT
     */
    struct ble_iso_set_cig_params_evt_param {
        esp_bt_status_t status;    /*!< Indicate CIG parameters update success status */
        uint8_t cig_id;    /*!< Used to identify a CIG */
        uint8_t cis_count; /*!< Total number of CIS configurations added or modified by this command */
        uint16_t cis_hdl[ESP_BLE_MAX_CIS_NUM]; /*!< Connection handle of the CIS in the CIG */
    } set_cig_params;      /*!< Event parameter of ESP_BLE_ISO_SET_CIG_PARAMS_CMPL_EVT */
    /**
     * @brief ESP_BLE_ISO_CREATE_CIS_CMPL_EVT
     */
    struct ble_iso_create_cis_params_evt_param {
        esp_bt_status_t status; /*!< Indicate creating cis success status */
    } create_cis;               /*!< Event parameter of ESP_BLE_ISO_CREATE_CIS_CMPL_EVT */
    /**
     * @brief ESP_BLE_ISO_CIS_ESTABLISHED_EVT
     */
    struct ble_iso_cis_estab_evt_params{
        esp_bt_status_t status;    /*!< Indicate the CIS is successfully established or not */
        uint16_t cis_handle;       /*!< Connection handle of the CIS */
        uint32_t cig_sync_delay;   /*!< The maximum time, in microseconds, for transmission of PDUs of all CISes in a CIG event */
        uint32_t cis_sync_delay;   /*!< he maximum time, in microseconds, for transmission of PDUs of the speci-fied CIS in a CIG event */
        uint32_t trans_lat_c_to_p; /*!< The actual transport latency, in microseconds, from Central to Peripheral */
        uint32_t trans_lat_p_to_c; /*!< The actual transport latency, in microseconds, from Peripheral to Central */
        uint8_t phy_c_to_p;        /*!< The transmitter PHY of packets from the Central */
        uint8_t phy_p_to_c;        /*!< The transmitter PHY of packets from the Peripheral */
        uint8_t nse;               /*!< Maximum number of subevents in each CIS event */
        uint8_t bn_c_to_p;         /*!< The burst number for Central to Peripheral transmission */
        uint8_t bn_p_to_c;         /*!< The burst number for Peripheral to Central transmission */
        uint8_t ft_c_to_p;         /*!< The flush timeout, in multiples of the ISO_Interval for the CIS, for each payload sent from the Central to the Peripheral */
        uint8_t ft_p_to_c;         /*!< The flush timeout, in multiples of the ISO_Interval for the CIS, for each payload sent from the Peripheral to the Central */
        uint16_t max_pdu_c_to_p;   /*!< Maximum size, in octets, of the payload from Central to Peripheral */
        uint16_t max_pdu_p_to_c;   /*!< Maximum size, in octets, of the payload from Peripheral to Central */
        uint16_t iso_interval;     /*!< The time between two consecutive CIS anchor points, Time = N × 1.25 ms */
    } cis_estab;                   /*!< Event parameter of ESP_BLE_ISO_CIS_ESTABLISHED_EVT */
    /**
     * @brief ESP_BLE_ISO_CIS_DISCONNECTED_EVT
     */
    struct ble_iso_cis_dis_evt_params{
        uint16_t cis_handle; /*!< Connection handle of the CIS */
        uint8_t reason;      /*!< Reason for disconnection. See [Vol 1] Part F, Controller Error Codes for error codes and descriptions */
    } cis_disconnected;      /*!< Event parameter of ESP_BLE_ISO_CIS_DISCONNECTED_EVT */
    /**
     * @brief ESP_BLE_ISO_CIS_REQUEST_EVT
     */
    struct ble_iso_cis_request_evt_params{
        uint16_t acl_handle; /*!< Connection handle of the ACL */
        uint16_t cis_handle; /*!< Connection handle of the CIS */
        uint8_t cig_id;      /*!< Identifier of the CIG */
        uint8_t cis_id;      /*!< Identifier of the CIS */
    } cis_request;           /*!< Event parameter of ESP_BLE_ISO_CIS_REQUEST_EVT */
    /**
     * @brief ESP_BLE_ISO_REMOVE_CIG_CMPL_EVT
     */
    struct ble_iso_remove_cig_params{
        esp_bt_status_t status; /*!< Indicate CIG remove succeeded or not*/
        uint8_t cig_id;         /*!< Identifier of a CIG */
    } remove_cig;               /*!< Event parameter of ESP_BLE_ISO_REMOVE_CIG_CMPL_EVT */
    /**
     * @brief ESP_BLE_ISO_ACCEPT_CIS_REQ_CMPL_EVT
     */
    struct ble_iso_accept_req_evt_params{
        esp_bt_status_t status; /*!< Indicate the CIS request is accepted successfully */
    } accept_req;               /*!< Event parameter of ESP_BLE_ISO_ACCEPT_CIS_REQ_CMPL_EVT */
    /**
     * @brief ESP_BLE_ISO_REJECT_CIS_REQ_CMPL_EVT
     */
    struct ble_iso_reject_req_evt_params{
        esp_bt_status_t status; /*!< Indicate the CIS request is rejected successfully */
        uint16_t cis_handle;    /*!< The connection handle of the CIS to be rejected */
    } reject_req;               /*!< Event parameter of ESP_BLE_ISO_REJECT_CIS_REQ_CMPL_EVT */

} esp_ble_iso_cb_param_t;

/**
 * @brief ISO callback function type
 * @param event : Event type
 * @param param : Point to callback parameter, currently is union type
 */
typedef void (* esp_ble_iso_cb_t)(esp_ble_iso_cb_event_t event, esp_ble_iso_cb_param_t *param);

/**
 * @brief           This function is called to occur iso event, such as scan result
 *
 * @param[in]       callback: callback function
 *
 * @return
 *                  - ESP_OK : success
 *                  - other  : failed
 *
 */
esp_err_t esp_ble_iso_register_callback(esp_ble_iso_cb_t callback);

/**
 * @brief           This function is called to get the current iso callback
 *
 * @return
 *                  - esp_ble_iso_cb_t : callback function
 *
 */
esp_ble_iso_cb_t esp_ble_iso_get_callback(void);



/**
 * @brief           This function is called to set iso data path.
 *
 * @param[in]       data_path_params: pointer to User defined data_path_params data structure.

 * @return
 *                  - ESP_OK : success
 *                  - other  : failed
 *
 */
esp_err_t esp_ble_iso_set_iso_data_path(esp_ble_iso_set_data_path_params_t *data_path_params);

/**
 * @brief           This function is called to remove iso data path.
 *
 * @param[in]       data_path_params: pointer to User defined data_path_params data structure.

 * @return
 *                  - ESP_OK : success
 *                  - other  : failed
 *
 */
esp_err_t esp_ble_iso_remove_iso_data_path(esp_ble_iso_remove_data_path_params_t *data_path_params);

/**
 * @brief           This function is called to read tx sync.
 *
 * @param[in]       iso_handle: Connection handle of the CIS or BIS.

 *
 * @return
 *                  - ESP_OK : success
 *                  - other  : failed
 *
 */
esp_err_t esp_ble_iso_read_iso_tx_sync(uint16_t iso_handle);

/**
 * @brief           This function is called to read link quality.
 *
 * @param[in]       iso_handle: Connection handle of the CIS or BIS.

 *
 * @return
 *                  - ESP_OK : success
 *                  - other  : failed
 *
 */
esp_err_t esp_ble_iso_read_link_quality(uint16_t iso_handle);

#ifdef __cplusplus
}
#endif

#endif /* __ESP_BLE_ISO_API_H__ */
