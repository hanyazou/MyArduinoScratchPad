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

#include "UsbcChgProbeTest.h"
#include "UsbcChgProbeHal.h"
#include "Util.h"

#include <Arduino.h>

// -----------------------------------------------------------------------------
// Test helpers
// -----------------------------------------------------------------------------

static bool in_range(float v, float lo, float hi) {
    return v >= lo && v <= hi;
}

static void print_voltage(const char *name, float v) {
    Serial.print(name);
    Serial.print("=");
    Serial.print(v, 3);
    Serial.print("V");
}

static bool expect_range(const char *label, float v, float lo, float hi) {
    const bool ok = in_range(v, lo, hi);

    Serial.print(label);
    Serial.print(": ");
    Serial.print(v, 3);
    Serial.print(" V, expected ");
    Serial.print(lo, 2);
    Serial.print("..");
    Serial.print(hi, 2);
    Serial.print(" V -> ");
    Serial.println(ok ? "OK" : "FAIL");

    return ok;
}

void usbc_chg_probe_test_init(void) {
    usbc_chg_probe_init();
}

void usbc_chg_probe_test_print_all_voltages(void) {
    const float vbus = usbc_chg_probe_read(USBC_CHG_PROBE_VBUS);
    const float cc1  = usbc_chg_probe_read(USBC_CHG_PROBE_CC1);
    const float cc2  = usbc_chg_probe_read(USBC_CHG_PROBE_CC2);
    const float dp   = usbc_chg_probe_read(USBC_CHG_PROBE_DP);
    const float dm   = usbc_chg_probe_read(USBC_CHG_PROBE_DM);

    print_voltage("VBUS", vbus); Serial.print("  ");
    print_voltage("CC1",  cc1);  Serial.print("  ");
    print_voltage("CC2",  cc2);  Serial.print("  ");
    print_voltage("D+",   dp);   Serial.print("  ");
    print_voltage("D-",   dm);
    Serial.println();
}

void usbc_chg_probe_test_all_probe_lines_open(void) {
    usbc_chg_probe_all_probe_lines_open();
}

void usbc_chg_probe_test_led_all_off(void) {
    usbc_chg_probe_led_all_off();
}

static bool serial_abort_requested(void) {
    while (Serial.available() > 0) {
        char c = Serial.read();
        if (c == 'q' || c == 'Q') {
            return true;
        }
    }
    return false;
}

static void print_wait_status(float vbus, float cc1, float cc2) {
    print_voltage("VBUS", vbus); Serial.print("  ");
    print_voltage("CC1",  cc1);  Serial.print("  ");
    print_voltage("CC2",  cc2);
    Serial.println();
}

static void blink_red(int count) {
    usbc_chg_probe_led_all_off();

    for (int i = 0; i < count; i++) {
        usbc_chg_probe_led_set(USBC_CHG_PROBE_LED_RED, true);
        delay(150);
        usbc_chg_probe_led_set(USBC_CHG_PROBE_LED_RED, false);
        delay(150);
    }
}

// -----------------------------------------------------------------------------
// Test 1:
// Board self-test with nothing connected to Type-C receptacle.
// -----------------------------------------------------------------------------

