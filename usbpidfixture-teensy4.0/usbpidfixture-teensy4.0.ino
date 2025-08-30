#include "usb_desc_helper.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define ERROR_MODE 10

uint8_t ledseg_pins[] = { 10, 5, 16, 15, 9, 8, 6, 7 };
uint8_t ledsel_pins[] = { 17, 18 };
uint8_t sw_pins[] = { 19, 20, 21, 22 };
uint8_t reset_button = 1;

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
    0x01,  // -  // ERROR_MODE
};

int g_mode = 0;
int g_mode_input = 0;
uint16_t g_original_vid = 0xffff;
uint16_t g_original_pid = 0xffff;
uint16_t g_vid = 0xffff;
uint16_t g_pid = 0xffff;
uint8_t* g_desc = nullptr;
size_t g_desc_len = 0;

void initialize(void) {
    pinMode(reset_button, INPUT_PULLUP);
    while (digitalRead(reset_button) == LOW) {
        // Wait for the software reset button to be released
    }
    for (unsigned int i = 0; i < ARRAY_SIZE(ledseg_pins); i++) {
        pinMode(ledseg_pins[i], OUTPUT);
        digitalWrite(ledseg_pins[i], HIGH);
    }
    for (unsigned int i = 0; i < ARRAY_SIZE(ledsel_pins); i++) {
        pinMode(ledsel_pins[i], OUTPUT);
        digitalWrite(ledsel_pins[i], LOW);
    }
    for (unsigned int i = 0; i < ARRAY_SIZE(sw_pins); i++) {
        pinMode(sw_pins[i], INPUT_PULLUP);
    }

    // Delay to let GPIO levels stabilize after power-up/reset before reading.
    delay(5);
}

void display(uint8_t val) {
    for (unsigned int i = 0; i < ARRAY_SIZE(ledsel_pins); i++) {
        pinMode(ledsel_pins[i], OUTPUT);
        digitalWrite(ledsel_pins[i], LOW);
    }
    digitalWrite(ledsel_pins[0], HIGH);

    unsigned int pattern = segpatterns[val];
    for (unsigned int i = 0; i < ARRAY_SIZE(ledseg_pins); i++) {
        pinMode(ledseg_pins[i], OUTPUT);
        digitalWrite(ledseg_pins[i], (pattern & 0x80) ? LOW : HIGH);
        pattern <<= 1;
    }
}

void print02x(uint8_t data) {
    if (data < 0x10) {
        Serial.print("0");
    }
    Serial.print(data, HEX);
}

void print04x(uint16_t data) {
    print02x((data >> 8) & 0xff);
    print02x((data >> 0) & 0xff);
}

void set_usb_id(uint16_t vid, uint16_t pid) {
    get_usb_device_descriptor(&g_desc, &g_desc_len);
    if (g_desc == nullptr || g_desc_len < 0x12) {
        return;
    }

    uint16_t* vidp = reinterpret_cast<uint16_t*>(&g_desc[8]);
    uint16_t* pidp = reinterpret_cast<uint16_t*>(&g_desc[10]);

    g_original_vid = *vidp;
    g_original_pid = *pidp;
    *vidp = vid;
    *pidp = pid;
    g_vid = *vidp;
    g_pid = *pidp;
}

uint8_t read_sw(void) {
    uint8_t value = 0;
    for (int i = ARRAY_SIZE(sw_pins) - 1; 0 <= i; i--) {
        value <<= 1;
        if (digitalRead(sw_pins[i]))
            value |= 1;
    }
    return value;
}

extern "C" void startup_middle_hook(void) {
    initialize();
    g_mode_input = g_mode = read_sw();
    if (0 < g_mode && g_mode <= 8) {
        set_usb_id(0x1a0a, 0x0100 + g_mode);
    } else {
        g_mode = ERROR_MODE;
    }
    display(g_mode);
}

void setup() {
    Serial.begin(9600);
    for (int i = 0; i < 100 && !Serial; i++)
        delay(50);

    Serial.print("USB device descriptor address=0x");
    Serial.print(reinterpret_cast<long unsigned int>(g_desc), HEX);
    Serial.print(", length=");
    Serial.println(g_desc_len);

    Serial.print("USB test fixture mode = ");
    if (g_mode != ERROR_MODE) {
        Serial.println(g_mode);
    } else {
        Serial.println("none");
    }

    if (g_desc == nullptr || g_desc_len < 0x12) {
        Serial.println("running as the Teensy USB serial device");
    } else {
        Serial.print("ID: ");
        print04x(g_original_vid);
        Serial.print(":");
        print04x(g_original_pid);

        Serial.print(" -> ");
        print04x(g_vid);
        Serial.print(":");
        print04x(g_pid);
        Serial.println();
    }
}

void loop() {
    static int count = 0;
    static bool changing_mode = false;

    if (digitalRead(reset_button) == LOW) {
        Serial.println("Resetting...");
        delay(100);  // some delay for flush messages
        SCB_AIRCR = 0x05FA0004;  // trigger Teensy 4.x reset
    }

    uint8_t mode = read_sw();
    if (g_mode_input != mode) {
        changing_mode = true;
    }

    if (changing_mode) {
        display(mode);
    } else {
        bool lit = ((count++ / 100) % 2);
        digitalWrite(LED_BUILTIN, lit ? HIGH : LOW);
        digitalWrite(ledseg_pins[0], lit ? LOW : HIGH);
        delay(10);
    }
}
