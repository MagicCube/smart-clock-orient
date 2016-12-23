#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <ESP8266WebServer.h>

class HttpService
{
  public:
    HttpService();
    void begin();
    void loop();
    void setTemperature(int temp);

  private:
    ESP8266WebServer server;
    int temperature;
    void setupFileSystem();
};

#endif
