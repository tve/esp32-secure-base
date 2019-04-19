ESP32 Base Project with Secure MQTT and OTA
===========================================

This project contains a relatively simple base firmware for the ESP32 that is
centered around MQTT and reasonably secure. It almost exclusively uses MQTTS (MQTT over TLS)
for communication for security. OTA updates are triggered over MQTTS but download the
firmware using HTTP and then check the MD5 checksum to ensure no tampering occurred
(but this does expose the firmware in the clear).

Disclaimer: I hesitated to call this firmware "secure" because I'm sure it can be hacked.
It certainly hasn't been put under serious scrutiny (and there are open issues, see below).
The goal here is to start with something reasonable as opposed to most esp8266 and esp32
software out there that mostly uses unencrypted communication.

This base project does not include an HTTP(S) server simply because that's one less thing to secure
with certificates and passwords. It means some more stuff needs to be done through MQTT than
otherwise, but perhaps that's an acceptable tradeoff.

Open issues
-----------
The main open issue is the initial configuration. Right now the Wifi and MQTT server
configs are hard-coded in the firmware. That's both inconvenient and insecure because
OTA updates happen in the clear and thus an attacker could extract the Wifi and MQTT creds.
The plan is to go through a 2-step process where the firmware contains some initial
semi-public creds that allow it to connect to the MQTT server and from which it can then
get a set of private creds.

For general use it would make sense to add an AP configuration mode with an HTTP server
or WiFiManager.
It would just have to be carefully crafted to be secure and not to use up a ton of resources.

Features
--------

- Asynchronous MQTT client supporting QoS 0-2 and supporting PSK TLS cipher suites
- SNTP time synchronization
- OTA triggered by MQTT message, pulls firmware using HTTP and checks it using SHA1
- Simple MQTT based time zone offset
- Periodic status info sent via MQTT
