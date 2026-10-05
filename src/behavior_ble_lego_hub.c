/*
 * Copyright (c) 2026, George Norton
 *
 * SPDX-License-Identifier: MIT
 */
#define DT_DRV_COMPAT zmk_behavior_ble_lego_hub
#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>
#include <zmk/behavior.h>
#include "hub/discover.h"
#include <dt-bindings/behavior_ble_lego_hub.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static const struct behavior_parameter_value_metadata param_values[] = {
	{.display_name = "Pair new hub",
	 .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
	 .value = HUB_PAIR_CMD},
	{.display_name = "Disconnect all hubs",
	 .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
	 .value = HUB_DISCONNECT_CMD}};

static const struct behavior_parameter_metadata_set param_metadata_set[] = {{
	.param1_values = param_values,
	.param1_values_len = ARRAY_SIZE(param_values),
}};

static const struct behavior_parameter_metadata metadata = {
	.sets_len = ARRAY_SIZE(param_metadata_set),
	.sets = param_metadata_set,
};

#endif

static int behavior_ble_lego_hub_init(const struct device *dev)
{
	return 0;
};

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
				     struct zmk_behavior_binding_event event)
{
	return 0;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
				      struct zmk_behavior_binding_event event)
{
	switch (binding->param1) {
	case HUB_PAIR_CMD:
		scan_for_lego_hubs();
		break;
	case HUB_DISCONNECT_CMD:
		disconnect_lego_hubs();
		break;
	default:
		LOG_ERR("Unexpected Hub command %u", binding->param1);
		break;
	}
	return 0;
}

static const struct behavior_driver_api behavior_ble_lego_hub_driver_api = {
	.locality = BEHAVIOR_LOCALITY_EVENT_SOURCE,
	.binding_pressed = on_keymap_binding_pressed,
	.binding_released = on_keymap_binding_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
	.parameter_metadata = &metadata,
#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
};

#define HUB_INST(n)                                                                                \
	BEHAVIOR_DT_INST_DEFINE(n, behavior_ble_lego_hub_init, NULL, NULL, NULL, POST_KERNEL,      \
				CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                               \
				&behavior_ble_lego_hub_driver_api);

DT_INST_FOREACH_STATUS_OKAY(HUB_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */