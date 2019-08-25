// Adapted from https://github.com/espressif/arduino-esp32/blob/master/libraries/Update/examples/AWS_S3_OTA_Update/AWS_S3_OTA_Update.ino
// by Thorsten von Eicken, 2019

#include <AsyncTCP.h>

class ESBOTA {
public:

    // begin the OTA process, the payload should contain <URL>|<md5>.
    static void begin(char *payload, size_t len);
    // begin the OTA process by downloading url and checking the md5.
    static void begin(char *url, char *md5);

//private:

    // Variables to validate response
    static long contentLength;
    static bool isValidContentType;
    static AsyncClient *client;
    static bool gotHeader;

    static char buf[128];
    static char *host;
    static int port;
    static char *uri;
    static char md5[34];
    static uint32_t start;

    // Utility to extract header value from headers
    static String getHeaderValue(String header, String headerName);

    static void connected(void *obj, AsyncClient *cli);
    static void disconnected(void *obj, AsyncClient *cli);
    static void acked(void *obj, AsyncClient *cli, size_t len, uint32_t time);
    static void errored(void *obj, AsyncClient *cli, int8_t error);
    static void onHeader(AsyncClient *cli);
    static void onData(void *obj, AsyncClient *cli, void *d, size_t len);
    static void timedout(void *obj, AsyncClient *cli, uint32_t time);
};
