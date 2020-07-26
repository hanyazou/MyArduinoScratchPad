/*
 * https://arduino-info.wikispaces.com/LCD-Blue-I2C
 */
 
// I2C Scanner
// Written by Nick Gammon
// Date: 20th April 2011

#include <Wire.h>

//#define Serial SerialUSB

void setup() {
  Serial.begin (9600);
  while (!Serial);

  // Leonardo: wait for serial port to connect
  //while (!Serial) 
    {
    }
}  // end of setup

void loop() {
    Serial.println ();
  Serial.println ("I2C scanner. Scanning ...");
  byte count = 0;
  
  Wire.begin();
  for (byte i = 0; i < 128; i++)
  {
    //Serial.print("I2C scanner. Scanning ");
    //Serial.println(i);
    Wire.beginTransmission (i);
    if (Wire.endTransmission () == 0)
      {
      Serial.print ("Found address: ");
      Serial.print (i, DEC);
      Serial.print (" (0x");
      Serial.print (i, HEX);
      Serial.println (")");
      count++;
      delay (1);  // maybe unneeded?
      } // end of good response
  } // end of for loop
  Serial.println ("Done.");
  Serial.print ("Found ");
  Serial.print (count, DEC);
  Serial.println (" device(s).");
  delay(1 * 1000);
}
