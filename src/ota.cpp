// Adapted from https://github.com/espressif/arduino-esp32/blob/master/libraries/Update/examples/AWS_S3_OTA_Update/AWS_S3_OTA_Update.ino
// by Thorsten von Eicken, 2019

#include <Arduino.h>

#include <WiFi.h>
#include <AsyncMqttClient.h>
#include <Update.h>

// Variables to validate response
static long contentLength;
static bool isValidContentType = false;
static AsyncClient *client = 0;
static bool gotHeader;

static char buf[128];
static char *ota_host = buf;
static int ota_port = 1880;
static char *ota_uri = 0;
static char ota_md5[34];
static uint32_t ota_start = 0;

// Utility to extract header value from headers
String getHeaderValue(String header, String headerName) {
  return header.substring(strlen(headerName.c_str()));
}

void otaConnected(void *obj, AsyncClient *cli) {
  printf("OTA: connected, fetching %s\n", ota_uri);
  if (cli->space() < 512) {
    printf("OTA: not enough space in TX buffer: %d\n", cli->space());
    cli->stop(); return;
  }
  char buf[256];
  int len = snprintf(buf, 256,
      "GET /%s HTTP/1.1\r\nHost: %s\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n",
      ota_uri, ota_host);
  int l = cli->write(buf, len);
  if (l != len) {
    printf("OTA: only wrote %d out of %d\n", l, len);
    cli->stop(); return;
  }
  buf[0] = 0;
  contentLength = 0;
  isValidContentType = false;
  gotHeader = false;
  return;
}

void otaDisconnected(void *obj, AsyncClient *cli) {
  printf("OTA: disconnected\n");
  if (Update.isRunning()) Update.abort();
  //if (client) delete client;
  client = 0;
}

void otaAcked(void *obj, AsyncClient *cli, size_t len, uint32_t time) {
  printf("OTA: acked\n");
}

void otaErrored(void *obj, AsyncClient *cli, int8_t error) {
  printf("OTA: errored: %d (%s)\n", error, cli->errorToString(error));
}

void otaGotHeader(AsyncClient *cli) {
  int bufLen = strlen(buf);
  if (strncmp(buf, "HTTP/1.1", 8) == 0) {
    if (bufLen < 12 || strncmp(buf+9, "200", 3) != 0) {
      printf("OTA: did not get 200 status code: %s\n", buf);
      cli->stop();
    }
  } else if (strncmp(buf, "Content-Length: ", 16) == 0) {
    contentLength = atol(buf+16);
    if (contentLength < 1024 || contentLength > 8*1024*1024) {
      printf("OTA: invalid Content-Length: %ld\n", contentLength);
      cli->stop();
      contentLength = 0;
    }
  } else if (strncmp(buf, "Content-Type: ", 14) == 0) {
    if (strcmp(buf+14, "application/octet-stream") != 0) {
      printf("OTA: invalid Content-Type: %s\n", buf+14);
      cli->stop();
    } else {
      isValidContentType = true;
    }
  }
}

void otaGotData(void *obj, AsyncClient *cli, void *d, size_t len) {
  char *data = (char *)d;
  //printf("OTA: got data (%d)\n", len);
  if (!gotHeader) {
    // reading HTTP response headers
    int bufLen = strlen(buf);
    int i = 0; // index into data
    while (i<len && bufLen<128) {
      if (data[i] == '\n') {
        // got a header line
        if (bufLen == 0 || (bufLen == 1 && buf[0] == '\r')) {
          // end of headers
          if (contentLength == 0) {
            printf("OTA: Content-Length header missing\n");
            cli->stop();
            return;
          }
          bufLen = 0;
          gotHeader = true;
          i++;
          break;
        }
        buf[bufLen-1] = 0;
        otaGotHeader(cli);
        buf[0] = 0;
        bufLen = 0;
      } else if (data[i] == 0) {
        // should not find null character
        printf("OTA: got null character in HTTP response headers\n");
        cli->stop();
        return;
      } else {
        // ordinary character
        buf[bufLen++] = data[i];
      }
      i++;
    }
    // check whether we popped out fo the loop due to oversize header
    if (bufLen == 128) {
      buf[127] = 0;
      printf("OTA: got too long a header line: %s\n", buf);
      cli->stop();
      return;
    }
    buf[bufLen] = 0;
    if (!gotHeader && i == len) return;
    if (!gotHeader) printf("OTA: whooops???\n");
    if (contentLength == 0) { printf("OTA: ignoring...\n"); return; }
    if (!isValidContentType) { contentLength = 0; return; }
    // start update
    if (Update.isRunning()) Update.abort();
    bool canBegin = Update.begin(contentLength);
    if (!canBegin) {
      printf("OTA: not enough space to perform OTA\n");
      cli->stop();
      return;
    }
    Update.setMD5(ota_md5);
    printf("OTA: started flashing, length=%ld md5=%s\n", contentLength, ota_md5);
    if (i < len) {
      size_t w = Update.write((uint8_t*)(data+i), len-i);
      if (w != len-i) {
        printf("OTA: write failed, wrote %d expected %d\n", w, len-i);
        contentLength = 0;
        cli->stop();
      }
    }
    return;
  }
  // plain data
  if (contentLength == 0) { printf("OTA: ignoring...\n"); return; }
  size_t w = Update.write((uint8_t*)data, len);
  if (w != len) {
    printf("OTA: write failed, wrote %d expected %d\n", w, len);
    contentLength = 0;
    cli->stop();
  } else {
    Serial.print('~');
  }
  if (Update.isFinished()) {
    if (Update.end()) {
      printf("\nOTA: successful! Took %.1fs. Rebooting.\n", (millis()-ota_start)/1000.0);
      ESP.restart();
    } else {
      printf("\nOTA: error %d\n", Update.getError());
      contentLength = 0;
      cli->stop();
    }
  }
}

void otaTimedout(void *obj, AsyncClient *cli, uint32_t time) {
  printf("OTA: timed-out\n");
  client = 0;
}

void startOTA(char *url, char *md5) {
  if (client) {
    printf("OTA: Fetch in progress, not starting new one\n");
    return;
  }
  if (strlen(url) > 127) {
    printf("OTA: URL %s too long\n", url);
    return;
  }
  if (strncmp(url, "http://", 7) != 0) {
    printf("OTA: URL must start with http:// (%s)\n", url);
    return;
  }
  strcpy(buf, url+7);
  ota_host = buf;
  // split off URI
  ota_uri = strchr(ota_host, '/');
  if (!ota_uri) {
    printf("OTA: Can't find start of URI\n");
    return;
  }
  *ota_uri = 0; ota_uri++;
  // determine port
  char *p = strchr(ota_host, ':');
  if (p) {
    *p = 0;
    ota_port = atoi(p+1);
  } else {
    ota_port = 80;
  }
  strncpy(ota_md5, md5, 33);

  printf("OTA: Connecting to %s port %d\n", ota_host, ota_port);
  ota_start = millis();

  client = new AsyncClient();
  client->onConnect(otaConnected);
  client->onDisconnect(otaDisconnected);
  client->onAck(otaAcked);
  client->onError(otaErrored);
  client->onData(otaGotData);
  client->onTimeout(otaTimedout);

  // Start connection
  if (!client->connect(ota_host, ota_port)) {
    printf("OTA: Failed to initiate connection.\n");
    delete client;
    return;
  }
}
