ESP32 Base Project with Secure MQTT and OTA
===========================================

__WARNING__: this project is still work-in-progress, see the open issues

This project contains a relatively simple Arduino base firmware for the ESP32 that is
centered around MQTT and that attempts to be reasonably secure.
It almost exclusively uses MQTTS (MQTT over TLS) for communication.
OTA updates are triggered over MQTTS but download the
firmware using HTTP and then check the MD5 checksum to ensure no tampering occurred
(but this does expose the firmware in the clear).

Disclaimer: I hesitated to call this firmware "secure" because I'm sure it can be hacked.
It certainly hasn't been put under serious scrutiny (and there are open issues, see below).
The goal here is to start with something reasonable as opposed to most esp8266 and esp32
software out there that mostly uses unencrypted communication.

This base project does not include an HTTP(S) server simply because that's one less thing to secure
with certificates and passwords. It means some more stuff needs to be done through MQTT than
otherwise, but hopefully that's an acceptable tradeoff.

Open issues
-----------
The main open issue is the initial configuration. Right now the Wifi and MQTT server
configs are hard-coded in the firmware. That's both inconvenient and insecure because
OTA updates happen in the clear and thus an attacker could extract the Wifi and MQTT creds
from an update as it happens or by downloading the firmware and inspecting it.

The plan is to go through a 2-step process where the firmware contains some initial
semi-public creds that allow it to connect to the MQTT server and from which it can then
get a set of private creds which are stored on the device.

For general use it would make sense to add an AP configuration mode with an HTTP server
or WiFiManager.
It would just have to be carefully crafted to be secure and not to use up a ton of resources.

Another issue is that this base project does not deal with the physical security of the
esp32. The chip contains features to encrypt the firmware and make it tamper-proof but
this base project does not make any use of that. Maybe in the future...

Features
--------

- Asynchronous MQTT client supporting QoS 0-2 and supporting PSK TLS cipher suites
  (https://github.com/tve/async-mqtt-client)
- SNTP time synchronization
- OTA triggered by MQTT message, pulls firmware using HTTP and checks it using MD5
  (uses https://github.com/tve/AsyncTCP)
- Simple MQTT based time zone offset
- Periodic status info sent via MQTT