bool usbc_chg_probe_test_unconnected_self_check(void) {
    Serial.println();
    Serial.println("=== Test 1: unconnected self-check ===");
    Serial.println("Make sure nothing is connected to the Type-C receptacle.");
    Serial.println("This test also detects if the breakout board CC pull-downs were not removed.");
    Serial.println();

    usbc_chg_probe_led_all_off();
    usbc_chg_probe_all_probe_lines_open();
    delay(200);

    bool ok = true;

    Serial.println("[Initial open state]");
    usbc_chg_probe_test_print_all_voltages();
    Serial.println("Note: open CC/D+/D- nodes may float. Initial open voltage is informational only.");
    Serial.println();

    // High-side path check.
    // This checks D3/D4/D7/D9 high-side paths without enabling the low-side pins.
    // With nothing connected, CC1/CC2/D+/D- should all rise close to 5V.
    Serial.println("[High-side path check]");
    Serial.println("Enable only high-side/Rp pins. Low-side/Rd pins are Hi-Z.");
    Serial.println("Expected: CC1, CC2, D+, D- should be near 5V with nothing connected.");

    usbc_chg_probe_all_probe_lines_open();
    delay(100);

    // CC2 Rp high-side only: D3 -- 56k -- CC2
    pin_hiz(PIN_CC2_RD_5K1);
    pin_drive_high(PIN_CC2_RP_56K);

    // D+ high-side only: D4 -- 68k -- D+
    pin_hiz(PIN_DP_LO_10K);
    pin_drive_high(PIN_DP_HI_68K);

    // D- high-side only: D7 -- 68k -- D-
    pin_hiz(PIN_DM_LO_10K);
    pin_drive_high(PIN_DM_HI_68K);

    // CC1 Rp high-side only: D9 -- 56k -- CC1
    pin_hiz(PIN_CC1_RD_5K1);
    pin_drive_high(PIN_CC1_RP_56K);

    delay(200);

    ok &= expect_range("CC1 high-side", usbc_chg_probe_read(USBC_CHG_PROBE_CC1), CC_RP_OPEN_MIN_V, ADC_REF_V + 0.10f);
    ok &= expect_range("CC2 high-side", usbc_chg_probe_read(USBC_CHG_PROBE_CC2), CC_RP_OPEN_MIN_V, ADC_REF_V + 0.10f);
    ok &= expect_range("D+ high-side",  usbc_chg_probe_read(USBC_CHG_PROBE_DP),  CC_RP_OPEN_MIN_V, ADC_REF_V + 0.10f);
    ok &= expect_range("D- high-side",  usbc_chg_probe_read(USBC_CHG_PROBE_DM),  CC_RP_OPEN_MIN_V, ADC_REF_V + 0.10f);
    Serial.println();

    usbc_chg_probe_all_probe_lines_open();
    delay(100);

    // Low-side / Rd-side path check.
    // This intentionally drives the normally-low pins HIGH.
    // With nothing connected, CC1/CC2/D+/D- should all rise close to 5V.
    //
    // CC2: D2 -- 5.1k -- CC2
    // D+ : D5 -- 10k  -- D+
    // D- : D6 -- 10k  -- D-
    // CC1: D8 -- 5.1k -- CC1
    Serial.println("[Low-side/Rd-side HIGH path check]");
    Serial.println("Enable only low-side/Rd-side pins as HIGH. High-side/Rp pins are Hi-Z.");
    Serial.println("Expected: CC1, CC2, D+, D- should be near 5V with nothing connected.");

    usbc_chg_probe_all_probe_lines_open();
    delay(100);

    // CC2 Rd-side only: D2 -- 5.1k -- CC2, driven HIGH for test
    pin_hiz(PIN_CC2_RP_56K);
    pin_drive_high(PIN_CC2_RD_5K1);

    // D+ low/PWM-side only: D5 -- 10k -- D+, driven HIGH for test
    pin_hiz(PIN_DP_HI_68K);
    pin_drive_high(PIN_DP_LO_10K);

    // D- low/PWM-side only: D6 -- 10k -- D-, driven HIGH for test
    pin_hiz(PIN_DM_HI_68K);
    pin_drive_high(PIN_DM_LO_10K);

    // CC1 Rd-side only: D8 -- 5.1k -- CC1, driven HIGH for test
    pin_hiz(PIN_CC1_RP_56K);
    pin_drive_high(PIN_CC1_RD_5K1);

    delay(200);

    ok &= expect_range("CC1 low/Rd-side HIGH", usbc_chg_probe_read(USBC_CHG_PROBE_CC1), CC_RP_OPEN_MIN_V, ADC_REF_V + 0.10f);
    ok &= expect_range("CC2 low/Rd-side HIGH", usbc_chg_probe_read(USBC_CHG_PROBE_CC2), CC_RP_OPEN_MIN_V, ADC_REF_V + 0.10f);
    ok &= expect_range("D+ low-side HIGH",     usbc_chg_probe_read(USBC_CHG_PROBE_DP),  CC_RP_OPEN_MIN_V, ADC_REF_V + 0.10f);
    ok &= expect_range("D- low-side HIGH",     usbc_chg_probe_read(USBC_CHG_PROBE_DM),  CC_RP_OPEN_MIN_V, ADC_REF_V + 0.10f);
    Serial.println();

    usbc_chg_probe_all_probe_lines_open();
    delay(100);

    // CC Rd check.
    Serial.println("[CC Rd check]");
    usbc_chg_probe_set(USBC_CHG_PROBE_CC1, USBC_CHG_PROBE_RD);
    usbc_chg_probe_set(USBC_CHG_PROBE_CC2, USBC_CHG_PROBE_RD);
    delay(100);

    ok &= expect_range("CC1 Rd", usbc_chg_probe_read(USBC_CHG_PROBE_CC1), 0.0f, CC_LOW_MAX_V);
    ok &= expect_range("CC2 Rd", usbc_chg_probe_read(USBC_CHG_PROBE_CC2), 0.0f, CC_LOW_MAX_V);
    Serial.println();

    // CC Rp check.
    // With no external connection and no built-in breakout pull-down,
    // CC should rise close to 5V through 56k.
    Serial.println("[CC Rp default check]");
    usbc_chg_probe_set(USBC_CHG_PROBE_CC1, USBC_CHG_PROBE_RP_DEFAULT);
    usbc_chg_probe_set(USBC_CHG_PROBE_CC2, USBC_CHG_PROBE_RP_DEFAULT);
    delay(100);

    ok &= expect_range("CC1 Rp open", usbc_chg_probe_read(USBC_CHG_PROBE_CC1), CC_RP_OPEN_MIN_V, ADC_REF_V + 0.10f);
    ok &= expect_range("CC2 Rp open", usbc_chg_probe_read(USBC_CHG_PROBE_CC2), CC_RP_OPEN_MIN_V, ADC_REF_V + 0.10f);
    Serial.println();

    // D+ 0.6V check.
    Serial.println("[D+ fixed 0.6V check]");
    usbc_chg_probe_set(USBC_CHG_PROBE_DP, USBC_CHG_PROBE_0V6);
    delay(100);
    ok &= expect_range("D+ 0.6V", usbc_chg_probe_read(USBC_CHG_PROBE_DP), DLINE_0V6_MIN_V, DLINE_0V6_MAX_V);
    Serial.println();

    // D- 0.6V check.
    Serial.println("[D- fixed 0.6V check]");
    usbc_chg_probe_set(USBC_CHG_PROBE_DM, USBC_CHG_PROBE_0V6);
    delay(100);
    ok &= expect_range("D- 0.6V", usbc_chg_probe_read(USBC_CHG_PROBE_DM), DLINE_0V6_MIN_V, DLINE_0V6_MAX_V);
    Serial.println();

    // D+ PWM check at 1.2V.
    Serial.println("[D+ PWM 1.2V check]");
    usbc_chg_probe_set(USBC_CHG_PROBE_DP, 1.2f);
    delay(200);
    ok &= expect_range("D+ PWM 1.2V", usbc_chg_probe_read(USBC_CHG_PROBE_DP), PWM_1V2_MIN_V, PWM_1V2_MAX_V);
    Serial.println();

    // D- PWM check at 1.2V.
    Serial.println("[D- PWM 1.2V check]");
    usbc_chg_probe_set(USBC_CHG_PROBE_DM, 1.2f);
    delay(200);
    ok &= expect_range("D- PWM 1.2V", usbc_chg_probe_read(USBC_CHG_PROBE_DM), PWM_1V2_MIN_V, PWM_1V2_MAX_V);
    Serial.println();

    // Discharge check.
    Serial.println("[D+/D- discharge check]");
    usbc_chg_probe_set(USBC_CHG_PROBE_DP, USBC_CHG_PROBE_DISCHARGE);
    usbc_chg_probe_set(USBC_CHG_PROBE_DM, USBC_CHG_PROBE_DISCHARGE);
    delay(100);

    ok &= expect_range("D+ discharge", usbc_chg_probe_read(USBC_CHG_PROBE_DP), 0.0f, DLINE_LOW_MAX_V);
    ok &= expect_range("D- discharge", usbc_chg_probe_read(USBC_CHG_PROBE_DM), 0.0f, DLINE_LOW_MAX_V);
    Serial.println();

    usbc_chg_probe_all_probe_lines_open();

    if (ok) {
        Serial.println("Test 1 result: PASS");
        usbc_chg_probe_led_set(USBC_CHG_PROBE_LED_GREEN, true);
    } else {
        Serial.println("Test 1 result: FAIL");
        Serial.println("Red LED blinking.");
        blink_red(10);
    }

    Serial.println();
    return ok;
}

