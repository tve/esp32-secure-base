ESP32 Base Project with Secure MQTT and OTA
===========================================

__WARNING__: this project is still work-in-progress, see the open issues

This project contains a relatively simple Arduino base firmware for the ESP32 that is
centered around MQTT and that attempts to be reasonably secure.
It almost exclusively uses MQTTS (MQTT over TLS) for communication.
OTA updates are triggered over MQTTS but download the
firmware using HTTP and then check the MD5 checksum to ensure no tampering occurred
(but this does expose the firmware in the clear).
Initial configuration is done using ESPAsyncWiFiManager, i.e., by running an access point and a web
server.

Disclaimer: I hesitated to call this firmware "secure" because I'm sure it can be hacked.
Also, the initial configuration is not as bullet proof as I wish it were, it just gets painful real
quick.
The firmware also hasn't been put under serious scrutiny (and there are open issues, see below).
So... why bother?
The goal here is to start with something reasonable as opposed to defaulting to wide-open
insecure unencrypted stuff.
That being said, if you see a flaw, I'd love to hear about it (kindly) and I'd love even
more to merge a fix!

Features
--------

- Initial configuration via web server over WiFi AP, includes configuration of MQTT server
  and authentication (PSK)
- Asynchronous MQTT client supporting QoS 0-2 and supporting PSK TLS cipher suites
  (https://github.com/tve/async-mqtt-client)
- OTA triggered by MQTT message, pulls firmware using HTTP and checks it using MD5
  (uses https://github.com/tve/AsyncTCP)

Open issues
-----------

- The initial configuration is not as secure as I'd like it to be: the AP is open and the web server
  is on HTTP (unencrypted). I'd like to move to a secured AP with a default passwordi (which can be
  changed in the first config run). Using the default password the communication can still be
  cracked but the hurdle is much higher than currently.
- The ESPAsyncWiFiManager does not seem to shut down the AP once Wifi is connected, this
  should be forced.
- The "reconfiguration" (i.e., starting up the AP even though the configured WiFi network can be
  joined) is broken due to a bug in AsyncTCP.
- I'd like to add a command line configuration so I don't need to mess with my laptop's WiFi to join
  the AP and figure out how to cut&paste the MQTT PSK.
- This base project does not deal with the physical security of the esp32. The chip contains
  features to encrypt the firmware and make it tamper-proof but this base project does not make any
  use of that and it may not work with the Arduino framework anyhow.

MQTT provisioning
-------------------

At boot, if there is no config saved in flash an mqtt identity is generated (it is the same as the
Wifi access point SSID) and a random PSK is also generated. Both are stored in the new config and will
persist, unless they're changed via the portal UI.

The idea is that the ident/psk can be entered into the MQTT server config, or custom ident/psk can
be generated and configured using the portal.

### Example

Erasing the entire flash to get a clean slate starting point:
```
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 --before default_reset erase_flash
```
Flashing and running the secure base example:
```
cd examples/basic
pio run -e usb -t upload -t monitor
```
When it finally boots up, this is what happens:
```
===== ESP32 Secure Base Example =====
E (55) SPIFFS: mount failed, -10025
[E][SPIFFS.cpp:52] begin(): Mounting SPIFFS failed! Error: -1
** Formatting fresh SPIFFS, takes ~20 seconds
E (60) SPIFFS: mount failed, -10025
Format took 19s
No config file, initializing mqtt ident/psk
MQTT ident=ESP-C44F330A9C35 psk=b9d4e59d1b55028d2d9ea49235dca5fa
```
The psk is not printed on subsequence boots, so it's important to capture it here.
Alternatively, it is available through the portal UI.

Wifi Observations
-----------------

The esp32-arduino WiFi class contains a number of reconnect features:
- By default the STA config is saved to flash (this can be disabled with `WiFi.persistent(false)`)
- By default the STA reconnects automatically after a disconnect (this can be disabled with
  `WiFi.autoConnect(false)`), this happens in `WiFiGenericClass::_eventCallback`.
- Need to verify that once disconnected the firmware keeps scanning to try to reconnect. (__TODO__)

There are a couple of security issues that need to be considered when using the ESPAsyncWiFiManager:
- The configuration portal must have a password otherwise an attacker can do a simple denial of
  service attack on the AP, wait for the config portal to come up and then hack the system.
  It's probably best to force setting the password using an extra parameter in the config.
- The configuration portal should take some time to come up to make attacking the portal a tad
  harder, this can be controlled using `setConnectTimeout`.

The use of the ESPAsyncWiFiManager class falls into two main use-cases: systems that are not useful
without Wifi where configuration can be blocking and where failure is best dealt with by reset, and
systems that need to operate with or without Wifi where configuration should not be blocking and
failure should not cause a reset.

Systems that cannot meaningfully operate without Wifi should:
- Set an upper bound to the configuration portal using
`ESPAsyncWiFiManager.setConfigurationPortalTimeout` and reset the system if no connection can be
established. This ensures the system doesn't get stuck. A value of 5-10 minutes
should be a reasonable compromise between usability and ensuring that the system doesn't get stuck
if unattended. If `ESPAsyncWiFiManager.autoConnect` returns false (not connected) then reset.

Systems that must operate even without Wifi should turn on the STA using `WiFi.mode(WIFI_STA)` and
then check periodically whether a connection has been established. If not, use
`ESPAsyncWiFiManager.setupConfigPortalModeless` to start the configuration portal.
Once connected, the portal should be shut down.

An alternative for systems that must operate even without Wifi is to use the blocking
`autoConnect` with an acceptable `setConnectTimeout` and accept the fact that the system won't
operate for a few minutes if the AP cannot be contacted after a reset.

