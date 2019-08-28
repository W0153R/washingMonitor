#include <Arduino.h>
#include "I2Cdev.h"
#include "MPU6050.h"
#include "Wire.h"
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <BlynkSimpleEsp8266.h>

#define DEBUG false
//#define BLYNK_PRINT Serial

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

// Vibration threshold necessary to change from IDLE to MAYBE_ON.
#define WASHER_THRESHOLD 750
// How often a machine's vibration score should exceed the threshold to be considered ON.
#define TIME_WINDOW 3000
// How long a machine needs to spend (in ms) in each of the MAYBE states until we move it to the next state. These should always be more than 3000ms.
#define TIME_UNTIL_ON   30000  // 30 seconds
#define TIME_UNTIL_DONE 600000 // 10 minutes

#define HOSTNAME "washingMonitor"
char blynkAuth[] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXX";

enum ApplianceState {
  IDLE, // Nothing is happening.
  MAYBE_ON, // We had a vibration event. We think the appliance may be on, but we're not sure yet.
  ON, // Enough vibration has happened in a short time that we're ready to proclaim that the appliance is on.
  MAYBE_DONE, // It stopped moving. Maybe it's done?
  DONE  // It's been silent for long enough that we're sure it's done.
};

ApplianceState state = IDLE;
long lastIdleTime = millis();
long lastActiveTime = millis() - (TIME_WINDOW + 100);

MPU6050 accelgyro;
int16_t ax, ay, az;
int16_t gx, gy, gz;
int16_t initialAccel[3];
int16_t initialGyro[3];

void updateState() {
  int16_t newAccel[3];
  int16_t newGyro[3];
  int32_t total = 0;
  accelgyro.getMotion6(&newAccel[0], &newAccel[1], &newAccel[2], &newGyro[0], &newGyro[1], &newGyro[2]);
  for (uint8_t i = 0; i < 3; i++) {
    newAccel[i] = initialAccel[i] - newAccel[i];
    newGyro[i] = initialGyro[i] - newGyro[i];
    total += newAccel[i] < 0 ? -1*newAccel[i] : newAccel[i];
    total += newGyro[i] < 0 ? -1*newGyro[i] : newGyro[i];
  }

  if (DEBUG) {
    Serial.print("state: "); Serial.print(state);
    Serial.print(" total: "); Serial.println(total);
  }

  long now = millis();

  if (total > WASHER_THRESHOLD) {
    lastActiveTime = now;
  }

  // "Recently" active means our force exceeded the threshold at least once within the last three seconds.
  bool wasRecentlyActive = (now - lastActiveTime) < TIME_WINDOW;

  switch (state) {
    case IDLE:
      if (wasRecentlyActive) {  // Whenever there's so much as a twitch, we switch to the MAYBE_ON state.
        state = MAYBE_ON;
      } else {
        lastIdleTime = now;
      }
      break;
    case MAYBE_ON:
      if (wasRecentlyActive) {
        if (now > (lastIdleTime + TIME_UNTIL_ON)) { // Long enough that this is not a false alarm.
          state = ON;
        }
      } else {
        state = IDLE;
      }
      break;
    case ON:
      if (!wasRecentlyActive) {  // We stopped vibrating. Are we off? Switch to MAYBE_DONE so we can figure it out.
        state = MAYBE_DONE;
      }
      break;
    case MAYBE_DONE:
      if (wasRecentlyActive) {  // We thought we were done, but we're vibrating now. False alarm! Go back to ON.
        state = ON;
      } else if (now > (lastActiveTime + TIME_UNTIL_DONE)) {  // We've been idle for long enough that we're certain that the cycle has stopped.
        state = DONE;
      }
      break;
    case DONE:
      if (DEBUG) { Serial.println("Done"); }
      Blynk.notify("De was is klaar!");
      state = IDLE;
      break;
  }
}

void setup() {
  Wire.begin();
  if (DEBUG) { Serial.begin(115200); }
  delay(500);
  Serial.setDebugOutput(DEBUG);
  accelgyro.initialize();
  delay(500);

  if (!accelgyro.testConnection()) {
    delay(1000);
    ESP.restart();
    delay(1000);
  }
  accelgyro.setXAccelOffset(755);
  accelgyro.setYAccelOffset(891);
  accelgyro.setZAccelOffset(1507);
  accelgyro.setXGyroOffset(59);
  accelgyro.setYGyroOffset(-27);
  accelgyro.setZGyroOffset(0);
  delay(500);
  accelgyro.getMotion6(&initialAccel[0], &initialAccel[1], &initialAccel[2], &initialGyro[0], &initialGyro[1], &initialGyro[2]);

  WiFi.hostname(HOSTNAME);
  WiFiManager wifiManager;
  wifiManager.setDebugOutput(DEBUG);
  if (!wifiManager.autoConnect(HOSTNAME)) {
    delay(1000);
    ESP.restart();
    delay(1000);
  }
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }

  if (DEBUG) {
    Serial.println();
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }

  if ( !MDNS.begin(HOSTNAME) ) {
    if (DEBUG) {
      Serial.print("Error! Failed to broadcast hostname via MDNS: ");
      Serial.println(HOSTNAME);
    }
    delay(1000);
    ESP.restart();
    delay(1000);
  }
  httpUpdater.setup(&server, "/", "admin", "XXXXXX");
  server.begin();
  MDNS.addService("http", "tcp", 80);

  Blynk.config(blynkAuth);
  Blynk.connect();
  while (!Blynk.connected()) {
    delay(100);
  }
  delay(5000);
  Blynk.notify("washingMonitor booted");

  if (DEBUG) { Serial.println("Ready!"); }
}

void loop() {
  server.handleClient();
  Blynk.run();
  updateState();
  delay(50);
}
