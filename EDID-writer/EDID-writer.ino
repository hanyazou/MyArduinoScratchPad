/*
 * EDID writer for CAT24C208 EEPROM
 */
#include <Wire.h>

#define EEPROM_I2C_ADDR 0x50
#define EEPROM_I2C_SEGMENT_POINTER_ADDR 0x30

#if 0
// 24C02N ATMEL
#define EEPROM_SIZE 256  // 2048bits
#define EEPROM_WRITE_DELAY 5
#else
// CAT24C208 ON Semiconductor
#define EEPROM_SIZE 512  // 4096bits
#define EEPROM_I2C_CONFIG_ADDR 0x31
#define EEPROM_I2C_CONFIG_BYTE 0x0c // write enable, lower bank
#define EEPROM_WRITE_DELAY 10
#endif

#define WIRE_SUCCESS 0
#define WIRE_ERROR_BUFFER_OVERFLOW 1
#define WIRE_ERROR_ADDRESS_NACK 2
#define WIRE_ERROR_DATA_NACK 3
#define WIRE_ERROR_OTHER 4

/*
 * EDID (dummy)
 */
byte eepromdat[] = {
    // block 0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    // block 1
    0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    // block 2
    0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    // block 3
    0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x19, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x1a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x1b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x1d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

void i2c_print_error(char* op, int address, int status)
{
    Serial.print("I2C ERROR: ");
    Serial.print(op);
    Serial.print(" address=0x");
    Serial.print(address, HEX);
    Serial.print(" status=");
    Serial.print(status);
    Serial.println();
}

int i2c_eeprom_read_segment_byte(uint16_t eeaddress, byte* rdata, bool segment)
{
    int status;
    int max_retry = 50;

    *rdata = 0xFF;

  retry:
    if (segment) {
        Wire.beginTransmission(EEPROM_I2C_SEGMENT_POINTER_ADDR);
        Wire.write((byte)(eeaddress / 0x100));
        status = Wire.endTransmission(false);  // no STOP condition
        if (status != WIRE_SUCCESS) {
            if (status == WIRE_ERROR_ADDRESS_NACK || status == WIRE_ERROR_DATA_NACK) {
                delay(5);
                if (0 < max_retry--)
                    goto retry;
            }
            i2c_print_error("write", EEPROM_I2C_SEGMENT_POINTER_ADDR, status);
            return status;
        }
    }

    Wire.beginTransmission(EEPROM_I2C_ADDR);
    Wire.write((byte)(eeaddress % 0x100));
    status = Wire.endTransmission(false);  // no STOP condition
    if (status != WIRE_SUCCESS) {
        if (status == WIRE_ERROR_ADDRESS_NACK || status == WIRE_ERROR_DATA_NACK) {
            delay(1);
            if (0 < max_retry--)
                goto retry;
        }
        i2c_print_error("write", EEPROM_I2C_ADDR, status);
        return status;
    }

    Wire.requestFrom(EEPROM_I2C_ADDR, (uint8_t)1);
    while (!Wire.available()); 
    *rdata = Wire.read();

    return WIRE_SUCCESS;
}

int i2c_eeprom_write_segment_byte(uint16_t eeaddress, byte data, bool segment)
{
    int status;
    int max_retry = 50;

  retry:
    if (segment) {
        Wire.beginTransmission(EEPROM_I2C_SEGMENT_POINTER_ADDR);
        Wire.write((byte)(eeaddress / 0x100));
        status = Wire.endTransmission(false);  // no STOP condition
        if (status != WIRE_SUCCESS) {
            if (status == WIRE_ERROR_ADDRESS_NACK || status == WIRE_ERROR_DATA_NACK) {
                delay(5);
                if (0 < max_retry--)
                    goto retry;
            }
            i2c_print_error("write", EEPROM_I2C_SEGMENT_POINTER_ADDR, status);
            return status;
        }
    }

    Wire.beginTransmission(EEPROM_I2C_ADDR);
    Wire.write((byte)(eeaddress % 0x100));
    Wire.write(data);
    status = Wire.endTransmission();
    if (status != WIRE_SUCCESS) {
        if (status == WIRE_ERROR_ADDRESS_NACK || status == WIRE_ERROR_DATA_NACK) {
            delay(1);
            if (0 < max_retry--)
                goto retry;
        }
        i2c_print_error("write", EEPROM_I2C_ADDR, status);
        return status;
    }

    return WIRE_SUCCESS;
}

int i2c_write_byte(uint8_t deviceaddress, int eeaddress, byte data)
{
    int status;
    int max_retry = 50;

  retry:
    Wire.beginTransmission(deviceaddress);
    Wire.write((byte)eeaddress);
    Wire.write(data);
    status = Wire.endTransmission();
    if (status != WIRE_SUCCESS) {
        if (status == WIRE_ERROR_ADDRESS_NACK || status == WIRE_ERROR_DATA_NACK) {
            delay(1);
            if (0 < max_retry--)
                goto retry;
        }
        i2c_print_error("write", EEPROM_I2C_ADDR, status);
        return status;
    }

    return WIRE_SUCCESS;
}

void write_read_verify(bool do_write, bool do_read, bool do_verify)
{
    byte b;
    uint8_t i2c_addr;
    char* op;

    if (do_verify && do_write) {
        op = "Write and Verify";
    } else
    if (do_verify) {
        op = "Verify";
    } else
    if (do_write) {
        op = "Write";
    } else
    if (do_read) {
        op = "Read";
    } else {
        op = "Dump internal data";
    }

    if (do_verify) {
        do_read = true;
    }

    Serial.print("Start ");
    Serial.print(op);
    Serial.print("...");
#ifdef EEPROM_I2C_CONFIG_ADDR
    i2c_write_byte(EEPROM_I2C_CONFIG_ADDR, 0, EEPROM_I2C_CONFIG_BYTE);
#endif
    for (uint16_t index = 0; index < EEPROM_SIZE; index++) {
        if (index < sizeof(eepromdat)) {
            b = eepromdat[index];
        } else {
            b = 0xFF;
        }
        uint8_t d =  b;

        if (do_write) {
            i2c_eeprom_write_segment_byte(index, b, 0x100 <= index);
            delay(EEPROM_WRITE_DELAY);
	}
        if (do_read) {
            i2c_eeprom_read_segment_byte(index, &d, 0x100 <= index);
	}
        if (do_verify && b != d) {
            Serial.println();
            Serial.print(F("verification failed at 0x"));
	    Serial.print(index, HEX);
            Serial.print(", data=0x");
	    Serial.print(b, HEX);
            Serial.print(", eeprom=0x");
	    Serial.print(d, HEX);
            while (1);
        }

        if ((index % 16) == 0) {
            Serial.println();
            if (index < 0x1000) Serial.print('0');
            if (index < 0x100) Serial.print('0');
            if (index < 0x10) Serial.print('0');
            Serial.print(index, HEX);
            Serial.print(": ");
	}
        Serial.print("0x");
        if (d < 0x10) Serial.print('0');
        Serial.print(d, HEX); //print content to serial port
        if ((index % 16) != 15)
            Serial.print(", ");
    }
    Serial.print("\n\r========\n\r");
    Serial.print(op);
    Serial.println(" finished!");
}

void setup() {
    Serial.begin(9600);
    while (!Serial);

    Serial.println();
    Serial.println(F("EEPROM WRITER"));
    Serial.print(F("EEPROM data size: "));
    Serial.println(sizeof(eepromdat));

    Wire.begin(); // initialise the connection

    do {
        Serial.println();
        Serial.println(F("   r: read from EEPROM"));
        Serial.println(F("   w: write to EEPROM"));
        Serial.println(F("   v: verify EEPROM"));
        Serial.println(F("  wv: write to EEPROM and verify"));
        Serial.println(F("   d: dump internal data"));
        Serial.println();
        Serial.println(F("input & return to start"));
        while (!Serial.available());
	String input;
        while (Serial.available()) {
            char c = Serial.read();
            if (isalnum(c))
                input += c;
        }
	Serial.print("input='");
	Serial.print(input);
	Serial.println("'");

        if (input == "r") {
            write_read_verify(false, true, false);
        } else
        if (input == "w") {
            write_read_verify(true, false, false);
        } else
        if (input == "v") {
            write_read_verify(false, false, true);
        } else
        if (input == "wv") {
            write_read_verify(true, false, false);
            Serial.println();
            write_read_verify(false, false, true);
        } else
        if (input == "d") {
            write_read_verify(false, false, false);
        }
    } while (true);
}

void loop() {
}
