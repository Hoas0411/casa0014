// blue = connecting, green = connected, red = failed
void startWifi() {
  LedBlue();
  Serial.print(F("Connecting to WiFi: "));
  Serial.println(ssid);

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println(F("WiFi module not found"));
    while (true);
  }

  // try 5 times, then let loop() try again later
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 5) {
    WiFi.begin(ssid, password);
    Serial.print(F("."));
    delay(3000);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("\nWiFi connected, IP: "));
    Serial.println(WiFi.localIP());
    LedGreen();
  } else {
    Serial.println(F("\nWiFi failed, will retry"));
    LedRed();
  }
}

void reconnectMQTT() {
  LedBlue();
  wifiClient.stop();
  Serial.println(F("Connecting to MQTT..."));
  if (mqttClient.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
    Serial.println(F("connected"));
    LedGreen();
  } else {
    Serial.print(F("failed, rc="));
    Serial.println(mqttClient.state());
    LedRed();
    delay(2000);
  }
}
