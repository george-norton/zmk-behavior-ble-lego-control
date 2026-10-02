#pragma once
#include <stdbool.h>
#include "wedo2_internal.h"
#include "powered_up_internal.h"
#include "jg_jmc_internal.h"

#define MAX_PORTS 8

typedef enum hub_type {
	None,
	WeDo_2_0,
	PoweredUp,
	JG_JMC
} hub_type_t;

typedef struct hub {
	bt_addr_t addr;
	struct bt_conn *conn;
	hub_type_t type;
	uint8_t ports;
	int8_t port_state[MAX_PORTS];
	bool discovered;
	union {
		wedo_hub_t wedo_hub;
		powered_up_hub_t powered_up_hub;
		jg_jmc_hub_t jg_jmc_hub;
	};
} hub_t;