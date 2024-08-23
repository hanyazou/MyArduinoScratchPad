/*  7 segment display MAX7219 driver serial control
  Schematic: https://www.electronoobs.com/eng_arduino_tut54_sch1.php
  Code: https://www.electronoobs.com/eng_arduino_tut54_code1.php
*/

#define MAX7219_Data_IN 2
#define MAX7219_Chip_Select  3
#define MAX7219_Clock 4

#define MAX7219_REG_NOP                 0x0
#define MAX7219_REG_DIGIT(n)            ((n) + 1)
#define     MAX7219_REG_DIGIT_DP            0x80
#define MAX7219_REG_DECODE_MODE         0x9
#define MAX7219_REG_INTENSITY           0xa
#define MAX7219_REG_SCAN_LIMIT          0xb
#define MAX7219_REG_SHUTDOWN            0xc
#define MAX7219_REG_DISPLAY_TEST        0xf

#define NUM_CASCADES 4
//#define SEGMENT_LED

/*
 *           A
 *         -----
 *      F/  G  /B
 *       -----
 *    E/     /C
 *     -----
 *       D
 */
#ifdef SEGMENT_LED
byte decode_table[] = {
 //ABCDEFG
  B1111110,  // 0
  B0110000,  // 1
  B1101101,  // 2
  B1111001,  // 3
  B0110011,  // 4
  B1011011,  // 5
  B1011111,  // 6
  B1110010,  // 7
  B1111111,  // 8
  B1111011,  // 9
  B1110111,  // A
  B0011111,  // b
  B1001110,  // C
  B0111101,  // d
  B1001111,  // E
  B1000111,  // F
};
#else
// DOT_MATRIX
byte decode_table[] = {
 //ABCDEFG
  B11111111,  // 0
  B01111111,  // 1
  B00111111,  // 2
  B00011111,  // 3
  B00001111,  // 4
  B00000111,  // 5
  B00000011,  // 6
  B00000001,  // 7
  B00000000,  // 8
  B00000001,  // 9
  B00000011,  // A
  B00000111,  // b
  B00001111,  // C
  B00011111,  // d
  B00111111,  // E
  B01111111,  // F
};
#endif

void max7219_write(byte send_to_address, int nchains, byte data[]) {
    digitalWrite(MAX7219_Chip_Select, LOW);
    for (int i = 0; i < nchains; i++) {
        shiftOut(MAX7219_Data_IN, MAX7219_Clock, MSBFIRST, send_to_address);
        shiftOut(MAX7219_Data_IN, MAX7219_Clock, MSBFIRST, data[i]);
    }
    digitalWrite(MAX7219_Chip_Select, HIGH);
}

void max7219_write(byte send_to_address, int nchains, int data) {
    digitalWrite(MAX7219_Chip_Select, LOW);
    for (int i = 0; i < nchains; i++) {
        shiftOut(MAX7219_Data_IN, MAX7219_Clock, MSBFIRST, send_to_address);
        shiftOut(MAX7219_Data_IN, MAX7219_Clock, MSBFIRST, data);
    }
    digitalWrite(MAX7219_Chip_Select, HIGH);
}

void setup() {
    // Setup I/O pins
    pinMode(MAX7219_Data_IN, OUTPUT);
    pinMode(MAX7219_Chip_Select, OUTPUT);
    pinMode(MAX7219_Clock, OUTPUT);
    digitalWrite(MAX7219_Clock, HIGH);
    delay(200);

    // Setup MAX7219
    max7219_write(MAX7219_REG_DISPLAY_TEST, NUM_CASCADES, 0x00); // test mode off
    max7219_write(MAX7219_REG_SHUTDOWN, NUM_CASCADES, 0x01); // normal operation
    max7219_write(MAX7219_REG_SCAN_LIMIT, NUM_CASCADES, 0x07); // display digits 0 thru 7
    max7219_write(MAX7219_REG_INTENSITY, NUM_CASCADES, 0x0f); // max brightness
    max7219_write(MAX7219_REG_DECODE_MODE, NUM_CASCADES, 0x00); // bypasses the decoder
}

void loop() {
    static int count = 0;

    // Fill buffer
    byte data[8][NUM_CASCADES];
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < NUM_CASCADES; j++) {
            int val = (8 * NUM_CASCADES - 8 * j - i - 1 + count) % 16;
            data[i][j] = decode_table[val];
#ifdef SEGMENT_LED
            if (val == 0x0f) data[i][j] |= MAX7219_REG_DIGIT_DP;
#endif
        }
    }

    //  Send buffer to cascaded MX7219s
    for (int i = 0; i < 8; i++) {
        max7219_write(MAX7219_REG_DIGIT(i), NUM_CASCADES, data[i]);
    }

    count++;
    delay(250);
}
