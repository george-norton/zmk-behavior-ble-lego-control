#pragma once
#include "hub.h"

bool is_wedo_hub(struct net_buf_simple *ad, hub_t *hub);
void wedo_service_discovery(hub_t *hub);
void wedo_set_motor_speed(hub_t *hub, uint8_t port, int8_t speed);