#include <IPAddress.h>

const char* ssid = "Duyen Pham 1";
const char* password = "99999999";
const IPAddress hostIP(192, 168, 1, 5);
const char* hostPort = "5000";
const String serverName = "http://" + hostIP.toString() + ":" + hostPort + "/upload";