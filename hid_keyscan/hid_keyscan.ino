/*********************************************************************
 This is an example for our nRF52 based Bluefruit LE modules

 Pick one up today in the adafruit shop!

 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

/* This sketch demonstate how to use BLE HID to scan an array of pin
 * and send its keycode. It is essentially an implementation of hid keyboard,
 * useful reference if you want to make an BLE keyboard.
 */

#include <bluefruit.h>

BLEHidAdafruit blehid;

#define MOD_LCTRL	(KEYBOARD_MODIFIER_LEFTCTRL << 8)
#define MOD_LSHIFT	(KEYBOARD_MODIFIER_LEFTSHIFT << 8)
#define MOD_LALT	(KEYBOARD_MODIFIER_LEFTALT << 8)
#define MOD_LGUI	(KEYBOARD_MODIFIER_LEFTGUI << 8)
#define MOD_RCTRL	(KEYBOARD_MODIFIER_RIGHTCTRL << 8)
#define MOD_RSHIFT	(KEYBOARD_MODIFIER_RIGHTSHIFT << 8)
#define MOD_RALT	(KEYBOARD_MODIFIER_RIGHTALT << 8)
#define MOD_RGUI	(KEYBOARD_MODIFIER_RIGHTGUI << 8)
#define IS_MODIFIER(s)	((s)&0xff00)
#define GET_MODIFIER(s)	(((s)&0xff00)>>8)

#define LAYER_KEY_1	(0xfff1)
#define IS_LAYER_KEY(s)	(((s)&0xfff0) == 0xfff0)
#define GET_LAYER(s)	(((s)&0x000f))

#if defined ARDUINO_NRF52840_FEATHER
/*
 * Adafruit Feather nRF52840
 */
const char *keyboard_name = "Feather nRF52840";
const byte nlayers = 2;
const byte nrows = 5;
const byte ncols = 14;
const byte battery_pin = A6;
byte rowPins[] = {9, 6, 5, PIN_WIRE_SCL, PIN_WIRE_SDA}; //connect to the row pinouts of the keypad
byte columnPins[] = {
  A0, A1, A2, A3, A4,
  A5, PIN_SPI_SCK, PIN_SPI_MOSI, PIN_SPI_MISO, PIN_SERIAL_RX,
  13, 12, 11, 10
}; //connect to the column pinouts of the keypad
int symbols[nlayers][nrows][ncols] = {
  // LAYER 0
  // R0
  HID_KEY_ESCAPE,	HID_KEY_1,		HID_KEY_2,		HID_KEY_3,			HID_KEY_4,		HID_KEY_5,		HID_KEY_6,
  HID_KEY_7,		HID_KEY_8,		HID_KEY_9,		HID_KEY_0,			HID_KEY_MINUS,		HID_KEY_EQUAL,		HID_KEY_BACKSPACE,
  // R1
  HID_KEY_TAB,		HID_KEY_Q,		HID_KEY_W,		HID_KEY_E,			HID_KEY_R,		HID_KEY_T,		HID_KEY_Y,
  HID_KEY_U,		HID_KEY_I,		HID_KEY_O,		HID_KEY_P,			HID_KEY_BRACKET_LEFT,	HID_KEY_BRACKET_RIGHT,	HID_KEY_BACKSLASH,
  // R2
  MOD_LCTRL,		HID_KEY_A,		HID_KEY_S,		HID_KEY_D,			HID_KEY_F,		HID_KEY_G,		HID_KEY_H,
  HID_KEY_J,		HID_KEY_K,		HID_KEY_L,		HID_KEY_SEMICOLON,		HID_KEY_APOSTROPHE,	HID_KEY_NONE,		HID_KEY_RETURN,
  // R3
  MOD_LSHIFT,		HID_KEY_NONE,		HID_KEY_Z,		HID_KEY_X,			HID_KEY_C,		HID_KEY_V,		HID_KEY_B,
  HID_KEY_N,		HID_KEY_M,		HID_KEY_COMMA,		HID_KEY_PERIOD,			HID_KEY_SLASH,		MOD_RSHIFT,		HID_KEY_NONE,
  // R4
  HID_KEY_CAPS_LOCK,	MOD_LALT,		MOD_LGUI,		HID_KEY_NONE,			HID_KEY_NONE,		HID_KEY_NONE,		HID_KEY_SPACE,
  HID_KEY_NONE,		HID_KEY_NONE,		MOD_RALT,		MOD_RGUI,			MOD_RCTRL,		HID_KEY_NONE,		LAYER_KEY_1,

  // LAYER 1
  // R0
  HID_KEY_GRAVE,	HID_KEY_F1,		HID_KEY_F2,		HID_KEY_F3,			HID_KEY_F4,		HID_KEY_F5,		HID_KEY_F6,
  HID_KEY_F7,		HID_KEY_F8,		HID_KEY_F9,		HID_KEY_F10,			HID_KEY_F11,		HID_KEY_F12,		HID_KEY_DELETE,
  // R1
  HID_KEY_NONE,		HID_KEY_NONE,		HID_KEY_NONE,		HID_KEY_NONE,			HID_KEY_NONE,		HID_KEY_NONE,		HID_KEY_NONE,
  HID_KEY_NONE,		HID_KEY_NONE,		HID_KEY_NONE,		HID_KEY_NONE,			HID_KEY_NONE,		HID_KEY_NONE,		HID_KEY_NONE,
  // R2
  MOD_LCTRL,		HID_KEY_NONE,		HID_KEY_NONE,		HID_KEY_NONE,			HID_KEY_NONE,		HID_KEY_NONE,		HID_KEY_NONE,
  HID_KEY_NONE,		HID_KEY_NONE,		HID_KEY_NONE,		HID_KEY_NONE,			HID_KEY_NONE,		HID_KEY_NONE,		HID_KEY_NONE,
  // R3
  MOD_LSHIFT,		HID_KEY_NONE,		HID_KEY_NONE,		HID_KEY_NONE,			HID_KEY_NONE,		HID_KEY_NONE,		HID_KEY_NONE,
  HID_KEY_NONE,		HID_KEY_NONE,		HID_KEY_NONE,		HID_KEY_NONE,			HID_KEY_ARROW_UP,	MOD_RSHIFT,		HID_KEY_NONE,
  // R4
  HID_KEY_CAPS_LOCK,	MOD_LALT,		MOD_LGUI,		HID_KEY_NONE,			HID_KEY_NONE,		HID_KEY_NONE,		HID_KEY_SPACE,
  HID_KEY_NONE,		HID_KEY_NONE,		HID_KEY_ARROW_LEFT,	HID_KEY_ARROW_DOWN,		HID_KEY_ARROW_RIGHT,	HID_KEY_NONE,		LAYER_KEY_1,
};
#else
/*
 * Hanyaduino nRF52840
 */
