import requests
import sys
from os.path import basename, splitext
#from platformio import util
from datetime import date

Import('env')

try:
    import configparser
except ImportError:
    import ConfigParser as configparser

config = configparser.ConfigParser()
config.read("platformio.ini")

#value1 = config.get("my_env", "custom_option1")
#
#ota_config = {k: v for k, v in config.get("mqtt_ota")}
#version = "2019-04-17" # project_config.get("common", "release_version")

#
# Push new firmware to the OTA storage
#

def publish_firmware(source, target, env):
    firmware_path = str(source[0])
    firmware_name = splitext(basename(firmware_path))[0]
    version = date.today().isoformat()

    print("Uploading {0} to OTA store. Version: {1}".format(
        firmware_name, version))

    url = "/".join([
        "http://core.voneicken.com:1880", "esp32-firmware",
        firmware_name,
        version
    ])
    print("URL: {0}".format(url))

    #print(env.Dump())

    headers = {
        "Content-type": "application/octet-stream",
    }
    mqtt_device = config.get("env:"+env['PIOENV'], "mqtt_device")
    if mqtt_device:
        headers["mqtt_device"] = mqtt_device
        print("OTA: command will be sent to {0}/ota".format(mqtt_device))

    r = None
    try:
        r = requests.put(url,
            data=open(firmware_path, "rb"),
            headers=headers)
        #auth=(bintray_config.get("user"), bintray_config['api_token']))
        r.raise_for_status()
    except requests.exceptions.RequestException as e:
        sys.stderr.write("Failed to submit package: %s\n" %
                ("%s\n%s" % (r.status_code, r.text) if r else str(e)))
        env.Exit(1)

    print("The firmware has been successfuly uploaded")

    #if r.status_code == 200:
    #    print("The firmware has been successfuly uploaded")
    #else:
    #    print("*** Failed to submit package: {0}\n{1}".format(r.status_code, r.text))
    #    env.Exit(1)


# Custom upload command and program name
print(env["PROJECT_DIR"])
env.Replace(
    PROGNAME=basename(env["PROJECT_DIR"]),
    UPLOADCMD=publish_firmware
)
