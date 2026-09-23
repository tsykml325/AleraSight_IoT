#include <Wire.h>

void setup() {

  Serial.begin(115200);

  Wire.begin(21,22);

  Serial.println("SCAN");
}

void loop() {

  byte error, address;

  for(address = 1; address < 127; address++ ) {

    Wire.beginTransmission(address);

    error = Wire.endTransmission();

    if(error == 0) {

      Serial.print("FOUND : 0x");

      Serial.println(address, HEX);
    }
  }

  delay(3000);
}