const char *keyboard_name = "Hanyaduino nRF52840";
const byte nlayers = 1;
const byte nrows = 4;
const byte ncols = 6;
const byte battery_pin = A6;
byte rowPins[] = {4, 5, 6, 8}; //connect to the row pinouts of the keypad
byte columnPins[] = {21, 20, 19, 18, 15, 14}; //connect to the column pinouts of the keypad
int symbols[nlayers][nrows][ncols] = {
  HID_KEY_Q, HID_KEY_W, HID_KEY_E, HID_KEY_R, HID_KEY_T, HID_KEY_Y,
  HID_KEY_A, HID_KEY_S, HID_KEY_D, HID_KEY_F, HID_KEY_G, HID_KEY_H,
  HID_KEY_Z, HID_KEY_X, HID_KEY_C, HID_KEY_V, HID_KEY_B, HID_KEY_N,
  HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, MOD_LCTRL, MOD_LSHIFT, HID_KEY_RETURN,
};
#endif

void scanKeys(byte scandata[nrows][ncols]) {
  // Re-intialize the row pins. Allows sharing these pins with other hardware.
  for (byte r=0; r<nrows; r++) {
    pinMode(rowPins[r], INPUT_PULLDOWN);
    digitalWrite(rowPins[r], LOW);
  }

  // bitMap stores ALL the keys that are being pressed.
  static byte accumulator[ncols * nrows];
  for (byte c=0; c<ncols; c++) {
    pinMode(columnPins[c], OUTPUT);
    digitalWrite(columnPins[c], HIGH);  // Begin column pulse output.
    for (byte r=0; r<nrows; r++) {
      scandata[r][c] = digitalRead(rowPins[r]);
      if (scandata[r][c])
        accumulator[r * ncols + c] += 1;
      delay(1);
    }

    // Set pin to high impedance input. Effectively ends column pulse.
    digitalWrite(columnPins[c], LOW);
    pinMode(columnPins[c], INPUT);
  }

  // Battery level
  int rawlevel = analogRead(battery_pin);
  int level = map(rawlevel, 500, 649, 0, 100);

  static unsigned long prev_time = 0;
  if (500*1000 < micros() - prev_time) {
    prev_time = micros();
    for (byte r=0; r<nrows; r++) {
      Serial.print("R");
      Serial.print(r);
      Serial.print(":");
      for (byte c=0; c<ncols; c++) {
        Serial.print(accumulator[r * ncols + c]?"x":"_");
        accumulator[r * ncols + c] = 0;
      }
      Serial.print("  ");
    }
    Serial.print("battery=");
    Serial.print(level);
    Serial.print("%");
    Serial.println();
  }
}

