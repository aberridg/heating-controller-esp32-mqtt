# heating-controller-esp32-mqtt

This is a fairly advanced heating controller using MQTT over WiFi. Designed for ESP32, with relays for all the heating valves.

Zones are triggered via MQTT or thermostats. Zone names are currently hard-coded.

Right now, it is configured for three heating zones, with a fourth zone for domestic hot water.

Open sourcing this to see if there's any interest from others in taking it further. Let me know if you find it useful, fork it, raise issues, do what you will!

I have designed some custom hardware to go with this:

https://github.com/aberridg/heating-controller-esp32-hardware

Although, it's not quite plug n' play yet - the pin numbers are currently set for my prototype, not the finished hardware (as of Mid-June 2022, the hardware design is on order)!

