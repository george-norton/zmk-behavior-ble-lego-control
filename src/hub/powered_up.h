#pragma once
#include "hub.h"

bool is_powered_up_hub(struct net_buf_simple *ad, hub_t *hub);
void powered_up_service_discovery(hub_t *hub);
void powered_up_set_motor_speed(hub_t *hub, uint8_t port, int8_t speed);