void setup() 
{
#if 1
  Serial.begin(115200);
  delay(5000);

  Serial.println("-------------------------------");
  Serial.println("Bluefruit52 HID Keyscan Example");
  Serial.print("keyboard name: ");
  Serial.println(keyboard_name);
  Serial.print("rows x cols: ");
  Serial.print(nrows);
  Serial.print(" x ");
  Serial.print(ncols);
  Serial.println("");
  Serial.println("-------------------------------\n");

  Serial.println();
  Serial.println("Go to your phone's Bluetooth settings to pair your device");
  Serial.println("then open an application that accepts keyboard input");

  Serial.println();
  Serial.println("Wire configured Pin to GND to send key");
  Serial.println("Wire Shift Keky to GND if you want to send it in upper case");
  Serial.println();  
#endif

  Bluefruit.begin();
  // Set max power. Accepted values are: -40, -30, -20, -16, -12, -8, -4, 0, 4
  Bluefruit.setTxPower(4);
  Bluefruit.setName(keyboard_name);

  /* Start BLE HID
   * Note: Apple requires BLE device must have min connection interval >= 20m
   * ( The smaller the connection interval the faster we could send data).
   * However for HID and MIDI device, Apple could accept min connection interval 
   * up to 11.25 ms. Therefore BLEHidAdafruit::begin() will try to set the min and max
   * connection interval to 11.25  ms and 15 ms respectively for best performance.
   */
  blehid.begin();

  // Set callback for set LED from central
  blehid.setKeyboardLedCallback(set_keyboard_led);

  /* Set connection interval (min, max) to your perferred value.
   * Note: It is already set by BLEHidAdafruit::begin() to 11.25ms - 15ms
   * min = 9*1.25=11.25 ms, max = 12*1.25= 15 ms 
   */
  /* Bluefruit.setConnInterval(9, 12); */

  // Set up and start advertising
  startAdv();
}

void startAdv(void)
{  
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_KEYBOARD);
  
  // Include BLE HID service
  Bluefruit.Advertising.addService(blehid);

  // There is enough room for the dev name in the advertising packet
  Bluefruit.Advertising.addName();
  
  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   * 
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html   
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds
}

bool keyPressedPreviously = false;
byte prev_scandata[nrows][ncols];

void loop()
{
  /*-------------- San Pin Array and send report ---------------------*/
  byte scandata[nrows][ncols];
  bool anyKeyPressed = false;
  uint8_t modifier = 0;
  uint8_t l = 0;
  uint8_t count=0;
  uint8_t keycode[6] = { 0 };

  scanKeys(scandata);

  // scan layer key
  for (byte c=0; c<ncols; c++) {
    for (byte r=0; r<nrows; r++) {
      if (scandata[r][c] && IS_LAYER_KEY(symbols[l][r][c])) {
        l = GET_LAYER(symbols[l][r][c]);
      }
    }
  }

  // scan mofidier key
  for (byte c=0; c<ncols; c++) {
    for (byte r=0; r<nrows; r++) {
      if (scandata[r][c] && !IS_LAYER_KEY(symbols[l][r][c]) && IS_MODIFIER(symbols[l][r][c])) {
        modifier |= GET_MODIFIER(symbols[l][r][c]);
      }
    }
  }

  // scan normal key and send report
  for (byte c=0; c<ncols; c++) {
    for (byte r=0; r<nrows; r++) {
      if (scandata[r][c] && !IS_MODIFIER(symbols[l][r][c])) {
        keycode[count++] = symbols[l][r][c];

        // used later
        anyKeyPressed = true;
        keyPressedPreviously = true;

        // 6 is max keycode per report
        if ( count == 6) {
          blehid.keyboardReport(modifier, keycode);

          // reset report
          count = 0;
          memset(keycode, 0, 6);
        }
      }
    }    
  }

  // Send any remaining keys (not accumulated up to 6)
  if ( count ) {
    blehid.keyboardReport(modifier, keycode);
  }

  // Send All-zero report to indicate there is no keys pressed
  // Most of the time, it is, though we don't need to send zero report
  // every loop(), only a key is pressed in previous loop() 
  if ( !anyKeyPressed && keyPressedPreviously )
  {
    keyPressedPreviously = false;
    //Serial.println("blehid.keyRelease()");
    blehid.keyRelease();
  }  

  memcpy(prev_scandata, scandata, sizeof(scandata));

  // Poll interval
  delay(10);
}

/**
 * Callback invoked when received Set LED from central.
 * Must be set previously with setKeyboardLedCallback()
 *
 * The LED bit map is as follows: (also defined by KEYBOARD_LED_* )
 *    Kana (4) | Compose (3) | ScrollLock (2) | CapsLock (1) | Numlock (0)
 */
void set_keyboard_led(uint8_t led_bitmap)
{
  // light up Red Led if any bits is set
  if ( led_bitmap )
  {
    ledOn( LED_RED );
  }
  else
  {
    ledOff( LED_RED );
  }
}
