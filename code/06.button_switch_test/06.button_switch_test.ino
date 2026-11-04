// Find which 2 of the button's 4 terminals are connected,
// so I know where to solder the battery wire.
// Wire the terminals to D0-D3 (any order), press the button and watch Serial.

const int pins[4] = { 0, 1, 2, 3 };

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000);
}

void loop() {
  String result = "";

  // pull one pin LOW at a time and see which others follow it
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) pinMode(pins[j], INPUT_PULLUP);
    pinMode(pins[i], OUTPUT);
    digitalWrite(pins[i], LOW);
    delay(2);
    for (int j = i + 1; j < 4; j++) {
      if (digitalRead(pins[j]) == LOW) {
        result += "D" + String(pins[i]) + "-D" + String(pins[j]) + " ";
      }
    }
  }

  Serial.print("connected: ");
  Serial.println(result.length() ? result : "none");
  delay(300);
}
