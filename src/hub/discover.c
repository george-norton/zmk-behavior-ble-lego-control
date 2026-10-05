#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/led.h>
#include "discover.h"
#include "hub.h"
#include "wedo2.h"
#include "powered_up.h"
#include "jg_jmc.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static K_MUTEX_DEFINE(discover_mutex);
#define PAIRING_TIMOUT_MS 10000

#if DT_HAS_ALIAS(led0)
#define BLINK_DURATION_MS 500
#define BLINK_BRIGHTNESS  50
#define LED_NODE          DT_ALIAS(led0)
static const struct led_dt_spec led0 = LED_DT_SPEC_GET(LED_NODE);

static int blink_count = 0;
static void led_blink_fn(struct k_timer *timer_id)
{
	if (blink_count) {
		led_set_brightness_dt(&led0, blink_count % 2 ? BLINK_BRIGHTNESS : 0);
		blink_count--;
	} else {
		k_timer_stop(timer_id);
	}
}
static void led_stop_fn(struct k_timer *timer_id)
{
	led_off_dt(&led0);
}
K_TIMER_DEFINE(blink_timer, led_blink_fn, led_stop_fn);
#endif

static struct bt_le_scan_param scan_with_timeout = {
	.type = BT_LE_SCAN_TYPE_ACTIVE,
	.options = BT_LE_SCAN_OPT_FILTER_DUPLICATE,
	.interval = BT_GAP_SCAN_FAST_INTERVAL,
	.window = BT_GAP_SCAN_FAST_WINDOW,
	.timeout = PAIRING_TIMOUT_MS / 10,
	.interval_coded = 0,
	.window_coded = 0,
};

struct behavior_ble_lego_hub_data {
	const struct device *dev;
};

hub_t hubs[MAX_HUBS] = {{}};

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			 struct net_buf_simple *ad)
{
	char dev[BT_ADDR_LE_STR_LEN] = {};
	int err = 0;
	bool hub_found = false;

	bt_addr_le_to_str(addr, dev, sizeof(dev));
	printk("[DEVICE]: %s, AD evt type %u, AD data len %u, RSSI %i: ", dev, type, ad->len, rssi);
	for (int i = 0; i < ad->len; i++) {
		printk("%02x ", ad->data[i]);
	}
	printk("\n");

	uint8_t idx = 0;
	bool found_idx = false;
	for (int i = 0; i < MAX_HUBS; i++) {
		if (hubs[i].type != None && bt_addr_eq(&hubs[i].addr, &addr->a)) {
			idx = i;
			found_idx = true;
			break;
		}
		if (!found_idx && hubs[i].type == None) {
			found_idx = true;
			idx = i;
		}
	}

	if (!found_idx) {
		LOG_INF("%s: No free slots for this hub", __func__);
		return;
	}

	/* Advertising data contains LTV values. We are
	   looking for devices with specific UUIDs. */
	if (type == 0) {
		hub_found = is_powered_up_hub(ad, &hubs[idx]) || is_wedo_hub(ad, &hubs[idx]) ||
			    is_jg_jmc_hub(ad, &hubs[idx]);
	}

	/* For now, we're only interested WeDo 2.0 and PoweredUp hubs */
	if (!hub_found) {
		LOG_INF("%s: Ignore unknown device", __func__);
		return;
	}

	bt_addr_copy(&hubs[idx].addr, &addr->a);
	LOG_INF("%s: Stop scan", __func__);
#if DT_HAS_ALIAS(led0)
	k_timer_stop(&blink_timer);
#endif
	err = bt_le_scan_stop();
	if (err) {
		LOG_ERR("%s: Stop LE scan failed (err %d)", __func__, err);
		return;
	}

	// LOG_INF("%s: Connecting to device %p (idx=%d, type=%d)", __func__, (void*)
	// hubs[idx].conn, idx, hub_type);
	err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT,
				&(hubs[idx].conn));
	if (err) {
		LOG_ERR("%s: Create conn failed (err %d)", __func__, err);
	}
	LOG_INF("< device_found");
}

static void connected(struct bt_conn *conn, uint8_t conn_err)
{
	LOG_INF("%s: Connected, conn_err = %d", __func__, conn_err);

	for (int i = 0; i < MAX_HUBS; i++) {
		if (hubs[i].conn == conn) {
			LOG_INF("%s: Hub Idx = %d, Type %d, Discovered %d", __func__, i,
				hubs[i].type, hubs[i].discovered);
			if (!hubs[i].discovered) {
				switch (hubs[i].type) {
				case WeDo_2_0:
					wedo_service_discovery(&hubs[i]);
					break;
				case PoweredUp:
					powered_up_service_discovery(&hubs[i]);
					break;
				case JG_JMC:
					jg_jmc_service_discovery(&hubs[i]);
					break;
				default:
					LOG_ERR("%s: Unexpected hub type %d", __func__,
						hubs[i].type);
					break;
				}
			}
			return;
		}
	}

	LOG_ERR("%s: Hub not found", __func__);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("%s: Disconnected. reason = %d", __func__, reason);

	for (int i = 0; i < MAX_HUBS; i++) {
		if (hubs[i].conn == conn) {
			bt_conn_unref(hubs[i].conn);
			if (reason == BT_HCI_ERR_LOCALHOST_TERM_CONN) {
				memset(&hubs[i], 0, sizeof(hub_t));
			} else {
				hubs[i].conn = NULL;
			}
		}
	}
	if (reason != BT_HCI_ERR_LOCALHOST_TERM_CONN) {
#if DT_HAS_ALIAS(led0)
		blink_count = PAIRING_TIMOUT_MS / BLINK_DURATION_MS;
		k_timer_start(&blink_timer, K_SECONDS(0), K_MSEC(BLINK_DURATION_MS));
#endif
		int err = bt_le_scan_start(&scan_with_timeout, device_found);
		if (err) {
			LOG_ERR("%s: Scanning failed to start (err %d)", __func__, err);
		}
	}
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

void scan_for_lego_hubs(void)
{
	int err;

	err = bt_enable(NULL);
	if (err && err != -EALREADY) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return;
	}
#if DT_HAS_ALIAS(led0)
	blink_count = PAIRING_TIMOUT_MS / BLINK_DURATION_MS;
	k_timer_start(&blink_timer, K_SECONDS(0), K_MSEC(BLINK_DURATION_MS));
#endif
	err = bt_le_scan_start(&scan_with_timeout, device_found);
	if (err) {
		LOG_ERR("%s: Scanning failed to start (err %d)", __func__, err);
		return;
	}

	LOG_INF("%s: Scanning successfully started", __func__);
}

void disconnect_lego_hubs(void)
{
	LOG_ERR("Disconnecting from lego hubs");
	for (int i = 0; i < MAX_HUBS; i++) {
		if (hubs[i].conn) {
			bt_conn_disconnect(hubs[i].conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
		} else {
			memset(&hubs[i], 0, sizeof(hub_t));
		}
	}
}
