// First motion test: compare each reading with the last one
// and print SHAKE when the change is big enough.

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

Adafruit_MPU6050 mpu;

const float SHAKE_THRESHOLD = 3.5;  // m/s^2

float lastX, lastY, lastZ;
bool shaken = false;
unsigned long lastShake = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  if (!mpu.begin(0x68)) {
    Serial.println("MPU6050 not found");
    while (true) delay(100);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  sensors_event_t a, g, t;
  mpu.getEvent(&a, &g, &t);
  lastX = a.acceleration.x;
  lastY = a.acceleration.y;
  lastZ = a.acceleration.z;
}

void loop() {
  sensors_event_t a, g, t;
  mpu.getEvent(&a, &g, &t);

  float dx = a.acceleration.x - lastX;
  float dy = a.acceleration.y - lastY;
  float dz = a.acceleration.z - lastZ;
  float change = sqrt(dx * dx + dy * dy + dz * dz);

  lastX = a.acceleration.x;
  lastY = a.acceleration.y;
  lastZ = a.acceleration.z;

  if (change > SHAKE_THRESHOLD) {
    shaken = true;
    lastShake = millis();
  }

  Serial.print("Change: ");
  Serial.print(change, 2);
  // keep showing SHAKE for 300 ms so it doesn't flicker
  Serial.println(shaken && millis() - lastShake < 300 ? "  SHAKE" : "  QUIET");
  delay(50);
}
