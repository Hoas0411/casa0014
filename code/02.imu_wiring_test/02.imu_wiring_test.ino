// Check the IMU wiring after soldering the header pins.
// Scans the I2C bus, then prints acceleration (g) and z rotation (deg/s).

#include <Wire.h>

byte addr = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000);
  Wire.begin();

  Serial.println("Scanning I2C...");
  for (byte a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.print("found device at 0x");
      Serial.println(a, HEX);
      if (a == 0x68 || a == 0x69) addr = a;
    }
  }
  if (!addr) {
    Serial.println("No MPU found - check wiring");
    while (true);
  }

  Serial.print("WHO_AM_I = 0x");
  Serial.println(readReg(0x75), HEX);
  writeReg(0x6B, 0x00);  // wake up
  writeReg(0x1C, 0x10);  // accel +-8g
  writeReg(0x1B, 0x10);  // gyro +-1000 deg/s
}

void loop() {
  // 14 bytes from 0x3B: accel xyz, temperature, gyro xyz
  Wire.beginTransmission(addr);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(addr, (byte)14);
  int16_t v[7];
  for (int i = 0; i < 7; i++) v[i] = (Wire.read() << 8) | Wire.read();

  float ax = v[0] / 4096.0, ay = v[1] / 4096.0, az = v[2] / 4096.0;
  float gz = v[6] / 32.8;

  Serial.print("ax:"); Serial.print(ax, 2);
  Serial.print(" ay:"); Serial.print(ay, 2);
  Serial.print(" az:"); Serial.print(az, 2);
  Serial.print(" gz:"); Serial.println(gz, 0);
  delay(200);
}

byte readReg(byte r) {
  Wire.beginTransmission(addr);
  Wire.write(r);
  Wire.endTransmission(false);
  Wire.requestFrom(addr, (byte)1);
  return Wire.read();
}

void writeReg(byte r, byte val) {
  Wire.beginTransmission(addr);
  Wire.write(r);
  Wire.write(val);
  Wire.endTransmission();
}
