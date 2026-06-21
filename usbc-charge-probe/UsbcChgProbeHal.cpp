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

#include "UsbcChgProbeHal.h"
#include "Util.h"

// -----------------------------------------------------------------------------
// Low-level helpers
// -----------------------------------------------------------------------------

static bool is_cc(usbc_chg_probe_signal_t sig) {
    return sig == USBC_CHG_PROBE_CC1 || sig == USBC_CHG_PROBE_CC2;
}

static bool is_dline(usbc_chg_probe_signal_t sig) {
    return sig == USBC_CHG_PROBE_DP || sig == USBC_CHG_PROBE_DM;
}

static int adc_pin_of(usbc_chg_probe_signal_t sig) {
    switch (sig) {
    case USBC_CHG_PROBE_CC1:  return PIN_ADC_CC1;
    case USBC_CHG_PROBE_CC2:  return PIN_ADC_CC2;
    case USBC_CHG_PROBE_DP:   return PIN_ADC_DP;
    case USBC_CHG_PROBE_DM:   return PIN_ADC_DM;
    case USBC_CHG_PROBE_VBUS: return PIN_ADC_VBUS;
    default:              return -1;
    }
}

static int cc_rp_pin(usbc_chg_probe_signal_t sig) {
    switch (sig) {
    case USBC_CHG_PROBE_CC1: return PIN_CC1_RP_56K;
    case USBC_CHG_PROBE_CC2: return PIN_CC2_RP_56K;
    default:             return -1;
    }
}

static int cc_rd_pin(usbc_chg_probe_signal_t sig) {
    switch (sig) {
    case USBC_CHG_PROBE_CC1: return PIN_CC1_RD_5K1;
    case USBC_CHG_PROBE_CC2: return PIN_CC2_RD_5K1;
    default:             return -1;
    }
}

static int dline_hi_pin(usbc_chg_probe_signal_t sig) {
    switch (sig) {
    case USBC_CHG_PROBE_DP: return PIN_DP_HI_68K;
    case USBC_CHG_PROBE_DM: return PIN_DM_HI_68K;
    default:            return -1;
    }
}

static int dline_lo_pin(usbc_chg_probe_signal_t sig) {
    switch (sig) {
    case USBC_CHG_PROBE_DP: return PIN_DP_LO_10K;
    case USBC_CHG_PROBE_DM: return PIN_DM_LO_10K;
    default:            return -1;
    }
}

static const char *signal_name(usbc_chg_probe_signal_t sig) {
    switch (sig) {
    case USBC_CHG_PROBE_CC1:  return "CC1";
    case USBC_CHG_PROBE_CC2:  return "CC2";
    case USBC_CHG_PROBE_DP:   return "D+";
    case USBC_CHG_PROBE_DM:   return "D-";
    case USBC_CHG_PROBE_VBUS: return "VBUS";
    default:              return "?";
    }
}

static void led_write_raw(int pin, bool on) {
    // LED anode is tied to 3.3V.
    // GPIO LOW turns LED on.
    digitalWrite(pin, on ? LOW : HIGH);
}

void usbc_chg_probe_led_set(usbc_chg_probe_led_t led, bool on) {
    switch (led) {
    case USBC_CHG_PROBE_LED_GREEN: led_write_raw(PIN_LED_GREEN, on); break;
    case USBC_CHG_PROBE_LED_RED:   led_write_raw(PIN_LED_RED,   on); break;
    }
}

void usbc_chg_probe_led_all_off(void) {
    usbc_chg_probe_led_set(USBC_CHG_PROBE_LED_RED, false);
    usbc_chg_probe_led_set(USBC_CHG_PROBE_LED_GREEN, false);
}

// -----------------------------------------------------------------------------
// Public API implementation
// -----------------------------------------------------------------------------

