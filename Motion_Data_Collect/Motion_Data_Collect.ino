/*
Learn Edge AI on UNIHIKER K10
Edge Impulse Beginner Tutorial
Mukesh Sankhla | makerbrains.com
*/

#include "unihiker_k10.h"

UNIHIKER_K10 k10;

void setup() {
  Serial.begin(115200);
  k10.begin();
  Serial.println("Edge Impulse Data Forwarder");
  delay(1000);
}

void loop() {
  float accx = k10.getAccelerometerX();
  float accy = k10.getAccelerometerY();
  float accz = k10.getAccelerometerZ();

  // Print in CSV format for Edge Impulse
  // Format: X,Y,Z
  Serial.print(accx, 3);
  Serial.print(",");
  Serial.print(accy, 3);
  Serial.print(",");
  Serial.println(accz, 3);

  // Sampling frequency (e.g., 50 Hz → 20 ms delay)
  delay(20);
}
