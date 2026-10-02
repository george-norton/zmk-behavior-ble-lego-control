#pragma once
#include <zephyr/bluetooth/gatt.h>
#include "hub.h"

#define POWERED_UP_HUB_SERVICE                                                                     \
	BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0x00001623, 0x1212, 0xefde, 0x1623, 0x785feabcd123))
#define LEGO_HUB_CHARACTERISTIC                                                                    \
	BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0x00001624, 0x1212, 0xefde, 0x1623, 0x785feabcd123))

typedef struct powered_up_hub {
	struct bt_gatt_discover_params discover_params; // 20 Bytes
	uint16_t output_command_handle;
} powered_up_hub_t;
