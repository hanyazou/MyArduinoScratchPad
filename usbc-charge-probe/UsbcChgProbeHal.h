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

#ifndef USBC_CHG_PROBE_HAL_H
#define USBC_CHG_PROBE_HAL_H

#include <Arduino.h>

/*
 * USB-C Charge Probe
 *
 * A0 = VBUS sense, 100k/22k divider
 * A1 = CC2
 * A2 = CC1
 * A3 = D-
 * A4 = D+
 *
 * D2 = CC2 Rd 5.1k
 * D3 = CC2 Rp 56k, PWM-capable
 *
 * D4 = D+ 68k high-side
 * D5 = D+ 10k low/PWM-side, PWM-capable
 *
 * D6 = D- 10k low/PWM-side, PWM-capable
 * D7 = D- 68k high-side
 *
 * D8 = CC1 Rd 5.1k
 * D9 = CC1 Rp 56k, PWM-capable
 *
 * LED:
 * D11 = red,   active low
 * D12 = green, active low
 *
 * I2C:
 * D10 = SDA
 * A5 = SCL
 */

// -----------------------------------------------------------------------------
// Configuration
// -----------------------------------------------------------------------------

static const float ADC_REF_V = 5.0f;

// UNO R4 supports analogReadResolution().
// Use 12-bit here. This is enough and keeps numbers simple.
static const int ADC_BITS = 12;
static const int ADC_MAX  = (1 << ADC_BITS) - 1;

// VBUS divider: VBUS -- 100k -- A0 -- 22k -- GND
static const float VBUS_DIVIDER_SCALE = (100.0f + 22.0f) / 22.0f;

// Tolerances for self-test.
// These are intentionally loose because resistor tolerance, GPIO voltage,
// ADC reference, and capacitor settling are not precision-instrument grade.
static const float CC_LOW_MAX_V       = 0.25f;
static const float CC_RP_OPEN_MIN_V   = 4.00f;
static const float DLINE_0V6_MIN_V    = 0.45f;
static const float DLINE_0V6_MAX_V    = 0.85f;
static const float DLINE_LOW_MAX_V    = 0.25f;
static const float PWM_1V2_MIN_V      = 0.85f;
static const float PWM_1V2_MAX_V      = 1.60f;

static const float VBUS_PRESENT_MIN_V = 4.0f;
static const float CC_ATTACH_MIN_V    = 0.15f;
static const float CC_ATTACH_MAX_V    = 2.60f;

static const float BC12_DM_RESPONSE_MIN_V = 0.45f;
static const float BC12_DM_RESPONSE_MAX_V = 0.85f;

static const unsigned long ATTACH_WAIT_TIMEOUT_MS = 5UL * 60UL * 1000UL;
static const unsigned long BC12_VBUS_WAIT_TIMEOUT_MS = 5UL * 60UL * 1000UL;
static const unsigned long WAIT_PRINT_INTERVAL_MS = 5000UL;

// -----------------------------------------------------------------------------
// Pin assignment
// -----------------------------------------------------------------------------

static const int PIN_ADC_VBUS = A0;
static const int PIN_ADC_CC1  = A2;
static const int PIN_ADC_DM   = A3;
static const int PIN_ADC_DP   = A4;

// A5 is connected to the CC2 node on the shield, but the UNO R4 A5/SCL ADC
// reading showed an offset-like behavior at low voltage.  CC2 is read via A1
// using a jumper from the CC2 node.
//static const int PIN_ADC_CC2  = A5;  // A5 is physically connected to CC2 node but not used as ADC.
static const int PIN_ADC_CC2  = A1;  // jumper from CC2/A5 node

// A1 is not connected. Do not use.

// CC2
static const int PIN_CC2_RD_5K1 = 2;
static const int PIN_CC2_RP_56K = 3;

// D+
static const int PIN_DP_HI_68K = 4;
static const int PIN_DP_LO_10K = 5;

// D-
static const int PIN_DM_LO_10K = 6;
static const int PIN_DM_HI_68K = 7;

// CC1
static const int PIN_CC1_RD_5K1 = 8;
static const int PIN_CC1_RP_56K = 9;

// LED, active low
static const int PIN_LED_RED   = 11;
static const int PIN_LED_GREEN = 12;

// -----------------------------------------------------------------------------
// Public API types
// -----------------------------------------------------------------------------

enum usbc_chg_probe_signal_t {
  USBC_CHG_PROBE_CC1,
  USBC_CHG_PROBE_CC2,
  USBC_CHG_PROBE_DP,
  USBC_CHG_PROBE_DM,
  USBC_CHG_PROBE_VBUS,
};

enum usbc_chg_probe_state_t {
  USBC_CHG_PROBE_OPEN,

  // CC1/CC2 only
  USBC_CHG_PROBE_RD,
  USBC_CHG_PROBE_RP_DEFAULT,

  // DP/DM only
  USBC_CHG_PROBE_0V6,
  USBC_CHG_PROBE_DISCHARGE,
};

enum usbc_chg_probe_led_t {
  USBC_CHG_PROBE_LED_GREEN,
  USBC_CHG_PROBE_LED_RED,
};

void usbc_chg_probe_init(void);
bool usbc_chg_probe_set(usbc_chg_probe_signal_t sig, usbc_chg_probe_state_t state);
bool usbc_chg_probe_set(usbc_chg_probe_signal_t sig, float voltage);
void usbc_chg_probe_all_probe_lines_open(void);
float usbc_chg_probe_read(usbc_chg_probe_signal_t sig);

void usbc_chg_probe_led_set(usbc_chg_probe_led_t led, bool on);
void usbc_chg_probe_led_all_off(void);

#endif /* USBC_CHG_PROBE_HAL_H */
