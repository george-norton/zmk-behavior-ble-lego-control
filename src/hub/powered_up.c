#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/logging/log.h>
#include "powered_up.h"
#include "powered_up_internal.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);
static struct bt_uuid_128 uuid = BT_UUID_INIT_128(0);

// Only the TechnicControlPlusHub has been tested.
typedef enum {
	DuploTrainHub5 = 32,
	DuploTrainHub16 = 33,
	BoostMoveHub = 64,
	PoweredUp2PortSmartHub = 65,
	TechnicControlPlusHub = 128,
	SpikePrimeHub = 129,
	SpikeEssentialsHub = 131,
	TechnicMoveHub = 132
} ProductType;

bool is_powered_up_hub(struct net_buf_simple *ad, hub_t *hub)
{
	uint8_t product_type = 0;
	for (uint8_t i = 0; i < ad->len;) {
		const uint8_t length = ad->data[i++];
		const uint8_t ad_type = ad->data[i++];
		if ((ad_type == 6 || ad_type == 7) && length == BT_UUID_SIZE_128 + 1) {
			struct bt_uuid_128 service_uuid;
			bt_uuid_create(&service_uuid.uuid, &ad->data[i], BT_UUID_SIZE_128);
			if (!bt_uuid_cmp(&service_uuid.uuid, POWERED_UP_HUB_SERVICE)) {
				hub->type = PoweredUp;
				LOG_INF("PoweredUp Service\n");
			}
		} else if (ad_type == 0xff && length >= 3) {
			printk("[MANUFACTURER DATA] len=%d (%02x) ", length, ad->data[i + 3]);
			for (int j = 0; j < length; j++) {
				printk("%02x ", ad->data[i + j]);
			}
			printk("\n");

			product_type = ad->data[i + 3];
		} else if (length <= 1) {
			LOG_ERR("Invalid Length\n");
			return false;
		}
		i += (length - 1);
	}
	if (hub->type == PoweredUp) {
		switch (product_type) {
		case DuploTrainHub5:
		case DuploTrainHub16:
			hub->ports = 1;
			break;
		case SpikeEssentialsHub:
		case PoweredUp2PortSmartHub:
			hub->ports = 2;
			break;
		case TechnicMoveHub:
			hub->ports = 3;
			break;
		case BoostMoveHub:
		case TechnicControlPlusHub:
			hub->ports = 4;
			break;
		case SpikePrimeHub:
			hub->ports = 6;
			break;
		default:
			hub->ports = 1;
			break;
		}
		LOG_INF("Returning Type %d Ports %d\n", hub->type, hub->ports);
		return true;
	}
	return false;
}

static uint8_t find_handles(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			    struct bt_gatt_discover_params *params)
{
	// static uint16_t start_handle = 0;
	struct bt_gatt_service_val *svc_attr = attr ? attr->user_data : NULL;
	static char service_uuid_str[BT_UUID_STR_LEN] = {};
	static char attr_uuid_str[BT_UUID_STR_LEN] = {};
	powered_up_hub_t *powered_up_hub = CONTAINER_OF(params, powered_up_hub_t, discover_params);
	hub_t *hub = CONTAINER_OF(powered_up_hub, hub_t, powered_up_hub);

	if (!attr) {
		if (!hub->discovered) {
			// We failed to discover the motor control descriptor. So disconect.
			// Perhaps we could retry discovery instead?
			bt_conn_disconnect(hub->conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
		}
		return BT_GATT_ITER_STOP;
	}

	LOG_INF("find_handles (%d %d) %d", params ? params->start_handle : 0,
		params ? params->end_handle : 0, attr ? attr->handle : 0);

	bt_uuid_to_str(attr->uuid, attr_uuid_str, BT_UUID_STR_LEN);
	if (svc_attr) {
		bt_uuid_to_str(svc_attr->uuid, service_uuid_str, BT_UUID_STR_LEN);
	}
	switch (params->type) {
	case BT_GATT_DISCOVER_PRIMARY: {
		LOG_INF("SVC %04x. UUID %s %s", attr ? attr->handle : -1, service_uuid_str,
			attr_uuid_str);

		if (!bt_uuid_cmp(svc_attr->uuid, POWERED_UP_HUB_SERVICE)) {
			params->start_handle = attr->handle + 1;
			params->end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
			params->type = BT_GATT_DISCOVER_CHARACTERISTIC;
			memcpy(&uuid, LEGO_HUB_CHARACTERISTIC, sizeof(uuid));
			params->uuid = &uuid.uuid;

			LOG_INF("Discover characteristic %d", params->start_handle);
			bt_gatt_discover(conn, params);
			return BT_GATT_ITER_STOP;
		}
		break;
	}
	case BT_GATT_DISCOVER_CHARACTERISTIC: {
		LOG_INF("  CHR %04x. UUID %s %s", attr ? attr->handle : -1, service_uuid_str,
			attr_uuid_str);
		if (!bt_uuid_cmp(svc_attr->uuid, LEGO_HUB_CHARACTERISTIC)) {
			LOG_INF("Lego hub handle %p %p %04x", hub, params, attr->handle);
			powered_up_hub->output_command_handle = attr->handle + 1;
			hub->discovered = true;
		}
		break;
	}
	case BT_GATT_DISCOVER_DESCRIPTOR: {
		LOG_INF("    DES %04x. UUID %s", attr ? attr->handle : -1, attr_uuid_str);
		return BT_GATT_ITER_CONTINUE;
		break;
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

void powered_up_service_discovery(hub_t *hub)
{
	hub->powered_up_hub.discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
	hub->powered_up_hub.discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
	hub->powered_up_hub.discover_params.type = BT_GATT_DISCOVER_PRIMARY;
	memcpy(&uuid, POWERED_UP_HUB_SERVICE, sizeof(uuid));
	hub->powered_up_hub.discover_params.uuid = &uuid.uuid;
	hub->powered_up_hub.discover_params.func = find_handles;

	if (bt_gatt_discover(hub->conn, &(hub->powered_up_hub.discover_params))) {
		// Queue full. Consider delaying discovery, but for now disconnect.
		bt_conn_disconnect(hub->conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	}
}

void powered_up_set_motor_speed(hub_t *hub, uint8_t port, int8_t speed)
{
	LOG_INF("PoweredUp %p %d %d (%d) %04x\n", &(hub->powered_up_hub), port, speed, 0,
		hub->powered_up_hub.output_command_handle);
	uint8_t motor0[] = {0x08, 0x00, 0x81, /*0x32 +*/ port, 0x11, 0x51, 0x00, speed};
	bt_gatt_write_without_response(hub->conn, hub->powered_up_hub.output_command_handle, motor0,
				       sizeof(motor0), false);
}