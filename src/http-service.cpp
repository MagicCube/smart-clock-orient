#include "http-service.h"

#include <ESP8266WebServer.h>
#include <FS.h>

HttpService::HttpService() : server(80)
{

}

void HttpService::begin()
{
    this->setupFileSystem();

    server.on("/api/data", HTTP_GET, [ this ]() {
        server.send(200, "application/json", "{\"temperature\":" + String(this->temperature) + "}");
    });
    server.serveStatic("/", SPIFFS, "/index.html");
    server.begin();
}

void HttpService::loop()
{
    server.handleClient();
}

void HttpService::setTemperature(int temp)
{
    this->temperature = temp;
}

void HttpService::setupFileSystem()
{
    SPIFFS.begin();
    Dir dir = SPIFFS.openDir("/");
    while (dir.next())
    {
        String fileName = dir.fileName();
        size_t fileSize = dir.fileSize();
        Serial.printf("File: %s, size: %s\n", fileName.c_str(), String(fileSize).c_str());
    }
    Serial.printf("\n");
}
