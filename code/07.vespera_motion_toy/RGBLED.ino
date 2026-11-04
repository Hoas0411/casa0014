// onboard RGB LED on the MKR1010 (pins 25 R, 26 G, 27 B)
void setOnboard(byte r, byte g, byte b) {
  WiFiDrv::analogWrite(25, r);
  WiFiDrv::analogWrite(26, g);
  WiFiDrv::analogWrite(27, b);
}
void LedRed()   { setOnboard(155, 0, 0); }
void LedGreen() { setOnboard(0, 155, 0); }
void LedBlue()  { setOnboard(0, 0, 155); }
void LedOff()   { setOnboard(0, 0, 0); }
