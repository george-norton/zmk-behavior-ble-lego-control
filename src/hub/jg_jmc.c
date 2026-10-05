#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/logging/log.h>
#include <stdlib.h>
#include "jg_jmc.h"
#include "jg_jmc_internal.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);
static struct bt_uuid_16 uuid = BT_UUID_INIT_16(0);
void send_motor_state(struct k_work *item);

bool is_jg_jmc_hub(struct net_buf_simple *ad, hub_t *hub)
{
	for (uint8_t i = 0; i < ad->len;) {
		const uint8_t length = ad->data[i++];
		const uint8_t ad_type = ad->data[i++];
		if (ad_type == 9 && length > 10) {
			static const char *JG_JMC_STR = "JG_JMC-3D34";
			if (memcmp(&ad->data[i], JG_JMC_STR, 10) == 0) {
				LOG_INF("JG_JMC Service");
				hub->type = JG_JMC;
				hub->ports = 4;

				k_work_init_delayable(&hub->jg_jmc_hub.send_work, send_motor_state);
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
	// static uint16_t start_handle = 0;
	struct bt_gatt_service_val *svc_attr = attr ? attr->user_data : NULL;
	static char service_uuid_str[BT_UUID_STR_LEN] = {};
	static char attr_uuid_str[BT_UUID_STR_LEN] = {};
	jg_jmc_hub_t *jg_jmc_hub = CONTAINER_OF(params, jg_jmc_hub_t, discover_params);
	hub_t *hub = CONTAINER_OF(jg_jmc_hub, hub_t, jg_jmc_hub);

	LOG_INF("find_handles (%d %d) %d", params ? params->start_handle : 0,
		params ? params->end_handle : 0, attr ? attr->handle : 0);

	if (!attr) {
		if (!hub->discovered) {
			// We failed to discover the motor control descriptor. So disconect.
			// Perhaps we could retry discovery instead?
			bt_conn_disconnect(hub->conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
		}
		return BT_GATT_ITER_STOP;
	}

	bt_uuid_to_str(attr->uuid, attr_uuid_str, BT_UUID_STR_LEN);
	if (svc_attr) {
		bt_uuid_to_str(svc_attr->uuid, service_uuid_str, BT_UUID_STR_LEN);
	}
	switch (params->type) {
	case BT_GATT_DISCOVER_PRIMARY: {
		LOG_INF("SVC %04x. UUID %s %s", attr ? attr->handle : -1, service_uuid_str,
			attr_uuid_str);

		if (!bt_uuid_cmp(svc_attr->uuid, JG_JMC_SERVICE)) {
			memcpy(&uuid, JG_JMC_CHARACTERISTIC, sizeof(uuid));
			params->uuid = &uuid.uuid;
			params->type = BT_GATT_DISCOVER_CHARACTERISTIC;
			params->start_handle = attr->handle + 1;
			params->end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
			LOG_INF("Look for characteristic..");

			bt_gatt_discover(conn, params);

			return BT_GATT_ITER_STOP;
		}
		break;
	}
	case BT_GATT_DISCOVER_CHARACTERISTIC: {
		LOG_INF("  CHR %04x. UUID %s %s", attr ? attr->handle : -1, service_uuid_str,
			attr_uuid_str);
		if (!bt_uuid_cmp(svc_attr->uuid, JG_JMC_CHARACTERISTIC)) {
			LOG_INF("Look for descriptor..");
			params->uuid = NULL;
			params->type = BT_GATT_DISCOVER_DESCRIPTOR;
			params->start_handle = attr->handle + 1;
			params->end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
			LOG_INF("Look for characteristic..");

			bt_gatt_discover(conn, params);

			return BT_GATT_ITER_STOP;
		}
		break;
	}
	case BT_GATT_DISCOVER_DESCRIPTOR: {
		LOG_INF("    DES %04x. UUID %s", attr ? attr->handle : -1, attr_uuid_str);
		jg_jmc_hub->output_command_handle = attr->handle;
		hub->discovered = true;
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

void jg_jmc_service_discovery(hub_t *hub)
{
	hub->jg_jmc_hub.discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
	hub->jg_jmc_hub.discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
	hub->jg_jmc_hub.discover_params.type = BT_GATT_DISCOVER_PRIMARY;
	memcpy(&uuid, JG_JMC_SERVICE, sizeof(uuid));
	hub->jg_jmc_hub.discover_params.uuid = &uuid.uuid;
	hub->jg_jmc_hub.discover_params.func = find_handles;

	if (bt_gatt_discover(hub->conn, &(hub->jg_jmc_hub.discover_params))) {
		// Queue full. Consider delaying discovery, but for now disconnect.
		bt_conn_disconnect(hub->conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	}
}

static volatile uint8_t motor_state[4] = {};

static void write_callback(struct bt_conn *conn, uint8_t err, struct bt_gatt_write_params *params)
{
	jg_jmc_hub_t *jg_jmc_hub = CONTAINER_OF(params, jg_jmc_hub_t, write_params);
	k_mutex_lock(&jg_jmc_hub->mutex, K_FOREVER);
	if (jg_jmc_hub->motor_state_sent_count != jg_jmc_hub->motor_state_change_count) {
		k_work_schedule(&(jg_jmc_hub->send_work), K_MSEC(30));
	}
	k_mutex_unlock(&jg_jmc_hub->mutex);
}

void send_motor_state(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	jg_jmc_hub_t *jg_jmc_hub = CONTAINER_OF(dwork, jg_jmc_hub_t, send_work);
	hub_t *hub = CONTAINER_OF(jg_jmc_hub, hub_t, jg_jmc_hub);
	static uint8_t motor0[] = {0x5a, 0x6b, 0x02, 0x00, 0x05, 0x00,
				   0x00, 0x00, 0x00, 0x01, 0x00};

	k_mutex_lock(&hub->jg_jmc_hub.mutex, K_FOREVER);
	for (int i = 0; i < sizeof(jg_jmc_hub->motor_state); i++) {
		motor0[i + 5] = jg_jmc_hub->motor_state[i];
	}
	hub->jg_jmc_hub.motor_state_sent_count = hub->jg_jmc_hub.motor_state_change_count;
	k_mutex_unlock(&hub->jg_jmc_hub.mutex);

	int sum = 0;
	const int checksum_pos = sizeof(motor0) - 1;
	for (int i = 0; i < checksum_pos; i++) {
		sum += motor0[i];
	}
	motor0[checksum_pos] = sum % 256;

	jg_jmc_hub->write_params.handle = jg_jmc_hub->output_command_handle;
	jg_jmc_hub->write_params.data = motor0;
	jg_jmc_hub->write_params.length = sizeof(motor0);
	jg_jmc_hub->write_params.func = write_callback, jg_jmc_hub->write_params.offset = 0,
	bt_gatt_write(hub->conn, &jg_jmc_hub->write_params);
}

void jg_jmc_set_motor_speed(hub_t *hub, uint8_t port, int8_t speed)
{
	LOG_INF("JG_JMC %p %d %d (%d) %04x\n", &(hub->jg_jmc_hub), port, speed, 0,
		hub->jg_jmc_hub.output_command_handle);
	const uint8_t pair = port / 2;
	const uint8_t direction = speed == 0 ? 0 : (speed < 0 ? 2 : 1);
	const uint8_t connector = port % 2;

	k_mutex_lock(&hub->jg_jmc_hub.mutex, K_FOREVER);
	const uint8_t pair_state =
		hub->jg_jmc_hub.motor_state[pair * 2] & ~(0x3 << (2 * connector));
	hub->jg_jmc_hub.motor_state[pair * 2] = pair_state | (direction << (2 * connector));
	if (speed == 0 && pair_state) {
		// If we released a button, but the paired port is still active
		// leave the motor on.
	} else {
		hub->jg_jmc_hub.motor_state[pair * 2 + 1] = abs(speed);
	}
	hub->jg_jmc_hub.motor_state_change_count++;
	k_mutex_unlock(&hub->jg_jmc_hub.mutex);

	k_work_schedule(&hub->jg_jmc_hub.send_work, K_MSEC(30));
}