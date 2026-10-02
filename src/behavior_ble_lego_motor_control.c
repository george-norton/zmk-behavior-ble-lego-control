/*
 * Copyright (c) 2026, George Norton
 *
 * SPDX-License-Identifier: MIT
 */
#define DT_DRV_COMPAT zmk_behavior_ble_lego_motor_control
#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>
#include <zmk/behavior.h>
#include "hub/discover.h"
#include "hub/interface.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

extern hub_t hubs[MAX_HUBS];

struct behavior_ble_lego_motor_control_config {
	int8_t speed;
	bool relative;
};

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static const struct behavior_parameter_value_metadata param_values[] = {
	{.display_name = "Port number",
	 .type = BEHAVIOR_PARAMETER_VALUE_TYPE_RANGE,
	 .range = {.min = 0, .max = 64}}};

static const struct behavior_parameter_metadata_set param_metadata_set[] = {{
	.param1_values = param_values,
	.param1_values_len = ARRAY_SIZE(param_values),
}};

static const struct behavior_parameter_metadata metadata = {
	.sets_len = ARRAY_SIZE(param_metadata_set),
	.sets = param_metadata_set,
};

#endif

struct behavior_ble_lego_motor_control_data {
	const struct device *dev;
};

static int behavior_ble_lego_motor_control_init(const struct device *dev)
{
	struct behavior_ble_lego_motor_control_data *data = dev->data;
	data->dev = dev;

	return 0;
};

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
				     struct zmk_behavior_binding_event event)
{
	const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
	const struct behavior_ble_lego_motor_control_config *config = dev->config;

	if (config->relative) {
		set_motor_speed_relative(binding->param1, config->speed);
	} else {
		LOG_INF("Motor port %d Speed %d\n", binding->param1, config->speed);
		set_motor_speed_absolute(binding->param1, config->speed);
	}
	return 0;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
				      struct zmk_behavior_binding_event event)
{
	const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
	const struct behavior_ble_lego_motor_control_config *config = dev->config;

	if (!config->relative) {
		set_motor_speed_absolute(binding->param1, 0);
	}
	return 0;
}

static const struct behavior_driver_api behavior_ble_lego_motor_control_driver_api = {
	.locality = BEHAVIOR_LOCALITY_EVENT_SOURCE,
	.binding_pressed = on_keymap_binding_pressed,
	.binding_released = on_keymap_binding_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
	.parameter_metadata = &metadata,
#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
};

#define MOTOR_INST(n)                                                                              \
	static struct behavior_ble_lego_motor_control_data data##n = {};                           \
	static const struct behavior_ble_lego_motor_control_config config##n = {                   \
		.speed = (int8_t)DT_PROP(DT_DRV_INST(n), speed),                                   \
		.relative = DT_PROP(DT_DRV_INST(n), relative),                                     \
	};                                                                                         \
	BEHAVIOR_DT_INST_DEFINE(n, behavior_ble_lego_motor_control_init, NULL, &data##n,           \
				&config##n, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,      \
				&behavior_ble_lego_motor_control_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MOTOR_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */