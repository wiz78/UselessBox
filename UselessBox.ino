#include <ezButton.h>
#include <ESP32Servo.h>

#define OTA_ENABLED 0

const int switchPin = 12;
const int ledPin = 16;
const int motorPin = 18;

ezButton topSwitch(switchPin);

Servo servo;
const int restingPos = 160;
const int peekingPos = 80;
const int intermediatePos = 60;
const int pushingPos = 41;

unsigned int lastActionMillis = 0;
bool firstAction = true;
bool ledOn = false;

void simple();
void slow();
void verySlow();
void slowPeek();
void angry();
void peeking();
typedef void (*actionHandler)();

const actionHandler actions[] = {
  simple,
  slow,
  verySlow,
  slowPeek,
  angry,
  peeking,

  simple,
  simple,
  simple,
  simple,
  slowPeek,
  peeking,
  peeking,
  angry,
  angry,
  angry,
};

constexpr int actionsCount = sizeof( actions ) / sizeof( actions[0] );

#if OTA_ENABLED
// based on https://randomnerdtutorials.com/esp32-over-the-air-ota-programming/

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

#include "wifi_config.h"

const char *host = "uselessBox";
const char *otaPage = 
"<html>"
"<head>"
"<title>useless box OTA</title>"
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"</head>"
"<body>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')" 
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>"
 "</body>"
 "</html>";

WebServer server(80);
#endif

void setup()
{
#if OTA_ENABLED
  Serial.begin(115200);

  WiFi.begin(ssid, password);

  for(int i = 0; ( i < 20 ) && ( WiFi.status() != WL_CONNECTED ); i++) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) {
    Serial.println("Error setting up MDNS responder!");
  }

  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", otaPage);
  });

  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });

  /* set servo position, useful to calibrate the motor */
  server.on("/servo", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", "OK\n");
    
    if(server.hasArg("pos")) {
      servo.write(server.arg("pos").toInt());
      delay(300);
    }
  });

  server.on("/action", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", "OK\n");
    
    if(server.hasArg("num")) {
      int requestedAction = server.arg("num").toInt();

      actions[ requestedAction % ( actionsCount + 1 ) ]();
    } else if(server.hasArg("peek"))
      peekAfterwards();
  });

  server.begin();
#endif

  topSwitch.setDebounceTime(100);

  pinMode(ledPin, OUTPUT);
  setLed(false);

  servo.setPeriodHertz(50);
  servo.attach(motorPin);
  servo.write(restingPos);
}

void loop() 
{
#if OTA_ENABLED
  server.handleClient();
#endif

  topSwitch.loop();

  if(topSwitch.getState() == LOW) {
    int action, maxAction;
    unsigned long sinceLastAction = millis() - lastActionMillis;
    
    if(!lastActionMillis)
      srandom(millis());

    if(sinceLastAction > 60 * 5 * 1000)
      firstAction = true;

    if(firstAction)
      action = 0;
    else
      action = random(0, actionsCount - 1);

    actions[ action ]();

    if(!firstAction && (sinceLastAction < 5000) && (random(1, 10) > 6))
      peekAfterwards();

    lastActionMillis = millis();
    firstAction = false;
  } 
  
  if(ledOn)
    setLed(false);

  delay(20);
}

int stepFromTo(int from, int to)
{
  return (to > from) ? 1 : -1;
}

void simple()
{
  servo.write(pushingPos);
  delay(250);
  servo.write(restingPos);
}

void slow()
{
  for(int i = restingPos; i != pushingPos; i += stepFromTo(restingPos, pushingPos)) {
    servo.write(i);
    delay(10);
  }

  servo.write(intermediatePos);
  delay(100);

  servo.write(pushingPos);

  delay(100);

  servo.write(restingPos);
}

void verySlow()
{
  for(int i = restingPos; i != pushingPos; i += stepFromTo(restingPos, pushingPos)) {
    servo.write(i);
    delay(30);
  }

  servo.write(intermediatePos);
  delay(100);

  servo.write(pushingPos);

  delay(150);

  servo.write(restingPos);
}

void slowPeek()
{
  for(int i = restingPos; i != peekingPos; i += stepFromTo(restingPos, peekingPos)) {
    servo.write(i);
    delay(10);
  }
    
  servo.write(peekingPos);

  delay(500);
  servo.write(pushingPos);
  delay(250);

  servo.write(restingPos);
}

void angry()
{
  setLed(true);

  for(int i = 0; i < 3; i++) {
    servo.write(pushingPos);
    delay(250);
    servo.write(peekingPos);
    delay(200);
  }

  servo.write(restingPos);
  delay(200);
}

void peeking()
{
  servo.write(peekingPos);
  setLed(true);
  delay(250);
  servo.write(pushingPos);
  delay(250);
  servo.write(restingPos);
  delay(200);
}

void peekAfterwards()
{
  setLed(true);
  delay(100);
  servo.write(peekingPos);
  delay(1500);
  servo.write(restingPos);
}

void setLed(bool on)
{
  digitalWrite(ledPin, on ? HIGH : LOW);

  ledOn = on;
}
