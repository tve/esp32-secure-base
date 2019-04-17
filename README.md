ESP32 Base Project with Secure MQTT and OTA
===========================================

This project contains a relatively simple base firmware for the ESP32 that is
centered around MQTT and does not include an HTTP server, so there is no such
server to secure with HTTPS and passwords.

The Wifi configuration is hard-coded using a `.h` file. This could be changed to
using a WiFiManager to set-up an access point, if that's desirable.

Features:
- Asynchronous MQTT client supporting QoS 0-2 and supporting PSK TLS cipher suites
- SNTP time synchronization
- OTA triggered by MQTT message, pulls firmware using HTTP and checks it using SHA1
- Simple MQTT based time zone offset
- Periodic status info sent via MQTT

