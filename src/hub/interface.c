#include <zephyr/logging/log.h>
#include "hub.h"
#include "discover.h"
#include "interface.h"
#include "wedo2.h"
#include "powered_up.h"
#include "jg_jmc.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);
#define CLIP(x, l, h) (x < l ? l : x > h ? h : x)
extern hub_t hubs[MAX_HUBS];

static HubError get_hub(uint8_t port, hub_t **hub, uint8_t *hub_port)
{
	for (int i = 0; i < MAX_HUBS; i++) {
		if (port < hubs[i].ports) {
			*hub = &hubs[i];
			*hub_port = port;
			return NoErr;
		}
		port -= hubs[i].ports;
	}
	return NotFound;
}

HubError set_motor_speed_absolute(uint8_t port, int8_t speed)
{
	hub_t *hub = NULL;
	uint8_t hub_port = 0;
	HubError err = get_hub(port, &hub, &hub_port);
	LOG_INF("Motor err %d hub_port %d\n", err, hub_port);

	if (err == NoErr && hub->discovered) {
		hub->port_state[hub_port] = speed;
		if (hub->type == WeDo_2_0) {
			wedo_set_motor_speed(hub, hub_port, speed);
		}
		if (hub->type == PoweredUp) {
			powered_up_set_motor_speed(hub, hub_port, speed);
		}
		if (hub->type == JG_JMC) {
			jg_jmc_set_motor_speed(hub, hub_port, speed);
		}
	}
	return err;
}

HubError set_motor_speed_relative(uint8_t port, int8_t speed)
{
	hub_t *hub = NULL;
	uint8_t hub_port = 0;
	HubError err = get_hub(port, &hub, &hub_port);
	if (err == NoErr && hub->discovered) {
		hub->port_state[hub_port] = CLIP(hub->port_state[hub_port] + speed, -100, 100);
		if (hub->type == WeDo_2_0) {
			wedo_set_motor_speed(hub, hub_port, hub->port_state[hub_port]);
		}
		if (hub->type == PoweredUp) {
			powered_up_set_motor_speed(hub, hub_port, hub->port_state[hub_port]);
		}
		if (hub->type == JG_JMC) {
			jg_jmc_set_motor_speed(hub, hub_port, hub->port_state[hub_port]);
		}
	}
	return err;
}