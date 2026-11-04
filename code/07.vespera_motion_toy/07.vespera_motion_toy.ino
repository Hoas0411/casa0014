// Vespera motion toy - MKR WiFi 1010 + MPU6050 in a small box on a string
//   spin it round in a circle -> a lit column chases round the 12 columns
//   bob it up and down        -> a lit row chases through the 6 rows
//   leave it still            -> the whole lamp glows dim
// The three looks fade into each other instead of jumping.
// Vespera pixel index = col * 6 + row

#include <SPI.h>
#include <Wire.h>
#include <WiFiNINA.h>
#include <PubSubClient.h>
#include <utility/wifi_drv.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "arduino_secrets.h"

const char* ssid          = SECRET_SSID;
const char* password      = SECRET_PASS;
const char* mqtt_username = SECRET_MQTTUSER;
const char* mqtt_password = SECRET_MQTTPASS;
const char* mqtt_server   = "mqtt.cetools.org";
const int   mqtt_port     = 1884;

String lightId    = "30";
String mqtt_topic = "student/CASA0014/luminaire/" + lightId;
String clientId   = "";

WiFiClient   wifiClient;
PubSubClient mqttClient(wifiClient);

const int NUM_COLS     = 12;
const int NUM_ROWS     = 6;
const int payload_size = NUM_COLS * NUM_ROWS * 3;
byte RGBpayload[payload_size];

Adafruit_MPU6050 mpu;

// tuning
const unsigned long SAMPLE_MS    = 10;   // read sensor at 100 Hz
const unsigned long FRAME_MS     = 50;   // send 20 frames a second
const float FREEFALL_G           = 0.35; // below this the box is in the air
const unsigned long TOSS_HOLD_MS = 700;
const float SPIN_DPS             = 120;  // yaw rate that counts as spinning
const float STILL_DEV            = 0.04;
const unsigned long SWITCH_MS    = 250;  // new state must last this long
const float BLEND_TAU            = 0.35; // fade time in seconds
const float TRAIL                = 2.5;  // tail length of the chase
const float COL_SPEED_SCALE      = 0.5;
const float ROW_SPEED_MIN        = 10;   // rows per second
const float ROW_SPEED_MAX        = 24;

const byte STILL_RGB[3] = { 40, 30, 20 };   // dim warm white
const byte SPIN_RGB[3]  = { 0, 180, 255 };  // cyan
const byte TOSS_RGB[3]  = { 255, 60, 150 }; // pink

enum Motion { STILL, SPIN, TOSS };
Motion state = STILL;
Motion candidate = STILL;
unsigned long candidateSince = 0;

float magLP = 1.0;  // smoothed |a| in g
float devLP = 0.0;  // how shaky |a| is
float yawLP = 0.0;  // smoothed spin speed, deg/s
unsigned long lastFreefall = 0;

float colPhase = 0, rowPhase = 0;
float colSpeed = 0, rowSpeed = 0;
float wStill = 1, wSpin = 0, wToss = 0;  // how much of each look to show

unsigned long lastSample = 0, lastFrame = 0;

