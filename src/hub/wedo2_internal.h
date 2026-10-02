#pragma once
#include <zephyr/bluetooth/gatt.h>
#include "hub.h"

#define WEDO_2_0_HUB_SERVICE                                                                       \
	BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0x00001523, 0x1212, 0xefde, 0x1523, 0x785feabcd123))
#define WEDO_2_0_BUTTON_STATE_CHARACTERISTIC                                                       \
	BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0x00001526, 0x1212, 0xefde, 0x1523, 0x785feabcd123))
#define WEDO_2_0_PORT_TYPE_CHARACTERISTIC                                                          \
	BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0x00001527, 0x1212, 0xefde, 0x1523, 0x785feabcd123))
#define WEDO_2_0_INPUT_VALUE_CHARACTERISTIC                                                        \
	BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0x00001560, 0x1212, 0xefde, 0x1523, 0x785feabcd123))
#define WEDO_2_0_INPUT_FORMAT_CHARACTERISTIC                                                       \
	BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0x00001561, 0x1212, 0xefde, 0x1523, 0x785feabcd123))
#define WEDO_2_0_INPUT_COMMAND_CHARACTERISTIC                                                      \
	BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0x00001563, 0x1212, 0xefde, 0x1523, 0x785feabcd123))
#define WEDO_2_0_OUTPUT_COMMAND_CHARACTERISTIC                                                     \
	BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0x00001565, 0x1212, 0xefde, 0x1523, 0x785feabcd123))
#define WEDO_2_0_INPUT_SERVICE                                                                     \
	BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0x00004f0e, 0x1212, 0xefde, 0x1523, 0x785feabcd123))

typedef struct wedo_hub {
	struct bt_gatt_discover_params discover_params;                // 20 Bytes
	struct bt_gatt_subscribe_params port_type_subscription_params; // 24 Bytes
	uint16_t output_command_handle;
} wedo_hub_t;

typedef struct port_type_notification_message {
	uint8_t port;
	uint8_t connected;
	uint8_t connect_id;
	uint8_t type;
	uint8_t mode;
	uint8_t delta_interval;
	uint8_t units;
	uint8_t notifications_enabled;
} port_type_notification_message_t;