void usbc_chg_probe_init(void) {
    analogReadResolution(ADC_BITS);

    // All probe control pins Hi-Z first.
    pinMode(PIN_CC2_RD_5K1, INPUT);
    pinMode(PIN_CC2_RP_56K, INPUT);
    pinMode(PIN_DP_HI_68K,  INPUT);
    pinMode(PIN_DP_LO_10K,  INPUT);
    pinMode(PIN_DM_LO_10K,  INPUT);
    pinMode(PIN_DM_HI_68K,  INPUT);
    pinMode(PIN_CC1_RD_5K1, INPUT);
    pinMode(PIN_CC1_RP_56K, INPUT);

    // ADC pins as input.
    pinMode(PIN_ADC_VBUS, INPUT);
    pinMode(PIN_ADC_CC1,  INPUT);
    pinMode(PIN_ADC_DM,   INPUT);
    pinMode(PIN_ADC_DP,   INPUT);
    pinMode(PIN_ADC_CC2,  INPUT);

    // LEDs.
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_RED,   OUTPUT);
    usbc_chg_probe_led_all_off();
}

bool usbc_chg_probe_set(usbc_chg_probe_signal_t sig, usbc_chg_probe_state_t state) {
    if (sig == USBC_CHG_PROBE_VBUS) {
        return false; // VBUS is read-only.
    }

    if (is_cc(sig)) {
        const int rp = cc_rp_pin(sig);
        const int rd = cc_rd_pin(sig);

        switch (state) {
        case USBC_CHG_PROBE_OPEN:
            pin_hiz(rp);
            pin_hiz(rd);
            return true;

        case USBC_CHG_PROBE_RD:
            // Make Rp Hi-Z first, then enable Rd.
            pin_hiz(rp);
            pin_drive_low(rd);
            return true;

        case USBC_CHG_PROBE_RP_DEFAULT:
            // Make Rd Hi-Z first, then enable Rp.
            pin_hiz(rd);
            pin_drive_high(rp);
            return true;

        default:
            return false;
        }
    }

    if (is_dline(sig)) {
        const int hi = dline_hi_pin(sig); // 68k side
        const int lo = dline_lo_pin(sig); // 10k side, PWM-capable

        switch (state) {
        case USBC_CHG_PROBE_OPEN:
            pin_hiz(hi);
            pin_hiz(lo);
            return true;

        case USBC_CHG_PROBE_0V6:
            // 5V -- 68k -- node -- 10k -- GND
            pin_drive_low(lo);
            pin_drive_high(hi);
            return true;

        case USBC_CHG_PROBE_DISCHARGE:
            // Discharge smoothing capacitor through 10k.
            pin_hiz(hi);
            pin_drive_low(lo);
            return true;

        default:
            return false;
        }
    }

    return false;
}

bool usbc_chg_probe_set(usbc_chg_probe_signal_t sig, float voltage) {
    if (!is_dline(sig)) {
        return false;
    }

    const int hi = dline_hi_pin(sig);
    const int lo = dline_lo_pin(sig);

    if (voltage < 0.0f) {
        voltage = 0.0f;
    }
    if (voltage > ADC_REF_V) {
        voltage = ADC_REF_V;
    }

    // PWM mode:
    // 68k side is Hi-Z.
    // 10k side is PWM, smoothed by Cdp/Cdm.
    pin_hiz(hi);

    const int duty = (int)(voltage * 255.0f / ADC_REF_V + 0.5f);
    pinMode(lo, OUTPUT);
    analogWrite(lo, constrain(duty, 0, 255));

    return true;
}

void usbc_chg_probe_all_probe_lines_open(void) {
    usbc_chg_probe_set(USBC_CHG_PROBE_CC1, USBC_CHG_PROBE_OPEN);
    usbc_chg_probe_set(USBC_CHG_PROBE_CC2, USBC_CHG_PROBE_OPEN);
    usbc_chg_probe_set(USBC_CHG_PROBE_DP,  USBC_CHG_PROBE_OPEN);
    usbc_chg_probe_set(USBC_CHG_PROBE_DM,  USBC_CHG_PROBE_OPEN);
}

float usbc_chg_probe_read(usbc_chg_probe_signal_t sig) {
    const int pin = adc_pin_of(sig);
    if (pin < 0) {
        return NAN;
    }

    // Throw away the first sample after channel selection.
    analogRead(pin);
    delayMicroseconds(500);

    const int n = 16;
    uint32_t sum = 0;

    for (int i = 0; i < n; i++) {
        sum += analogRead(pin);
        delayMicroseconds(200);
    }

    const float raw = (float)sum / (float)n;
    float v = raw * ADC_REF_V / (float)ADC_MAX;

    if (sig == USBC_CHG_PROBE_VBUS) {
        v *= VBUS_DIVIDER_SCALE;
    }

    return v;
}
