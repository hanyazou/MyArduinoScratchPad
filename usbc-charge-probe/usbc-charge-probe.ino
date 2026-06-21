/*
 * Copyright (c) 2026 @hanyazou
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <Arduino.h>

#include "UsbcChgProbeTest.h"

// -----------------------------------------------------------------------------
// Menu
// -----------------------------------------------------------------------------

static void print_menu(void) {
    Serial.println();
    Serial.println("========================================");
    Serial.println("USB-C Charge Probe");
    Serial.println("========================================");
    Serial.println("1: Unconnected self-check");
    Serial.println("2: Sink attach / VBUS wait");
    Serial.println("3: BC1.2 D+ 0.6V / D- response check");
    Serial.println("o: Open all probe lines");
    Serial.println("v: Print voltages once");
    Serial.println("q: Turn LEDs off");
    Serial.println();
    Serial.print("> ");
}

static void drain_serial_input(void) {
    while (Serial.available() > 0) {
        Serial.read();
    }
}

void setup() {
    Serial.begin(115200);

    while (!Serial) {
        // Wait for Serial Monitor on boards that support this.
        // On UNO R4 this should be fine.
    }

    usbc_chg_probe_test_init();

    Serial.println();
    Serial.println("USB-C Charge Probe firmware started.");
    Serial.println("All probe lines are open.");
    print_menu();
}

void loop() {
    if (Serial.available() == 0) {
        return;
    }

    char c = Serial.read();

    // Consume CR/LF etc.
    if (c == '\r' || c == '\n' || c == ' ') {
        return;
    }

    Serial.println(c);

    switch (c) {
    case '1':
        drain_serial_input();
        usbc_chg_probe_test_unconnected_self_check();
        break;

    case '2':
        drain_serial_input();
        usbc_chg_probe_test_sink_attach_wait();
        break;

    case '3':
        drain_serial_input();
        usbc_chg_probe_test_bc12_dp_0v6_dm_response();
        break;

    case 'o':
    case 'O':
        usbc_chg_probe_test_all_probe_lines_open();
        Serial.println("All probe lines are open.");
        break;

    case 'v':
    case 'V':
        usbc_chg_probe_test_print_all_voltages();
        break;

    case 'q':
    case 'Q':
        usbc_chg_probe_test_led_all_off();
        Serial.println("LEDs off.");
        break;

    default:
        Serial.println("Unknown command.");
        break;
    }

    print_menu();
}
