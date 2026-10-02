#pragma once
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/kernel.h>
#include "hub.h"

#define JG_JMC_SERVICE        BT_UUID_DECLARE_16(0xfff0)
#define JG_JMC_CHARACTERISTIC BT_UUID_DECLARE_16(0xfff2)

typedef struct jg_jmc_hub {
	struct bt_gatt_discover_params discover_params;
	struct k_work_delayable send_work;
	struct bt_gatt_write_params write_params;
	uint16_t output_command_handle;
	struct k_mutex mutex;
	volatile uint8_t motor_state[4];
	volatile uint8_t motor_state_change_count;
	volatile uint8_t motor_state_sent_count;
} jg_jmc_hub_t;
