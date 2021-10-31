#include <Wire.h>

const int DEVADDR = 0x0a;
const int REG_LED_RED = 0x00;
const int REG_LEFT = 0x04;
const int REG_CHIP_ID = 0xfa;

bool reg_read(const int reg, byte* buf, int len) {
    Wire.beginTransmission(DEVADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) == 0) {
        memset(buf, 0, len);
        Wire.requestFrom(DEVADDR, len);
        Wire.readBytes(buf, len);
        return true;
    } else {
        return false;
    }
}
  
bool reg_write(const int reg, byte* buf, int len) {
    Wire.beginTransmission(DEVADDR);
    Wire.write(reg);
    for (int i = 0; i < len; i++)
        Wire.write(buf[i]);
    return Wire.endTransmission() == 0;
}

unsigned int get_chipid() {
    unsigned char buf[2];
    reg_read(REG_CHIP_ID, buf, 2);
    return buf[0] + (buf[1] << 8);
}

bool set_rgbw(const int r, const int g, const int b, const int w) {
    unsigned char buf[4];
    buf[0] = r;
    buf[1] = g;
    buf[2] = b;
    buf[3] = w;
    reg_write(REG_LED_RED, buf, 4);
}

void setup() {
    Serial.begin(9600);
    while (!Serial); 
    Wire.begin();

    unsigned int chipid = get_chipid();
    Serial.print("chip id: ");
    Serial.println(chipid, HEX);
}

void loop() {
    int scale = 10;
    static bool mode = true;
    static unsigned char r = 0, g = 0, b = 0, w = 0;
    byte buf[5];
    reg_read(REG_LEFT, buf, 5);

    int i;
    for (i = 0; i < 5; i++)
      if (buf[i] != 0) break;
    if (5 <= i)
        goto end;

 #if 0
    for (int i = 0; i < 5; i++) {
        Serial.print(buf[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
 #endif

    if (mode) {
        r += buf[0] * scale;
        r -= buf[1] * scale;
        g += buf[2] * scale;
        g -= buf[3] * scale;
    } else {
        b += buf[0] * scale;
        b -= buf[1] * scale;
        w += buf[2] * scale;
        w -= buf[3] * scale;
    }
    if (buf[4] == 0x81) {
        mode = !mode;
        Serial.print("mode: ");
        Serial.println(mode);
    }

    Serial.print("rgbw: ");
    Serial.print(r);
    Serial.print(" ");
    Serial.print(g);
    Serial.print(" ");
    Serial.print(b);
    Serial.print(" ");
    Serial.print(w);
    Serial.println();
    set_rgbw(r, g, b, w);

end:
    delay(50);
}
