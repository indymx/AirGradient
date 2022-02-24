#include "user.h"

//screen refresh
const int screenUpdateFrequencyMs = 5000;

//Id of the device for Prometheus
const char * deviceId = "testboard";

//Wifi information
const char* ssid = "Quickinet";
const char* password = "8929242969";
const uint16_t port = 9925;

#ifdef staticip
IPAddress static_ip(192, 168, 42, 20);
IPAddress gateway(192, 168, 42, 1);
IPAddress subnet(255, 255, 255, 0);
#endif

const char* ntp_server = "pool.ntp.org";