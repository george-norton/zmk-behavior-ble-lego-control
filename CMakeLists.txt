zephyr_library()

zephyr_library_sources_ifdef(CONFIG_ZMK_BEHAVIOR_BLE_LEGO_HUB src/behavior_ble_lego_hub.c src/hub/discover.c src/hub/wedo2.c src/hub/powered_up.c src/hub/jg_jmc.c)
zephyr_library_sources_ifdef(CONFIG_ZMK_BEHAVIOR_BLE_LEGO_MOTOR_CONTROL src/behavior_ble_lego_motor_control.c src/hub/interface.c)
zephyr_include_directories(include)
zephyr_include_directories(${APPLICATION_SOURCE_DIR}/include)