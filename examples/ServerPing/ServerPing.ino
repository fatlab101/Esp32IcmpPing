//
// Modified a simple server sample implementation to add Ping
// Via: http:\\<IP>/ping
//

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Esp32IcmpPing.h>
#include <FixedString.h>

AsyncWebServer server(80);

const char* ssid = "SSID";
const char* password = "PW1234";
const char* PARAM_MESSAGE = "message";


//Hard code Google DNS 
IPAddress google(8,8,4,4);
//Set recv timeout to 1/2 second and Count to 4
Esp32IcmpPing pingClient(google, 4, 500);  

 void callPing(String* s=nullptr) 
{
    PingResults results;
    if(pingClient.ping(results, &Serial))
    {
      results.PrintState(&Serial);
      if(s!=nullptr)
        *s=results.ResultString(true);
    }
    else
    {
       if(s!=nullptr)
          *s="Failed Ping!";
    }
}

void pingRequest(AsyncWebServerRequest *request) 
{
    String s;
    callPing(&s);
    request->send(404, "text/plain", s.c_str());
}

void notFound(AsyncWebServerRequest *request) 
{
    request->send(404, "text/plain", "Not found");
}

void setup() 
{
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    if (WiFi.waitForConnectResult() != WL_CONNECTED) 
    {
        Serial.println("WiFi Failed!");
        return;
    }
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.println("Ping Options:");
    pingClient.Options().PrintState(&Serial);
  
    server.on("/", HTTP_GET, 
	[](AsyncWebServerRequest *request)
	{
	request->send(200, "text/plain", "Hello, world");
	});

    // Send a GET request to <IP>/get?message=<message>
    server.on("/get", HTTP_GET, 
	[] (AsyncWebServerRequest *request) 
	{
        String message;
        if (request->hasParam(PARAM_MESSAGE)) 
            message = request->getParam(PARAM_MESSAGE)->value();
        else 
            message = "No message sent";
        request->send(200, "text/plain", "Hello, GET: " + message);
    });

    // Send a POST request to <IP>/post with a form field message set to <message>
    server.on("/post", HTTP_POST, 
	[](AsyncWebServerRequest *request)
	{
        String message;
        if (request->hasParam(PARAM_MESSAGE, true)) 
            message = request->getParam(PARAM_MESSAGE, true)->value();
         else 
            message = "No message sent";
        request->send(200, "text/plain", "Hello, POST: " + message);
    });

    // Send a GET ping request to <IP>/ping
    server.on("/ping", HTTP_GET, pingRequest);
    server.onNotFound(notFound);
    server.begin();

    //Do a ping!
    String s;
    callPing(&s);
    Serial.println(s);
}

void loop() 
{
}