// -----------------------------------------------------------------------------
// Test 2:
// Sink attach test.
// Set CC1/CC2 to Rd, ask the user to connect a Source.
// End when VBUS and CC attach-like voltage are detected.
// -----------------------------------------------------------------------------

static bool cc_attach_like(float cc_v) {
    return cc_v >= CC_ATTACH_MIN_V && cc_v <= CC_ATTACH_MAX_V;
}

bool usbc_chg_probe_test_sink_attach_wait(void) {
    Serial.println();
    Serial.println("=== Test 2: Sink attach / VBUS wait ===");
    Serial.println("The probe will set CC1 and CC2 to Rd.");
    Serial.println("Connect a USB-C Source to the Type-C receptacle.");
    Serial.println("Press 'q' to abort.");
    Serial.println();

    usbc_chg_probe_led_all_off();

    usbc_chg_probe_all_probe_lines_open();
    usbc_chg_probe_set(USBC_CHG_PROBE_CC1, USBC_CHG_PROBE_RD);
    usbc_chg_probe_set(USBC_CHG_PROBE_CC2, USBC_CHG_PROBE_RD);
    usbc_chg_probe_set(USBC_CHG_PROBE_DP, USBC_CHG_PROBE_OPEN);
    usbc_chg_probe_set(USBC_CHG_PROBE_DM, USBC_CHG_PROBE_OPEN);

    bool success = false;
    const unsigned long start_ms = millis();
    bool last_vbus_present = false;
    bool last_cc_attached  = false;
    bool have_last_state   = false;
    unsigned long last_print_ms = 0;

    while (millis() - start_ms < ATTACH_WAIT_TIMEOUT_MS) {
        if (serial_abort_requested()) {
            Serial.println("Aborted by user.");
            break;
        }

        const float vbus = usbc_chg_probe_read(USBC_CHG_PROBE_VBUS);
        const float cc1  = usbc_chg_probe_read(USBC_CHG_PROBE_CC1);
        const float cc2  = usbc_chg_probe_read(USBC_CHG_PROBE_CC2);

        const bool vbus_present = vbus >= VBUS_PRESENT_MIN_V;
        const bool cc_attached  = cc_attach_like(cc1) || cc_attach_like(cc2);

        const unsigned long now = millis();

        const bool state_changed =
            !have_last_state ||
            (vbus_present != last_vbus_present) ||
            (cc_attached  != last_cc_attached);

        const bool periodic_print =
            (now - last_print_ms) >= WAIT_PRINT_INTERVAL_MS;

        if (state_changed || periodic_print) {
            print_wait_status(vbus, cc1, cc2);

            if (vbus_present && !cc_attached) {
                Serial.println("  VBUS present, but CC voltage is not attach-like yet.");
            }

            last_print_ms = now;
            last_vbus_present = vbus_present;
            last_cc_attached = cc_attached;
            have_last_state = true;
        }

        if (vbus_present && cc_attached) {
            Serial.println("Source attach detected.");
            success = true;
            break;
        }

        delay(200);
    }

    if (success) {
        usbc_chg_probe_led_set(USBC_CHG_PROBE_LED_GREEN, true);
        Serial.println("Test 2 result: PASS");
    } else {
        Serial.println("Test 2 result: ERROR");
        Serial.println("Source attach was not detected, or the test was aborted/timed out.");
        blink_red(10);
    }

    Serial.println();
    return success;
}

