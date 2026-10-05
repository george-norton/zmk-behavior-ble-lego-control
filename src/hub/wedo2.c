#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/logging/log.h>
#include "wedo2.h"
#include "wedo2_internal.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);
static struct bt_uuid_128 uuid = BT_UUID_INIT_128(0);

bool is_wedo_hub(struct net_buf_simple *ad, hub_t *hub)
{
	for (uint8_t i = 0; i < ad->len;) {
		const uint8_t length = ad->data[i++];
		const uint8_t ad_type = ad->data[i++];
		LOG_INF("WeDo %d %d", ad_type, length);
		if ((ad_type == 6 || ad_type == 7) && length == BT_UUID_SIZE_128 + 1) {
			struct bt_uuid_128 service_uuid;
			bt_uuid_create(&service_uuid.uuid, &ad->data[i], BT_UUID_SIZE_128);
			if (!bt_uuid_cmp(&service_uuid.uuid, WEDO_2_0_HUB_SERVICE)) {
				hub->type = WeDo_2_0;
				hub->ports = 2;
				LOG_INF("WeDo Service");
				return true;
			}
		} else if (length <= 1) {
			LOG_ERR("Invalid Length\n");
			return false;
		}
		i += (length - 1);
	}
	return false;
}

static uint8_t find_handles(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			    struct bt_gatt_discover_params *params)
{
	struct bt_gatt_service_val *svc_attr = attr ? attr->user_data : NULL;
	char service_uuid_str[BT_UUID_STR_LEN] = {};
	char attr_uuid_str[BT_UUID_STR_LEN] = {};
	wedo_hub_t *wedo_hub = CONTAINER_OF(params, wedo_hub_t, discover_params);
	hub_t *hub = CONTAINER_OF(wedo_hub, hub_t, wedo_hub);
	if (!attr) {
		if (!hub->discovered) {
			// We failed to discover the motor control descriptor. So disconect.
			// Perhaps we could retry discovery instead?
			bt_conn_disconnect(hub->conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
		}
		return BT_GATT_ITER_STOP;
	}
	bt_uuid_to_str(attr->uuid, attr_uuid_str, BT_UUID_STR_LEN);
	bt_uuid_to_str(svc_attr->uuid, service_uuid_str, BT_UUID_STR_LEN);

	switch (params->type) {
	case BT_GATT_DISCOVER_PRIMARY: {
		LOG_INF("SVC %04x. UUID %s %s", attr ? attr->handle : -1, service_uuid_str,
			attr_uuid_str);

		if (!bt_uuid_cmp(svc_attr->uuid, WEDO_2_0_HUB_SERVICE)) {
			memcpy(&uuid, WEDO_2_0_OUTPUT_COMMAND_CHARACTERISTIC, sizeof(uuid));
			params->uuid = &uuid.uuid;
			params->type = BT_GATT_DISCOVER_CHARACTERISTIC;
			return BT_GATT_ITER_CONTINUE;
		}
		break;
	}
	case BT_GATT_DISCOVER_CHARACTERISTIC: {
		LOG_INF("  CHR %04x. UUID %s %s", attr ? attr->handle : -1, service_uuid_str,
			attr_uuid_str);
		if (!bt_uuid_cmp(svc_attr->uuid, WEDO_2_0_OUTPUT_COMMAND_CHARACTERISTIC)) {
			params->type = BT_GATT_DISCOVER_DESCRIPTOR;
			return BT_GATT_ITER_CONTINUE;
		}
		break;
	}
	case BT_GATT_DISCOVER_DESCRIPTOR: {
		LOG_INF("    DES %04x. UUID %s", attr ? attr->handle : -1, attr_uuid_str);
		wedo_hub->output_command_handle = attr->handle;
		hub->discovered = true;
		return BT_GATT_ITER_CONTINUE;
	}
	default:
		break;
	}
	if (!hub->discovered) {
		// We failed to discover the motor control descriptor. So disconect.
		// Perhaps we could retry discovery instead?
		bt_conn_disconnect(hub->conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	}
	return BT_GATT_ITER_STOP;
}

void wedo_service_discovery(hub_t *hub)
{
	hub->wedo_hub.discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
	hub->wedo_hub.discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
	hub->wedo_hub.discover_params.type = BT_GATT_DISCOVER_PRIMARY;
	memcpy(&uuid, WEDO_2_0_HUB_SERVICE, sizeof(uuid));
	hub->powered_up_hub.discover_params.uuid = &uuid.uuid;
	hub->wedo_hub.discover_params.func = find_handles;
	LOG_INF("WeDo Hub %p Params %p %p", &(hub->wedo_hub), &(hub->wedo_hub.discover_params),
		&(hub->wedo_hub.port_type_subscription_params));

	if (bt_gatt_discover(hub->conn, &(hub->wedo_hub.discover_params))) {
		// Queue full. Consider delaying discovery, but for now disconnect.
		bt_conn_disconnect(hub->conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	}
}

void wedo_set_motor_speed(hub_t *hub, uint8_t port, int8_t speed)
{
	uint8_t motor0[] = {port + 1, 0x01, 0x01, speed};
	LOG_INF("WeDo %p %d %d (%d)\n", &(hub->wedo_hub), port, speed,
		hub->wedo_hub.output_command_handle);
	bt_gatt_write_without_response(hub->conn, hub->wedo_hub.output_command_handle, motor0,
				       sizeof(motor0), false);
}