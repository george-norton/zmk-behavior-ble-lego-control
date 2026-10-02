#pragma once
#include "hub.h"

typedef enum {
	NoErr,
	NotFound
} HubError;

HubError set_motor_speed_absolute(uint8_t port, int8_t speed);
HubError set_motor_speed_relative(uint8_t port, int8_t speed);