// -----------------------------------------------------------------------------
// Test 3:
// BC1.2-style D+ 0.6V / D- response check.
//
// Intended use:
//   - Connect a Type-A BC1.2-capable charger through an A-to-C cable.
//   - The A-to-C cable should provide Rp on CC.
//   - This probe provides Rd on CC1/CC2.
//   - After VBUS is detected, the probe drives D+ to ~0.6V and observes D-.
//
// Notes:
//   - D- ~= 0.6V means "charging-port-like response".
//   - This alone does not distinguish CDP from DCP.
//   - VBUS absent is treated as an error.
//   - VBUS present but no D- response is reported as "no response", not a board error.
// -----------------------------------------------------------------------------

bool usbc_chg_probe_test_bc12_dp_0v6_dm_response(void) {
    Serial.println();
    Serial.println("=== Test 3: BC1.2 D+ 0.6V / D- response check ===");
    Serial.println("Use a Type-A charger with a Type-A to Type-C cable.");
    Serial.println("The probe will set CC1/CC2 to Rd, wait for VBUS,");
    Serial.println("then drive D+ to about 0.6V and read D-.");
    Serial.println("Press 'q' to abort.");
    Serial.println();

    usbc_chg_probe_led_all_off();

    // Start from a safe state.
    usbc_chg_probe_all_probe_lines_open();

    // DP/DM must be open before VBUS arrives.
    usbc_chg_probe_set(USBC_CHG_PROBE_DP, USBC_CHG_PROBE_OPEN);
    usbc_chg_probe_set(USBC_CHG_PROBE_DM, USBC_CHG_PROBE_OPEN);

    // Behave as a Type-C sink.
    usbc_chg_probe_set(USBC_CHG_PROBE_CC1, USBC_CHG_PROBE_RD);
    usbc_chg_probe_set(USBC_CHG_PROBE_CC2, USBC_CHG_PROBE_RD);

    Serial.println("Waiting for VBUS...");
    Serial.println();

    const unsigned long start_ms = millis();
    bool vbus_ok = false;
    bool last_vbus_present = false;
    bool have_last_state   = false;
    unsigned long last_print_ms = 0;

    while (millis() - start_ms < BC12_VBUS_WAIT_TIMEOUT_MS) {
        if (serial_abort_requested()) {
            Serial.println("Aborted by user.");
            usbc_chg_probe_all_probe_lines_open();
            blink_red(10);
            return false;
        }

        const float vbus = usbc_chg_probe_read(USBC_CHG_PROBE_VBUS);
        const float cc1  = usbc_chg_probe_read(USBC_CHG_PROBE_CC1);
        const float cc2  = usbc_chg_probe_read(USBC_CHG_PROBE_CC2);

        const bool vbus_present = vbus >= VBUS_PRESENT_MIN_V;
        const unsigned long now = millis();

        const bool state_changed =
            !have_last_state ||
            (vbus_present != last_vbus_present);

        const bool periodic_print =
            (now - last_print_ms) >= WAIT_PRINT_INTERVAL_MS;

        if (state_changed || periodic_print) {
            print_wait_status(vbus, cc1, cc2);

            last_print_ms = now;
            last_vbus_present = vbus_present;
            have_last_state = true;
        }

        if (vbus_present) {
            vbus_ok = true;
            break;
        }

        delay(200);
    }

    if (!vbus_ok) {
        Serial.println();
        Serial.println("Test 3 result: ERROR");
        Serial.println("VBUS was not detected.");
        Serial.println("Check the charger, cable, CC wiring, and GND connection.");
        usbc_chg_probe_all_probe_lines_open();
        blink_red(10);
        return false;
    }

    Serial.println();
    Serial.println("VBUS detected. Starting D+/D- check...");

    // Let the charger/cable/port settle after VBUS appears.
    delay(300);

    // Ensure D- is not driven by this probe.
    usbc_chg_probe_set(USBC_CHG_PROBE_DM, USBC_CHG_PROBE_OPEN);

    // Drive D+ to approximately 0.6V using the fixed 68k/10k divider.
    usbc_chg_probe_set(USBC_CHG_PROBE_DP, USBC_CHG_PROBE_0V6);

    // Allow D+ RC and charger response to settle.
    delay(300);

    const float vbus = usbc_chg_probe_read(USBC_CHG_PROBE_VBUS);
    const float cc1  = usbc_chg_probe_read(USBC_CHG_PROBE_CC1);
    const float cc2  = usbc_chg_probe_read(USBC_CHG_PROBE_CC2);
    const float dp   = usbc_chg_probe_read(USBC_CHG_PROBE_DP);
    const float dm   = usbc_chg_probe_read(USBC_CHG_PROBE_DM);

    Serial.println();
    Serial.println("[BC1.2-style response]");
    print_voltage("VBUS", vbus); Serial.print("  ");
    print_voltage("CC1",  cc1);  Serial.print("  ");
    print_voltage("CC2",  cc2);  Serial.print("  ");
    print_voltage("D+",   dp);   Serial.print("  ");
    print_voltage("D-",   dm);
    Serial.println();

    bool dp_ok = in_range(dp, DLINE_0V6_MIN_V, DLINE_0V6_MAX_V);
    bool dm_response = in_range(dm, BC12_DM_RESPONSE_MIN_V, BC12_DM_RESPONSE_MAX_V);

    Serial.print("D+ drive check: ");
    Serial.print(dp, 3);
    Serial.print(" V, expected ");
    Serial.print(DLINE_0V6_MIN_V, 2);
    Serial.print("..");
    Serial.print(DLINE_0V6_MAX_V, 2);
    Serial.print(" V -> ");
    Serial.println(dp_ok ? "OK" : "FAIL");

    Serial.print("D- response check: ");
    Serial.print(dm, 3);
    Serial.print(" V, expected ");
    Serial.print(BC12_DM_RESPONSE_MIN_V, 2);
    Serial.print("..");
    Serial.print(BC12_DM_RESPONSE_MAX_V, 2);
    Serial.print(" V -> ");
    Serial.println(dm_response ? "RESPONSE" : "NO RESPONSE");

    Serial.println();

    if (!dp_ok) {
        Serial.println("Test 3 result: ERROR");
        Serial.println("D+ was not driven to the expected 0.6V range.");
        Serial.println("This suggests a probe wiring or firmware issue.");
        blink_red(10);
        return false;
    }

    if (dm_response) {
        Serial.println("Test 3 result: PASS-like");
        Serial.println("D- responded around 0.6V.");
        Serial.println("This is charging-port-like behavior, but this test alone does not distinguish CDP from DCP.");
        usbc_chg_probe_led_set(USBC_CHG_PROBE_LED_GREEN, true);
        return true;
    }

    Serial.println("Test 3 result: DONE, no D- response");
    Serial.println("VBUS was present and D+ was driven, but D- did not respond around 0.6V.");
    Serial.println("This may be SDP-like/no-BC1.2 response, or the charger/cable may not expose that behavior.");

    return false;
}
