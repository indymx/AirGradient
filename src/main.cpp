/**
 * This sketch connects an AirGradient DIY sensor to a WiFi network, and runs a
 * tiny HTTP server to serve air quality metrics to Prometheus.
 */

#include "main.h"
#include <AirGradient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>

#include <Wire.h>
#include "SSD1306Wire.h"
#include <arduino-timer.h>

AirGradient ag = AirGradient();

// Config ----------------------------------------------------------------------

// Optional.
const char* deviceId = "";

// Hardware options for AirGradient DIY sensor.
const bool hasPM = true;
const bool hasCO2 = true;
const bool hasSHT = true;

// WiFi and IP connection info.
const char* ssid = "";
const char* password = "";
const int port = 9925;

// Uncomment the line below to configure a static IP address.
#define staticip
#ifdef staticip
IPAddress static_ip(192, 168, 42, 20);
IPAddress gateway(192, 168, 42, 1);
IPAddress subnet(255, 255, 255, 0);
#endif

// The frequency of measurement updates.
const int screenUpdateFrequencyMs = 5000;
const int pmSensorOnForMs = 30000;
const int pmSensorOnPeriodMs = 120000;

// For housekeeping.
int counter = 0;
int lastPM2 = 0;

// Config End ------------------------------------------------------------------

SSD1306Wire display(0x3c, SDA, SCL);
ESP8266WebServer server(port);
auto timer = timer_create_default();


void setup() {
    Serial.begin(9600);

    // Init Display.
    display.init();
    display.flipScreenVertically();
    showTextRectangle("Init", String(ESP.getChipId(),HEX),true);

    // Enable enabled sensors.
    if (hasPM) {
        ag.PMS_Init();
        WakupPM2(NULL);
        timer.every(pmSensorOnPeriodMs, WakupPM2);
    }
    if (hasCO2) ag.CO2_Init();
    if (hasSHT) ag.TMP_RH_Init(0x44);

    // Set static IP address if configured.
#ifdef staticip
    WiFi.config(static_ip,gateway,subnet);
#endif

    // Set WiFi mode to client (without this it may try to act as an AP).
    WiFi.mode(WIFI_STA);

    // Configure Hostname
    if ((deviceId != NULL) && (deviceId[0] == '\0')) {
        Serial.printf("No Device ID is Defined, Defaulting to board defaults");
    }
    else {
        wifi_station_set_hostname(deviceId);
        WiFi.setHostname(deviceId);
    }

    // Setup and wait for WiFi.
    WiFi.begin(ssid, password);
    Serial.println("");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        showTextRectangle("Trying to", "connect...", true);
        Serial.print(".");
    }

    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("MAC address: ");
    Serial.println(WiFi.macAddress());
    Serial.print("Hostname: ");
    Serial.println(WiFi.hostname());
    server.on("/", HandleRoot);
    server.on("/metrics", HandleRoot);
    server.onNotFound(HandleNotFound);

    server.begin();
    Serial.println("HTTP server started at ip " + WiFi.localIP().toString() + ":" + String(port));
    showTextRectangle("Listening To", WiFi.localIP().toString() + ":" + String(port),true);
    timer.every(screenUpdateFrequencyMs, updateScreen);
}

bool WakupPM2(void *) {
    Serial.println("Waking up PM2 sensor");
    ag.wakeUp();
    timer.in(pmSensorOnForMs, SleepPM2);
    return true;
}

bool SleepPM2(void *) {
    Serial.println("Saving current PM2 value");
    auto lastMesure = SetCurrentPM2();
    if(lastMesure !=0) {
        ag.sleep();
        Serial.println("Putting PM2 sensor to sleep");
    }
    return true;
}

void loop() {
    timer.tick();
    server.handleClient();
}

int GetCO2() {
    int stat = ag.getCO2_Raw();

    while (stat <=0 || stat >= 60000){
        Serial.println("Wrong CO2 reading: " +String(stat));
        stat = ag.getCO2_Raw();
        delay(10);
    }
    return stat;
}

int GetPM2() {
    return lastPM2;
}

int SetCurrentPM2() {
    int stat = ag.getPM2_Raw();
    if(stat>0) {
        lastPM2 = stat;
    }
    Serial.println("Set PM2 to " + String(lastPM2));
    return stat;
}

String GenerateMetrics() {
    String message = "";
    String idString = "{id=\"" + String(deviceId) + "\",mac=\"" + WiFi.macAddress().c_str() + "\"}";

    if (hasPM) {
        int stat = GetPM2();

        message += "# HELP pm02 Particulate Matter PM2.5 value\n";
        message += "# TYPE pm02 gauge\n";
        message += "pm02";
        message += idString;
        message += String(stat);
        message += "\n";
    }

    if (hasCO2) {
        int stat = GetCO2();

        message += "# HELP rco2 CO2 value, in ppm\n";
        message += "# TYPE rco2 gauge\n";
        message += "rco2";
        message += idString;
        message += String(stat);
        message += "\n";
    }

    if (hasSHT) {
        TMP_RH stat = ag.periodicFetchData();

        message += "# HELP atmp Temperature, in degrees Celsius\n";
        message += "# TYPE atmp gauge\n";
        message += "atmp";
        message += idString;
        message += String(stat.t);
        message += "\n";

        message += "# HELP rhum Relative humidity, in percent\n";
        message += "# TYPE rhum gauge\n";
        message += "rhum";
        message += idString;
        message += String(stat.rh);
        message += "\n";
    }

    return message;
}

void HandleRoot() {
    server.send(200, "text/plain", GenerateMetrics() );
}

void HandleNotFound() {
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    for (unsigned int i = 0; i < server.args(); i++) {
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    server.send(404, "text/html", message);
}

// DISPLAY
void showTextRectangle(String ln1, String ln2, boolean small) {
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    if (small) {
        display.setFont(ArialMT_Plain_16);
    } else {
        display.setFont(ArialMT_Plain_24);
    }
    display.drawString(32, 16, ln1);
    display.drawString(32, 36, ln2);
    display.display();
}

bool updateScreen(void *) {
    // Take a measurement at a fixed interval.
    switch (counter) {
        case 0:
            if (hasPM) {
                int stat = GetPM2();
                showTextRectangle("PM2",String(stat),false);
            }
            break;
        case 1:
            if (hasCO2) {
                int stat = GetCO2();
                showTextRectangle("CO2", String(stat), false);
            }
            break;
        case 2:
            if (hasSHT) {
                TMP_RH stat = ag.periodicFetchData();
                showTextRectangle("TMP", String(stat.t, 1) + "C", false);
            }
            break;
        case 3:
            if (hasSHT) {
                TMP_RH stat = ag.periodicFetchData();
                showTextRectangle("HUM", String(stat.rh) + "%", false);
            }
            break;
    }
    counter++;
    if (counter > 3) counter = 0;

    return true;
}