# ZMK BLE Lego control
This module provides basic control of BLE Lego hubs and behaviours to map motor controls to a keymap.
It should support most modern Lego BLE hubs, as well as WeDo 2.0 and a specific 3rd part power functions hub.

There are behaviours for pairing and disconnecting hubs, and for controlling motors (either by turning them on and off, or by adjusting their speed in increments.)

It can pair up to 8 hubs simultaneously, and control upto 64 motors.

This module was created for my [QT-1](https://github.com/george-norton/qt-1) controller, but you can use it on your own controllers or to add a Lego layer to your ZMK keyboard.