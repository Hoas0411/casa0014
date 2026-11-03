// Light 6 LEDs at a time and step round the lamp every 2 seconds,
// to see how the pixel numbers are laid out on the Vespera.
// Result: each group of 6 is one column, so pixel = col * 6 + row.

#include <SPI.h>
#include <WiFiNINA.h>
#include <PubSubClient.h>
#include "arduino_secrets.h"

const char* MQTT_TOPIC = "student/CASA0014/luminaire/30";
const int NUM_LEDS = 72;
const int GROUP_SIZE = 6;
byte payload[NUM_LEDS * 3];

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
String clientId;

int group = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);

  byte mac[6];
  WiFi.macAddress(mac);
  clientId = "MKR1010_30_" + String(mac[1], HEX) + String(mac[0], HEX);

  mqttClient.setServer("mqtt.cetools.org", 1884);
  mqttClient.setBufferSize(512);  // payload is 216 bytes
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWifi();
  if (!mqttClient.connected()) connectMqtt();
  mqttClient.loop();

  memset(payload, 0, sizeof(payload));
  int first = group * GROUP_SIZE;
  for (int p = first; p < first + GROUP_SIZE; p++) {
    payload[p * 3] = 100;
    payload[p * 3 + 1] = 100;
    payload[p * 3 + 2] = 100;
  }
  mqttClient.publish(MQTT_TOPIC, payload, sizeof(payload));

  Serial.print("Group ");
  Serial.print(group + 1);
  Serial.print(": LEDs ");
  Serial.print(first);
  Serial.print("-");
  Serial.println(first + GROUP_SIZE - 1);

  group = (group + 1) % (NUM_LEDS / GROUP_SIZE);
  delay(2000);
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
