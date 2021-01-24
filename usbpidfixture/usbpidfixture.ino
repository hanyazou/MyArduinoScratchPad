#include <Keyboard.h>
#include <USBDesc.h>

#ifdef LED_BUILTIN
#undef LED_BUILTIN
#endif
#define LED_BUILTIN 17 // pro micro RX LED on the board

uint8_t ledseg_pins[] = { 9, 4, 16, 10, 8, 7, 5, 6 };
uint8_t ledsel_pins[] = { 14, 15 };
uint8_t sw_pins[] = { 18, 19, 20, 21 };

uint8_t segpatterns[] = {
    0x7e,  // 0
    0x30,  // 1
    0x6d,  // 2
    0x79,  // 3
    0x33,  // 4
    0x5b,  // 5
    0x5f,  // 6
    0x70,  // 7
    0x7f,  // 8
    0x7b,  // 9
};

int mode = 0;

void initVariant() {
    pinMode(LED_BUILTIN, OUTPUT);

    for (int i = 0; i < sizeof(ledsel_pins); i++) {
        pinMode(ledsel_pins[i], OUTPUT);
        digitalWrite(ledsel_pins[i], LOW);
    }

    for (int i = sizeof(sw_pins) - 1; 0 <= i; i--) {
        pinMode(sw_pins[i], INPUT);
        digitalWrite(sw_pins[i], HIGH);
        mode = mode << 1;
        if (digitalRead(sw_pins[i]))
            mode = mode |= 1;
    }

    digitalWrite(ledsel_pins[0], HIGH);

    u8 pattern = segpatterns[mode];
    for (int i = 0; i < sizeof(ledseg_pins); i++) {
        pinMode(ledseg_pins[i], OUTPUT);
        digitalWrite(ledseg_pins[i], (pattern & 0x80) ? LOW : HIGH);
        pattern <<= 1;
    }
}

void AltDeviceDescriptor(const u8** desc, u8* flags) {
    static DeviceDescriptor mydesc =
        D_DEVICE(0xEF,0x02,0x01,64,0x054c,USB_PID,0x100,IMANUFACTURER,IPRODUCT,ISERIAL,1);
    if (1 <= mode && mode <= 8) {
        mydesc.idVendor = 0x1a0a;
        mydesc.idProduct = 0x0100 + mode;
        *desc = (u8*)&mydesc;
        *flags = 0;
    } else
    if (mode == 9) {
        const int led = 17; // pro micro RX LED on the board
        pinMode(led, OUTPUT);
        while (true) {
            digitalWrite(led, HIGH);
            delay(100);
            digitalWrite(led, LOW);
            delay(100);
        }
    }
}

void setup() {
    Serial.begin(9600);
    while (!Serial);
    Serial.print("USB test fixture mode = ");
    Serial.println(mode);
}

void loop() {
    Serial.println("running... on serial");

    digitalWrite(LED_BUILTIN, HIGH);
    digitalWrite(ledseg_pins[0], HIGH);
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(ledseg_pins[0], LOW);
    delay(1000);
}