void setup() {
  Serial.begin(115200);

  WiFiDrv::pinMode(25, OUTPUT);
  WiFiDrv::pinMode(26, OUTPUT);
  WiFiDrv::pinMode(27, OUTPUT);
  LedRed();

  if (!mpu.begin()) {
    Serial.println(F("IMU not found - check SDA/SCL wiring"));
    while (true) { LedRed(); delay(200); LedOff(); delay(200); }
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);  // spinning can go over 2g
  mpu.setGyroRange(MPU6050_RANGE_1000_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  byte mac[6];
  WiFi.macAddress(mac);
  char macStr[13];
  snprintf(macStr, sizeof(macStr), "%02X%02X%02X%02X%02X%02X", mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
  clientId = "MKR1010_Toy_" + String(macStr);

  startWifi();
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setBufferSize(512);  // payload is 216 bytes
  mqttClient.setKeepAlive(30);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) startWifi();
  if (!mqttClient.connected()) reconnectMQTT();
  mqttClient.loop();

  // two timers instead of delay() so reading and sending don't block each other
  unsigned long now = millis();
  if (now - lastSample >= SAMPLE_MS) {
    lastSample = now;
    readSensor(now);
    updateState(now);
  }
  if (now - lastFrame >= FRAME_MS) {
    float dt = (now - lastFrame) / 1000.0;
    lastFrame = now;
    animate(dt);
    render();
    publishFrame();
  }
}

void readSensor(unsigned long now) {
  sensors_event_t a, g, t;
  mpu.getEvent(&a, &g, &t);

  float ax = a.acceleration.x / 9.81, ay = a.acceleration.y / 9.81, az = a.acceleration.z / 9.81;
  float mag = sqrt(ax * ax + ay * ay + az * az);
  magLP += 0.05 * (mag - magLP);
  devLP += 0.05 * (fabs(mag - magLP) - devLP);

  // the sensor's z axis points along the string, so gyro z = spin speed
  float yaw = g.gyro.z * 57.2958;  // rad/s -> deg/s
  yawLP += 0.1 * (yaw - yawLP);

  if (mag < FREEFALL_G) lastFreefall = now;
}

Motion classify(unsigned long now) {
  bool airborne = now - lastFreefall < TOSS_HOLD_MS;
  bool spinning = fabs(yawLP) > SPIN_DPS;
  bool bobbing  = devLP > 0.3 && !spinning;

  if (airborne || bobbing) return TOSS;
  if (spinning) return SPIN;
  if (devLP < STILL_DEV && fabs(magLP - 1.0) < 0.08) return STILL;
  return state;  // not sure, keep the current state
}

void updateState(unsigned long now) {
  Motion c = classify(now);
  if (c != candidate) { candidate = c; candidateSince = now; }
  // a toss is obvious so switch straight away, others wait SWITCH_MS
  if (candidate != state && (candidate == TOSS || now - candidateSince > SWITCH_MS)) {
    state = candidate;
    Serial.println(state == STILL ? F("STILL") : state == SPIN ? F("SPIN") : F("TOSS"));
  }
}

void animate(float dt) {
  float k = 1.0 - exp(-dt / BLEND_TAU);

  // one turn of the box = one trip round the 12 columns (scaled down)
  float colTarget = state == SPIN ? yawLP / 360.0 * NUM_COLS * COL_SPEED_SCALE : 0;
  float rowTarget = 0;
  if (state == TOSS) rowTarget = constrain(mapf(devLP, 0.1, 0.8, ROW_SPEED_MIN, ROW_SPEED_MAX), ROW_SPEED_MIN, ROW_SPEED_MAX);

  // ease the speed too, so the light slides to a stop
  colSpeed += k * (colTarget - colSpeed);
  rowSpeed += k * (rowTarget - rowSpeed);
  colPhase = wrapf(colPhase + colSpeed * dt, NUM_COLS);
  rowPhase = wrapf(rowPhase + rowSpeed * dt, NUM_ROWS);

  wStill += k * ((state == STILL ? 1 : 0) - wStill);
  wSpin  += k * ((state == SPIN  ? 1 : 0) - wSpin);
  wToss  += k * ((state == TOSS  ? 1 : 0) - wToss);
}

// brightness (0-1) of a cell, with the head of the chase at `phase`
float chase(float pos, float phase, int n) {
  float behind = wrapf(phase - pos, n);
  return behind > TRAIL ? 0 : 1.0 - behind / TRAIL;
}

void render() {
  for (int col = 0; col < NUM_COLS; col++) {
    float cSpin = chase(col, colPhase, NUM_COLS) * wSpin;
    for (int row = 0; row < NUM_ROWS; row++) {
      float cToss = chase(row, rowPhase, NUM_ROWS) * wToss;
      int i = (col * NUM_ROWS + row) * 3;
      for (int ch = 0; ch < 3; ch++) {
        // keep at least 30% of the dim glow so the lamp never goes dark
        float v = STILL_RGB[ch] * max(wStill, 0.3f) + SPIN_RGB[ch] * cSpin + TOSS_RGB[ch] * cToss;
        RGBpayload[i + ch] = (byte)constrain((int)v, 0, 255);
      }
    }
  }
}

void publishFrame() {
  if (mqttClient.connected()) mqttClient.publish(mqtt_topic.c_str(), RGBpayload, payload_size);
}

float wrapf(float x, float n) { x = fmod(x, n); return x < 0 ? x + n : x; }
float mapf(float x, float a, float b, float c, float d) { return c + (x - a) * (d - c) / (b - a); }
