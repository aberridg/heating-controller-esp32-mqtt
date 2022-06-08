# heating-controller-esp32-mqtt

This is a fairly advanced heating controller using MQTT over WiFi. Designed for ESP32, with relays for all the heating valves.

Triggered via MQTT. Zone names are currently hard-coded.

Right now, it supports three zones, with a fourth valve for domestic hot water.

There are some bugs - sometimes it needs a reset to reconnect to WiFi or MQTT.

Open sourcing this to see if there's any interest from others in taking it further. Let me know if you find it useful, fork it, raise issues, do what you will!


