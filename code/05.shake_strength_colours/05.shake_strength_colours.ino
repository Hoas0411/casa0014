// Colour the whole lamp by how hard the sensor is shaken:
//   still = green, gentle shake = blue, strong shake = red

#include <SPI.h>
#include <Wire.h>
#include <WiFiNINA.h>
#include <PubSubClient.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "arduino_secrets.h"

const char* MQTT_TOPIC = "student/CASA0014/luminaire/30";
const int NUM_LEDS = 72;
byte payload[NUM_LEDS * 3];

const float SHAKE_THRESHOLD = 1.5;   // m/s^2 change between readings
const float STRONG_THRESHOLD = 5.0;
const byte BRIGHTNESS = 120;
const unsigned long HOLD_MS = 300;   // keep a colour this long after a shake

Adafruit_MPU6050 mpu;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
String clientId;

float lastX, lastY, lastZ;
unsigned long lastShake = 0, lastStrong = 0, lastSend = 0;
int lastColour = -1;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);

  if (!mpu.begin(0x68)) {
    Serial.println("MPU6050 not found - check wiring");
    while (true) delay(1000);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  sensors_event_t a, g, t;
  mpu.getEvent(&a, &g, &t);
  lastX = a.acceleration.x;
  lastY = a.acceleration.y;
  lastZ = a.acceleration.z;

  byte mac[6];
  WiFi.macAddress(mac);
  clientId = "MKR1010_30_" + String(mac[1], HEX) + String(mac[0], HEX);

  mqttClient.setServer("mqtt.cetools.org", 1884);
  mqttClient.setBufferSize(512);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWifi();
  if (!mqttClient.connected()) connectMqtt();
  mqttClient.loop();

  sensors_event_t a, g, t;
  mpu.getEvent(&a, &g, &t);
  float dx = a.acceleration.x - lastX;
  float dy = a.acceleration.y - lastY;
  float dz = a.acceleration.z - lastZ;
  float change = sqrt(dx * dx + dy * dy + dz * dz);
  lastX = a.acceleration.x;
  lastY = a.acceleration.y;
  lastZ = a.acceleration.z;

  unsigned long now = millis();
  if (change >= SHAKE_THRESHOLD) lastShake = now;
  if (change >= STRONG_THRESHOLD) lastStrong = now;

  // 0 = green, 1 = blue, 2 = red
  int colour = 0;
  if (lastStrong && now - lastStrong < HOLD_MS) colour = 2;
  else if (lastShake && now - lastShake < HOLD_MS) colour = 1;

  // send when the colour changes, otherwise refresh once a second
  if (colour != lastColour || now - lastSend > 1000) {
    for (int p = 0; p < NUM_LEDS; p++) {
      payload[p * 3]     = colour == 2 ? BRIGHTNESS : 0;
      payload[p * 3 + 1] = colour == 0 ? BRIGHTNESS : 0;
      payload[p * 3 + 2] = colour == 1 ? BRIGHTNESS : 0;
    }
    mqttClient.publish(MQTT_TOPIC, payload, sizeof(payload));
    lastColour = colour;
    lastSend = now;
  }

  delay(50);
}

void connectWifi() {
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting WiFi...");
    WiFi.begin(SECRET_SSID, SECRET_PASS);
    delay(3000);
  }
  Serial.println("WiFi connected");
}

void connectMqtt() {
  while (!mqttClient.connected()) {
    Serial.println("Connecting MQTT...");
    if (!mqttClient.connect(clientId.c_str(), SECRET_MQTTUSER, SECRET_MQTTPASS)) delay(2000);
  }
  Serial.println("MQTT connected");
}
