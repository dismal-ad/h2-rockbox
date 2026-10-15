/******************************************************************************
 *
 *  Copyright (C) 2014 Google, Inc.
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
#include <stdbool.h>
#include "common/bt_target.h"
#include "common/bt_trace.h"
#include "device/bdaddr.h"
#include "stack/bt_types.h"
#include "device/controller.h"
#include "device/event_mask.h"
#include "stack/hcimsgs.h"
#include "hci/hci_layer.h"
#include "hci/hci_packet_factory.h"
#include "hci/hci_packet_parser.h"
#include "stack/btm_ble_api.h"
#include "device/version.h"
#include "osi/future.h"
#include "config/stack_config.h"
const bt_event_mask_t BLE_EVENT_MASK = { "\x00\x00\x00\x00\x00\x00\x06\x7f" };

const bt_event_mask_t CLASSIC_EVENT_MASK = { HCI_LISBON_EVENT_MASK_EXT };

// TODO(zachoverflow): factor out into common module
const uint8_t SCO_HOST_BUFFER_SIZE = 0xff;

#define HCI_SUPPORTED_COMMANDS_ARRAY_SIZE 64
#define MAX_FEATURES_CLASSIC_PAGE_COUNT 3
#define BLE_SUPPORTED_STATES_SIZE         8
#define BLE_SUPPORTED_FEATURES_SIZE       8
#define BLE_EXT_ADV_DATA_LEN_MAX          1650

typedef struct {
    const hci_t *hci;
    const hci_packet_factory_t *packet_factory;
    const hci_packet_parser_t *packet_parser;

    bt_version_t bt_version;
    bt_bdaddr_t address;

    uint8_t supported_commands[HCI_SUPPORTED_COMMANDS_ARRAY_SIZE];
    uint8_t last_features_classic_page_index;
    bt_device_features_t features_classic[MAX_FEATURES_CLASSIC_PAGE_COUNT];

    uint16_t acl_data_size_classic;
    uint16_t acl_data_size_ble;
    uint16_t acl_buffer_count_classic;
    uint8_t acl_buffer_count_ble;

    uint8_t sco_data_size;
    uint16_t sco_buffer_count;

    uint8_t ble_white_list_size;
    uint8_t ble_resolving_list_max_size;
    uint8_t ble_supported_states[BLE_SUPPORTED_STATES_SIZE];
    bt_device_features_t features_ble;
    uint16_t ble_suggested_default_data_length;
    uint16_t ble_suggested_default_data_txtime;

    bool readable;
    bool ble_supported;
    bool simple_pairing_supported;
    bool secure_connections_supported;
} controller_local_param_t;

static controller_local_param_t controller_param;

#define AWAIT_COMMAND(command) future_await(controller_param.hci->transmit_command_futured(command))

static bool loaded = false;
// Module lifecycle functions

static void start_up(void)
{
    BT_HDR *response;

    // Send the initial reset command
    response = AWAIT_COMMAND(controller_param.packet_factory->make_reset());
    controller_param.packet_parser->parse_generic_command_complete(response);

    // Request the classic buffer size next
    response = AWAIT_COMMAND(controller_param.packet_factory->make_read_buffer_size());
    controller_param.packet_parser->parse_read_buffer_size_response(
        response, &controller_param.acl_data_size_classic, &controller_param.acl_buffer_count_classic,
        &controller_param.sco_data_size, &controller_param.sco_buffer_count);

    // Enable controller to host flow control
    response = AWAIT_COMMAND(controller_param.packet_factory->make_set_c2h_flow_control(HCI_HOST_FLOW_CTRL_ACL_ON));
    controller_param.packet_parser->parse_generic_command_complete(response);
    // Tell the controller about our buffer sizes and buffer counts next
    // TODO(zachoverflow): factor this out. eww l2cap contamination. And why just a hardcoded 10?
    response = AWAIT_COMMAND(
                   controller_param.packet_factory->make_host_buffer_size(
                       L2CAP_MTU_SIZE,
                       SCO_HOST_BUFFER_SIZE,
                       L2CAP_HOST_FC_ACL_BUFS,
                       10
                   )
               );

    controller_param.packet_parser->parse_generic_command_complete(response);

    // Read the local version info off the controller next, including
    // information such as manufacturer and supported HCI version
    response = AWAIT_COMMAND(controller_param.packet_factory->make_read_local_version_info());
    controller_param.packet_parser->parse_read_local_version_info_response(response, &controller_param.bt_version);

    // Read the bluetooth address off the controller next
    response = AWAIT_COMMAND(controller_param.packet_factory->make_read_bd_addr());
    controller_param.packet_parser->parse_read_bd_addr_response(response, &controller_param.address);

    // Request the controller's supported commands next
    response = AWAIT_COMMAND(controller_param.packet_factory->make_read_local_supported_commands());
    controller_param.packet_parser->parse_read_local_supported_commands_response(
        response,
        controller_param.supported_commands,
        HCI_SUPPORTED_COMMANDS_ARRAY_SIZE
    );

    // Read page 0 of the controller features next
    uint8_t page_number = 0;
    response = AWAIT_COMMAND(controller_param.packet_factory->make_read_local_extended_features(page_number));
    controller_param.packet_parser->parse_read_local_extended_features_response(
        response,
        &page_number,
        &controller_param.last_features_classic_page_index,
        controller_param.features_classic,
        MAX_FEATURES_CLASSIC_PAGE_COUNT
    );

    assert(page_number == 0);
    page_number++;

    // Inform the controller what page 0 features we support, based on what
    // it told us it supports. We need to do this first before we request the
    // next page, because the controller's response for page 1 may be
    // dependent on what we configure from page 0 and host SSP configuration
    controller_param.simple_pairing_supported = HCI_SIMPLE_PAIRING_SUPPORTED(
                                                    controller_param.features_classic[0].as_array) &&
                                                (bluedroid_config_get()->get_ssp_enabled());
    if (controller_param.simple_pairing_supported) {
        response = AWAIT_COMMAND(controller_param.packet_factory->make_write_simple_pairing_mode(HCI_SP_MODE_ENABLED));
        controller_param.packet_parser->parse_generic_command_complete(response);
    }

    // Done telling the controller about what page 0 features we support
    // Request the remaining feature pages
    while (page_number <= controller_param.last_features_classic_page_index &&
            page_number < MAX_FEATURES_CLASSIC_PAGE_COUNT) {
        response = AWAIT_COMMAND(controller_param.packet_factory->make_read_local_extended_features(page_number));
        controller_param.packet_parser->parse_read_local_extended_features_response(
            response,
            &page_number,
            &controller_param.last_features_classic_page_index,
            controller_param.features_classic,
            MAX_FEATURES_CLASSIC_PAGE_COUNT
        );

        page_number++;
    }

if ((bluedroid_config_get()->get_sc_enabled())) {
    controller_param.secure_connections_supported = HCI_SC_CTRLR_SUPPORTED(controller_param.features_classic[2].as_array);
    if (controller_param.secure_connections_supported) {
        response = AWAIT_COMMAND(controller_param.packet_factory->make_write_secure_connections_host_support(HCI_SC_MODE_ENABLED));
        controller_param.packet_parser->parse_generic_command_complete(response);
    }
}

    response = AWAIT_COMMAND(controller_param.packet_factory->make_set_event_mask(&CLASSIC_EVENT_MASK));
    controller_param.packet_parser->parse_generic_command_complete(response);

    controller_param.readable = true;
    // return future_new_immediate(FUTURE_SUCCESS);
    return;
}

static void shut_down(void)
{
    controller_param.readable = false;
}

static bool get_is_ready(void)
{
    return controller_param.readable;
}

static const bt_bdaddr_t *get_address(void)
{
    assert(controller_param.readable);
    return &controller_param.address;
}

static const bt_version_t *get_bt_version(void)
{
    assert(controller_param.readable);
    return &controller_param.bt_version;
}

// TODO(zachoverflow): hide inside, move decoder inside too
static const bt_device_features_t *get_features_classic(int index)
{
    assert(controller_param.readable);
    assert(index < MAX_FEATURES_CLASSIC_PAGE_COUNT);
    return &controller_param.features_classic[index];
}

static uint8_t get_last_features_classic_index(void)
{
    assert(controller_param.readable);
    return controller_param.last_features_classic_page_index;
}

static const bt_device_features_t *get_features_ble(void)
{
    assert(controller_param.readable);
    assert(controller_param.ble_supported);
    return &controller_param.features_ble;
}

static const uint8_t *get_ble_supported_states(void)
{
    assert(controller_param.readable);
    assert(controller_param.ble_supported);
    return controller_param.ble_supported_states;
}

static bool supports_simple_pairing(void)
{
    assert(controller_param.readable);
    return controller_param.simple_pairing_supported;
}

static bool supports_secure_connections(void)
{
    assert(controller_param.readable);
    return controller_param.secure_connections_supported;
}

static bool supports_simultaneous_le_bredr(void)
{
    assert(controller_param.readable);
    return HCI_SIMUL_LE_BREDR_SUPPORTED(controller_param.features_classic[0].as_array);
}

static bool supports_reading_remote_extended_features(void)
{
    assert(controller_param.readable);
    return HCI_READ_REMOTE_EXT_FEATURES_SUPPORTED(controller_param.supported_commands);
}

static bool supports_interlaced_inquiry_scan(void)
{
    assert(controller_param.readable);
    return HCI_LMP_INTERLACED_INQ_SCAN_SUPPORTED(controller_param.features_classic[0].as_array);
}

static bool supports_rssi_with_inquiry_results(void)
{
    assert(controller_param.readable);
    return HCI_LMP_INQ_RSSI_SUPPORTED(controller_param.features_classic[0].as_array);
}

static bool supports_extended_inquiry_response(void)
{
    assert(controller_param.readable);
    return HCI_EXT_INQ_RSP_SUPPORTED(controller_param.features_classic[0].as_array);
}

static bool supports_master_slave_role_switch(void)
{
    assert(controller_param.readable);
    return HCI_SWITCH_SUPPORTED(controller_param.features_classic[0].as_array);
}

static bool supports_ble(void)
{
    assert(controller_param.readable);
    return controller_param.ble_supported;
}

static bool supports_ble_privacy(void)
{
    assert(controller_param.readable);
    assert(controller_param.ble_supported);
    return HCI_LE_ENHANCED_PRIVACY_SUPPORTED(controller_param.features_ble.as_array);
}

static bool supports_ble_packet_extension(void)
{
    assert(controller_param.readable);
    assert(controller_param.ble_supported);
    return HCI_LE_DATA_LEN_EXT_SUPPORTED(controller_param.features_ble.as_array);
}

static bool supports_ble_connection_parameters_request(void)
{
    assert(controller_param.readable);
    assert(controller_param.ble_supported);
    return HCI_LE_CONN_PARAM_REQ_SUPPORTED(controller_param.features_ble.as_array);
}

static uint16_t get_acl_data_size_classic(void)
{
    assert(controller_param.readable);
    return controller_param.acl_data_size_classic;
}

static uint16_t get_acl_data_size_ble(void)
{
    assert(controller_param.readable);
    assert(controller_param.ble_supported);
    return controller_param.acl_data_size_ble;
}

static uint16_t get_acl_packet_size_classic(void)
{
    assert(controller_param.readable);
    return controller_param.acl_data_size_classic + HCI_DATA_PREAMBLE_SIZE;
}

static uint16_t get_acl_packet_size_ble(void)
{
    assert(controller_param.readable);
    return controller_param.acl_data_size_ble + HCI_DATA_PREAMBLE_SIZE;
}

static uint16_t get_ble_suggested_default_data_length(void)
{
    assert(controller_param.readable);
    assert(controller_param.ble_supported);
    return controller_param.ble_suggested_default_data_length;
}

static uint16_t get_ble_suggested_default_data_txtime(void)
{
    assert(controller_param.readable);
    assert(controller_param.ble_supported);
    return controller_param.ble_suggested_default_data_txtime;
}

static uint16_t get_acl_buffer_count_classic(void)
{
    assert(controller_param.readable);
    return controller_param.acl_buffer_count_classic;
}

static uint8_t get_acl_buffer_count_ble(void)
{
    assert(controller_param.readable);
    assert(controller_param.ble_supported);
    return controller_param.acl_buffer_count_ble;
}

static uint8_t get_ble_white_list_size(void)
{
    assert(controller_param.readable);
    assert(controller_param.ble_supported);
    return controller_param.ble_white_list_size;
}

static uint8_t get_ble_resolving_list_max_size(void)
{
    assert(controller_param.readable);
    assert(controller_param.ble_supported);
    return controller_param.ble_resolving_list_max_size;
}

static void set_ble_resolving_list_max_size(int resolving_list_max_size)
{
    assert(controller_param.readable);
    assert(controller_param.ble_supported);
    controller_param.ble_resolving_list_max_size = resolving_list_max_size;
}

static const controller_t interface = {
    start_up,
    shut_down,
    get_is_ready,

    get_address,
    get_bt_version,

    get_features_classic,
    get_last_features_classic_index,

    get_features_ble,
    get_ble_supported_states,

    supports_simple_pairing,
    supports_secure_connections,
    supports_simultaneous_le_bredr,
    supports_reading_remote_extended_features,
    supports_interlaced_inquiry_scan,
    supports_rssi_with_inquiry_results,
    supports_extended_inquiry_response,
    supports_master_slave_role_switch,

    supports_ble,
    supports_ble_packet_extension,
    supports_ble_connection_parameters_request,
    supports_ble_privacy,

    get_acl_data_size_classic,
    get_acl_data_size_ble,

    get_acl_packet_size_classic,
    get_acl_packet_size_ble,
    get_ble_suggested_default_data_length,
    get_ble_suggested_default_data_txtime,

    get_acl_buffer_count_classic,
    get_acl_buffer_count_ble,

    get_ble_white_list_size,

    get_ble_resolving_list_max_size,
    set_ble_resolving_list_max_size,
};

const controller_t *controller_get_interface(void)
{
    if (!loaded) {
        loaded = true;
        controller_param.hci = hci_layer_get_interface();
        controller_param.packet_factory = hci_packet_factory_get_interface();
        controller_param.packet_parser = hci_packet_parser_get_interface();
    }

    return &interface;
}
