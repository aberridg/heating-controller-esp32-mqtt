# heating-controller-esp32-mqtt

This is a fairly advanced heating controller using MQTT over WiFi. Designed for ESP32, with relays for all the heating valves.

Zones are triggered via MQTT or thermostats. Zone names are currently hard-coded.

Right now, it is configured for three heating zones, with a fourth zone for domestic hot water.

Open sourcing this to see if there's any interest from others in taking it further. Let me know if you find it useful, fork it, raise issues, do what you will!

I have designed some custom hardware to go with this:

https://github.com/aberridg/heating-controller-esp32-hardware

Notes for me:

Flashing the board over USB doesn't work without using a hub, and a USB-C to USB-C cable.

My main laptop doesn't allow esptool to upload OTA.

Solution is to copy the generated binary to a Linux machine, run: 

wget https://raw.githubusercontent.com/espressif/arduino-esp32/master/tools/espota.py

(to get the ESP OTA tool)

And then:

python3 ~andrew/espota.py -d -i 192.168.whatever -f /mnt/wherever/heating_control.ino.bin





