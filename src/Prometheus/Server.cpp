#include "Server.h"

#include <utility>
#include "Configuration/sensors.h"
#include "Configuration/user.h"

Prometheus::Server::Server(const int serverPort, std::shared_ptr<Metrics::Gatherer> metrics)
{
    _serverPort = serverPort;
    _server = std::make_unique<ESP8266WebServer>(_serverPort);
    _metrics = std::move(metrics);
}

Prometheus::Server::~Server()
{
    _server->stop();
    _server->close();
}

void Prometheus::Server::loop()
{
    _server->handleClient();
}

void Prometheus::Server::setup()
{
    _server->on("/", [this] { _handleRoot(); });
    _server->on("/metrics", [this] { _handleRoot(); });
    _server->onNotFound([this] { _handleNotFound(); });

    _server->begin();
    Serial.println("HTTP server started at ip " + WiFi.localIP().toString() + ":" + String(_serverPort));
}

String Prometheus::Server::_generateMetrics()
{
    String message = "";
    String idString = "{id=\"" + String(deviceId) + "\",mac=\"" + WiFi.macAddress().c_str() + "\"}";

    auto metrics = _metrics->getData();

#ifdef HAS_PM
    message += "# HELP pm02 Particulate Matter PM2.5 value\n";
    message += "# TYPE pm02 gauge\n";
    message += "pm02";
    message += idString;
    message += String(metrics.PM2);
    message += "\n";
#endif

#ifdef HAS_CO2
    message += "# HELP rco2 CO2 value, in ppm\n";
    message += "# TYPE rco2 gauge\n";
    message += "rco2";
    message += idString;
    message += String( metrics.CO2);
    message += "\n";

#endif

#ifdef HAS_SHT
    message += "# HELP atmp Temperature, in degrees Celsius\n";
    message += "# TYPE atmp gauge\n";
    message += "atmp";
    message += idString;
    message += String(metrics.TMP);
    message += "\n";

    message += "# HELP rhum Relative humidity, in percent\n";
    message += "# TYPE rhum gauge\n";
    message += "rhum";
    message += idString;
    message += String(metrics.HUM);
    message += "\n";

#endif

    return message;
}

void Prometheus::Server::_handleRoot()
{
    _server->send(200, "text/plain", _generateMetrics());
}

void Prometheus::Server::_handleNotFound()
{
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += _server->uri();
    message += "\nMethod: ";
    message += (_server->method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += _server->args();
    message += "\n";
    for (int i = 0; i < _server->args(); i++)
    {
        message += " " + _server->argName(i) + ": " + _server->arg(i) + "\n";
    }
    _server->send(404, "text/html", message);
}
