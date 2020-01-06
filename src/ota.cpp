// Adapted from https://github.com/espressif/arduino-esp32/blob/master/libraries/Update/examples/AWS_S3_OTA_Update/AWS_S3_OTA_Update.ino
// by Thorsten von Eicken, 2019

#include <Arduino.h>

#include <Update.h>
#include "ota.h"

#define LED_OTA 19 // ez-sbc board
#define LED_ON   0

// define static member variables - why is C++ so awful?
long ESBOTA::contentLength;
bool ESBOTA::isValidContentType;
AsyncClient *ESBOTA::client = 0;
bool ESBOTA::gotHeader;
char ESBOTA::buf[128];
char *ESBOTA::host;
int ESBOTA::port;
char *ESBOTA::uri;
char ESBOTA::md5[34];
uint32_t ESBOTA::start;

// begin the OTA process, the payload should contain <URL>|<md5>.
void ESBOTA::begin(char *payload, size_t len) {
    if (len > 128) return;
    char *md5 = strchr(payload, '|');
    if (!md5 || (md5-payload) >= 128) return;
    *md5++ = 0;
    char mm[33]; strncpy(mm, md5, 32); mm[32] = 0; // md5 string is not null-terminated
    printf("OTA message: fetch %s MD5=%s\n", payload, mm);
    begin(payload, md5);
}

// begin the OTA process by downloading url and checking the md5.
// the md5_ string does not need to be null-terminated: it is assumed to be 32 characters long.
void ESBOTA::begin(char *url, char *md5_) {
    if (client) {
        if (millis() - start > 20*1000) {
            client->stop();
            client = 0;
        }
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
    // save hostname for HTTP request
    strcpy(buf, url+7);
    host = buf;
    // split off URI
    uri = strchr(host, '/');
    if (!uri) {
        printf("OTA: Can't find start of URI\n");
        return;
    }
    *uri = 0; uri++;
    // determine port
    char *p = strchr(host, ':');
    if (p) {
        *p = 0;
        port = atoi(p+1);
    } else {
        port = 80;
    }
    strncpy(md5, md5_, 32);
    md5[32] = 0;

    printf("OTA: Connecting to %s port %d\n", host, port);
    start = millis();

    client = new AsyncClient();
    client->onConnect(connected);
    client->onDisconnect(disconnected);
    //client->onAck(acked);
    client->onError(errored);
    client->onData(onData);
    client->onTimeout(timedout);
    isValidContentType = false;

    // Start connection
    if (!client->connect(host, port)) {
        printf("OTA: Failed to initiate connection.\n");
        delete client;
        return;
    }
}

// Utility to extract header value from headers
String ESBOTA::getHeaderValue(String header, String headerName) {
    return header.substring(strlen(headerName.c_str()));
}

// TCP connected, send HTTP request.
void ESBOTA::connected(void *obj, AsyncClient *cli) {
    printf("OTA: connected, fetching %s\n", uri);
    if (cli->space() < 512) {
        printf("OTA: not enough space in TX buffer: %d\n", cli->space());
        cli->stop(); return;
    }
    char buf[256];
    int len = snprintf(buf, 256,
            "GET /%s HTTP/1.1\r\nHost: %s\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n",
            uri, host);
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

// TCP disconnected, abort any ongoing OTA.
void ESBOTA::disconnected(void *obj, AsyncClient *cli) {
    printf("OTA: disconnected\n");
    if (Update.isRunning()) Update.abort();
    //if (client) delete client;
    client = 0;
}

void ESBOTA::timedout(void *obj, AsyncClient *cli, uint32_t time) {
    printf("OTA: timed-out\n");
    client = 0;
}

#if 0
void ESBOTA::acked(void *obj, AsyncClient *cli, size_t len, uint32_t time) {
    printf("OTA: acked\n");
}
#endif

void ESBOTA::errored(void *obj, AsyncClient *cli, int8_t error) {
    printf("OTA: errored: %d (%s)\n", error, cli->errorToString(error));
}

// got HTTP request header, verify it.
void ESBOTA::onHeader(AsyncClient *cli) {
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

// got HTTP content, add to update.
void ESBOTA::onData(void *obj, AsyncClient *cli, void *d, size_t len) {
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
                onHeader(cli);
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
        Update.setMD5(md5);
        printf("OTA: started flashing, length=%ld md5=%s\n", contentLength, md5);
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
#if LED_OTA
	    pinMode(LED_OTA, OUTPUT);
	    digitalWrite(LED_OTA, LED_ON);
#endif
            printf("\nOTA: successful! Took %.1fs. Rebooting.\n", (millis()-start)/1000.0);
#if LED_OTA
	    delay(500);
	    digitalWrite(LED_OTA, 1-LED_ON);
#endif
            ESP.restart();
        } else {
            printf("\nOTA: error %d\n", Update.getError());
            contentLength = 0;
            cli->stop();
#if LED_OTA
	    pinMode(LED_OTA, OUTPUT);
	    for (int i=0; i<10; i++) {
		digitalWrite(LED_OTA, LED_ON);
		delay(100);
		digitalWrite(LED_OTA, 1-LED_ON);
		delay(100);
	    }
#endif
        }
    }
}
