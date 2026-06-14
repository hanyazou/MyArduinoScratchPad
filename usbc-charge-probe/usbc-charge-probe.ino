#include <Arduino.h>

/*
 * USB-C Charge Probe
 *
 * Final wiring:
 *
 * A0 = VBUS sense, 100k/22k divider
 * A1 = NC
 * A2 = CC1
 * A3 = D-
 * A4 = D+
 * A5 = CC2
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
 * D10 = blue,  active low
 * D11 = red,   active low
 * D12 = green, active low
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
static const int PIN_LED_BLUE  = 10;
static const int PIN_LED_RED   = 11;
static const int PIN_LED_GREEN = 12;

// -----------------------------------------------------------------------------
// Public API types
// -----------------------------------------------------------------------------

enum usbc_probe_signal_t {
  USBC_PROBE_CC1,
  USBC_PROBE_CC2,
  USBC_PROBE_DP,
  USBC_PROBE_DM,
  USBC_PROBE_VBUS,
};

enum usbc_probe_state_t {
  USBC_PROBE_OPEN,

  // CC1/CC2 only
  USBC_PROBE_RD,
  USBC_PROBE_RP_DEFAULT,

  // DP/DM only
  USBC_PROBE_0V6,
  USBC_PROBE_DISCHARGE,
};

enum usbc_probe_led_t {
  USBC_PROBE_LED_GREEN,
  USBC_PROBE_LED_RED,
  USBC_PROBE_LED_BLUE,
};

// -----------------------------------------------------------------------------
// Low-level helpers
// -----------------------------------------------------------------------------

static void pin_hiz(int pin) {
  digitalWrite(pin, LOW);   // disable input pull-up / clear output latch
  pinMode(pin, INPUT);
}

static void pin_drive_high(int pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH);
}

static void pin_drive_low(int pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
}

static bool is_cc(usbc_probe_signal_t sig) {
  return sig == USBC_PROBE_CC1 || sig == USBC_PROBE_CC2;
}

static bool is_dline(usbc_probe_signal_t sig) {
  return sig == USBC_PROBE_DP || sig == USBC_PROBE_DM;
}

static int adc_pin_of(usbc_probe_signal_t sig) {
  switch (sig) {
    case USBC_PROBE_CC1:  return PIN_ADC_CC1;
    case USBC_PROBE_CC2:  return PIN_ADC_CC2;
    case USBC_PROBE_DP:   return PIN_ADC_DP;
    case USBC_PROBE_DM:   return PIN_ADC_DM;
    case USBC_PROBE_VBUS: return PIN_ADC_VBUS;
    default:              return -1;
  }
}

static int cc_rp_pin(usbc_probe_signal_t sig) {
  switch (sig) {
    case USBC_PROBE_CC1: return PIN_CC1_RP_56K;
    case USBC_PROBE_CC2: return PIN_CC2_RP_56K;
    default:             return -1;
  }
}

static int cc_rd_pin(usbc_probe_signal_t sig) {
  switch (sig) {
    case USBC_PROBE_CC1: return PIN_CC1_RD_5K1;
    case USBC_PROBE_CC2: return PIN_CC2_RD_5K1;
    default:             return -1;
  }
}

static int dline_hi_pin(usbc_probe_signal_t sig) {
  switch (sig) {
    case USBC_PROBE_DP: return PIN_DP_HI_68K;
    case USBC_PROBE_DM: return PIN_DM_HI_68K;
    default:            return -1;
  }
}

static int dline_lo_pin(usbc_probe_signal_t sig) {
  switch (sig) {
    case USBC_PROBE_DP: return PIN_DP_LO_10K;
    case USBC_PROBE_DM: return PIN_DM_LO_10K;
    default:            return -1;
  }
}

static const char *signal_name(usbc_probe_signal_t sig) {
  switch (sig) {
    case USBC_PROBE_CC1:  return "CC1";
    case USBC_PROBE_CC2:  return "CC2";
    case USBC_PROBE_DP:   return "D+";
    case USBC_PROBE_DM:   return "D-";
    case USBC_PROBE_VBUS: return "VBUS";
    default:              return "?";
  }
}

static void led_write_raw(int pin, bool on) {
  // LED anode is tied to 3.3V.
  // GPIO LOW turns LED on.
  digitalWrite(pin, on ? LOW : HIGH);
}

void usbc_probe_led_set(usbc_probe_led_t led, bool on) {
  switch (led) {
    case USBC_PROBE_LED_GREEN: led_write_raw(PIN_LED_GREEN, on); break;
    case USBC_PROBE_LED_RED:   led_write_raw(PIN_LED_RED,   on); break;
    case USBC_PROBE_LED_BLUE:  led_write_raw(PIN_LED_BLUE,  on); break;
  }
}

void usbc_probe_led_rgb(bool red, bool green, bool blue) {
  usbc_probe_led_set(USBC_PROBE_LED_RED, red);
  usbc_probe_led_set(USBC_PROBE_LED_GREEN, green);
  usbc_probe_led_set(USBC_PROBE_LED_BLUE, blue);
}

void usbc_probe_led_all_off(void) {
  usbc_probe_led_rgb(false, false, false);
}

static void blink_red(int count) {
  usbc_probe_led_rgb(false, false, false);

  for (int i = 0; i < count; i++) {
    usbc_probe_led_set(USBC_PROBE_LED_RED, true);
    delay(150);
    usbc_probe_led_set(USBC_PROBE_LED_RED, false);
    delay(150);
  }
}

// -----------------------------------------------------------------------------
// Public API implementation
// -----------------------------------------------------------------------------

void usbc_probe_init(void) {
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
  pinMode(PIN_LED_BLUE,  OUTPUT);
  usbc_probe_led_all_off();
}

bool usbc_probe_set(usbc_probe_signal_t sig, usbc_probe_state_t state) {
  if (sig == USBC_PROBE_VBUS) {
    return false; // VBUS is read-only.
  }

  if (is_cc(sig)) {
    const int rp = cc_rp_pin(sig);
    const int rd = cc_rd_pin(sig);

    switch (state) {
      case USBC_PROBE_OPEN:
        pin_hiz(rp);
        pin_hiz(rd);
        return true;

      case USBC_PROBE_RD:
        // Make Rp Hi-Z first, then enable Rd.
        pin_hiz(rp);
        pin_drive_low(rd);
        return true;

      case USBC_PROBE_RP_DEFAULT:
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
      case USBC_PROBE_OPEN:
        pin_hiz(hi);
        pin_hiz(lo);
        return true;

      case USBC_PROBE_0V6:
        // 5V -- 68k -- node -- 10k -- GND
        pin_drive_low(lo);
        pin_drive_high(hi);
        return true;

      case USBC_PROBE_DISCHARGE:
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

bool usbc_probe_set(usbc_probe_signal_t sig, float voltage) {
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

float usbc_probe_read(usbc_probe_signal_t sig) {
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

  if (sig == USBC_PROBE_VBUS) {
    v *= VBUS_DIVIDER_SCALE;
  }

  return v;
}

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

static void print_all_voltages(void) {
  const float vbus = usbc_probe_read(USBC_PROBE_VBUS);
  const float cc1  = usbc_probe_read(USBC_PROBE_CC1);
  const float cc2  = usbc_probe_read(USBC_PROBE_CC2);
  const float dp   = usbc_probe_read(USBC_PROBE_DP);
  const float dm   = usbc_probe_read(USBC_PROBE_DM);

  print_voltage("VBUS", vbus); Serial.print("  ");
  print_voltage("CC1",  cc1);  Serial.print("  ");
  print_voltage("CC2",  cc2);  Serial.print("  ");
  print_voltage("D+",   dp);   Serial.print("  ");
  print_voltage("D-",   dm);
  Serial.println();
}

static void all_probe_lines_open(void) {
  usbc_probe_set(USBC_PROBE_CC1, USBC_PROBE_OPEN);
  usbc_probe_set(USBC_PROBE_CC2, USBC_PROBE_OPEN);
  usbc_probe_set(USBC_PROBE_DP,  USBC_PROBE_OPEN);
  usbc_probe_set(USBC_PROBE_DM,  USBC_PROBE_OPEN);
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

// -----------------------------------------------------------------------------
// Test 1:
// Board self-test with nothing connected to Type-C receptacle.
// -----------------------------------------------------------------------------

static bool test_unconnected_self_check(void) {
  Serial.println();
  Serial.println("=== Test 1: unconnected self-check ===");
  Serial.println("Make sure nothing is connected to the Type-C receptacle.");
  Serial.println("This test also detects if the breakout board CC pull-downs were not removed.");
  Serial.println();

  usbc_probe_led_all_off();
  all_probe_lines_open();
  delay(200);

  bool ok = true;

  Serial.println("[Initial open state]");
  print_all_voltages();
  Serial.println("Note: open CC/D+/D- nodes may float. Initial open voltage is informational only.");
  Serial.println();

  // High-side path check.
  // This checks D3/D4/D7/D9 high-side paths without enabling the low-side pins.
  // With nothing connected, CC1/CC2/D+/D- should all rise close to 5V.
  Serial.println("[High-side path check]");
  Serial.println("Enable only high-side/Rp pins. Low-side/Rd pins are Hi-Z.");
  Serial.println("Expected: CC1, CC2, D+, D- should be near 5V with nothing connected.");

  all_probe_lines_open();
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

  ok &= expect_range("CC1 high-side", usbc_probe_read(USBC_PROBE_CC1), CC_RP_OPEN_MIN_V, ADC_REF_V + 0.10f);
  ok &= expect_range("CC2 high-side", usbc_probe_read(USBC_PROBE_CC2), CC_RP_OPEN_MIN_V, ADC_REF_V + 0.10f);
  ok &= expect_range("D+ high-side",  usbc_probe_read(USBC_PROBE_DP),  CC_RP_OPEN_MIN_V, ADC_REF_V + 0.10f);
  ok &= expect_range("D- high-side",  usbc_probe_read(USBC_PROBE_DM),  CC_RP_OPEN_MIN_V, ADC_REF_V + 0.10f);
  Serial.println();

  all_probe_lines_open();
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

  all_probe_lines_open();
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

  ok &= expect_range("CC1 low/Rd-side HIGH", usbc_probe_read(USBC_PROBE_CC1), CC_RP_OPEN_MIN_V, ADC_REF_V + 0.10f);
  ok &= expect_range("CC2 low/Rd-side HIGH", usbc_probe_read(USBC_PROBE_CC2), CC_RP_OPEN_MIN_V, ADC_REF_V + 0.10f);
  ok &= expect_range("D+ low-side HIGH",     usbc_probe_read(USBC_PROBE_DP),  CC_RP_OPEN_MIN_V, ADC_REF_V + 0.10f);
  ok &= expect_range("D- low-side HIGH",     usbc_probe_read(USBC_PROBE_DM),  CC_RP_OPEN_MIN_V, ADC_REF_V + 0.10f);
  Serial.println();

  all_probe_lines_open();
  delay(100);

  // CC Rd check.
  Serial.println("[CC Rd check]");
  usbc_probe_set(USBC_PROBE_CC1, USBC_PROBE_RD);
  usbc_probe_set(USBC_PROBE_CC2, USBC_PROBE_RD);
  delay(100);

  ok &= expect_range("CC1 Rd", usbc_probe_read(USBC_PROBE_CC1), 0.0f, CC_LOW_MAX_V);
  ok &= expect_range("CC2 Rd", usbc_probe_read(USBC_PROBE_CC2), 0.0f, CC_LOW_MAX_V);
  Serial.println();

  // CC Rp check.
  // With no external connection and no built-in breakout pull-down,
  // CC should rise close to 5V through 56k.
  Serial.println("[CC Rp default check]");
  usbc_probe_set(USBC_PROBE_CC1, USBC_PROBE_RP_DEFAULT);
  usbc_probe_set(USBC_PROBE_CC2, USBC_PROBE_RP_DEFAULT);
  delay(100);

  ok &= expect_range("CC1 Rp open", usbc_probe_read(USBC_PROBE_CC1), CC_RP_OPEN_MIN_V, ADC_REF_V + 0.10f);
  ok &= expect_range("CC2 Rp open", usbc_probe_read(USBC_PROBE_CC2), CC_RP_OPEN_MIN_V, ADC_REF_V + 0.10f);
  Serial.println();

  // D+ 0.6V check.
  Serial.println("[D+ fixed 0.6V check]");
  usbc_probe_set(USBC_PROBE_DP, USBC_PROBE_0V6);
  delay(100);
  ok &= expect_range("D+ 0.6V", usbc_probe_read(USBC_PROBE_DP), DLINE_0V6_MIN_V, DLINE_0V6_MAX_V);
  Serial.println();

  // D- 0.6V check.
  Serial.println("[D- fixed 0.6V check]");
  usbc_probe_set(USBC_PROBE_DM, USBC_PROBE_0V6);
  delay(100);
  ok &= expect_range("D- 0.6V", usbc_probe_read(USBC_PROBE_DM), DLINE_0V6_MIN_V, DLINE_0V6_MAX_V);
  Serial.println();

  // D+ PWM check at 1.2V.
  Serial.println("[D+ PWM 1.2V check]");
  usbc_probe_set(USBC_PROBE_DP, 1.2f);
  delay(200);
  ok &= expect_range("D+ PWM 1.2V", usbc_probe_read(USBC_PROBE_DP), PWM_1V2_MIN_V, PWM_1V2_MAX_V);
  Serial.println();

  // D- PWM check at 1.2V.
  Serial.println("[D- PWM 1.2V check]");
  usbc_probe_set(USBC_PROBE_DM, 1.2f);
  delay(200);
  ok &= expect_range("D- PWM 1.2V", usbc_probe_read(USBC_PROBE_DM), PWM_1V2_MIN_V, PWM_1V2_MAX_V);
  Serial.println();

  // Discharge check.
  Serial.println("[D+/D- discharge check]");
  usbc_probe_set(USBC_PROBE_DP, USBC_PROBE_DISCHARGE);
  usbc_probe_set(USBC_PROBE_DM, USBC_PROBE_DISCHARGE);
  delay(100);

  ok &= expect_range("D+ discharge", usbc_probe_read(USBC_PROBE_DP), 0.0f, DLINE_LOW_MAX_V);
  ok &= expect_range("D- discharge", usbc_probe_read(USBC_PROBE_DM), 0.0f, DLINE_LOW_MAX_V);
  Serial.println();

  all_probe_lines_open();

  if (ok) {
    Serial.println("Test 1 result: PASS");
    usbc_probe_led_rgb(false, true, false); // green
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

static bool test_sink_attach_wait(void) {
  Serial.println();
  Serial.println("=== Test 2: Sink attach / VBUS wait ===");
  Serial.println("The probe will set CC1 and CC2 to Rd.");
  Serial.println("Connect a USB-C Source to the Type-C receptacle.");
  Serial.println("Press 'q' to abort.");
  Serial.println();

  usbc_probe_led_all_off();

  all_probe_lines_open();
  usbc_probe_set(USBC_PROBE_CC1, USBC_PROBE_RD);
  usbc_probe_set(USBC_PROBE_CC2, USBC_PROBE_RD);
  usbc_probe_set(USBC_PROBE_DP, USBC_PROBE_OPEN);
  usbc_probe_set(USBC_PROBE_DM, USBC_PROBE_OPEN);

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

    const float vbus = usbc_probe_read(USBC_PROBE_VBUS);
    const float cc1  = usbc_probe_read(USBC_PROBE_CC1);
    const float cc2  = usbc_probe_read(USBC_PROBE_CC2);

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
    usbc_probe_led_rgb(false, true, false); // green
    Serial.println("Test 2 result: PASS");
  } else {
    usbc_probe_led_rgb(false, false, false);
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

static bool test_bc12_dp_0v6_dm_response(void) {
  Serial.println();
  Serial.println("=== Test 3: BC1.2 D+ 0.6V / D- response check ===");
  Serial.println("Use a Type-A charger with a Type-A to Type-C cable.");
  Serial.println("The probe will set CC1/CC2 to Rd, wait for VBUS,");
  Serial.println("then drive D+ to about 0.6V and read D-.");
  Serial.println("Press 'q' to abort.");
  Serial.println();

  usbc_probe_led_all_off();

  // Start from a safe state.
  all_probe_lines_open();

  // DP/DM must be open before VBUS arrives.
  usbc_probe_set(USBC_PROBE_DP, USBC_PROBE_OPEN);
  usbc_probe_set(USBC_PROBE_DM, USBC_PROBE_OPEN);

  // Behave as a Type-C sink.
  usbc_probe_set(USBC_PROBE_CC1, USBC_PROBE_RD);
  usbc_probe_set(USBC_PROBE_CC2, USBC_PROBE_RD);

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
      all_probe_lines_open();
      blink_red(10);
      return false;
    }

    const float vbus = usbc_probe_read(USBC_PROBE_VBUS);
    const float cc1  = usbc_probe_read(USBC_PROBE_CC1);
    const float cc2  = usbc_probe_read(USBC_PROBE_CC2);

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
    all_probe_lines_open();
    blink_red(10);
    return false;
  }

  Serial.println();
  Serial.println("VBUS detected. Starting D+/D- check...");

  // Let the charger/cable/port settle after VBUS appears.
  delay(300);

  // Ensure D- is not driven by this probe.
  usbc_probe_set(USBC_PROBE_DM, USBC_PROBE_OPEN);

  // Drive D+ to approximately 0.6V using the fixed 68k/10k divider.
  usbc_probe_set(USBC_PROBE_DP, USBC_PROBE_0V6);

  // Allow D+ RC and charger response to settle.
  delay(300);

  const float vbus = usbc_probe_read(USBC_PROBE_VBUS);
  const float cc1  = usbc_probe_read(USBC_PROBE_CC1);
  const float cc2  = usbc_probe_read(USBC_PROBE_CC2);
  const float dp   = usbc_probe_read(USBC_PROBE_DP);
  const float dm   = usbc_probe_read(USBC_PROBE_DM);

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
    usbc_probe_led_rgb(false, true, false); // green
    return true;
  }

  Serial.println("Test 3 result: DONE, no D- response");
  Serial.println("VBUS was present and D+ was driven, but D- did not respond around 0.6V.");
  Serial.println("This may be SDP-like/no-BC1.2 response, or the charger/cable may not expose that behavior.");

  // Blue means completed, but no BC1.2-like response.
  usbc_probe_led_rgb(false, false, true);
  return false;
}

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

  usbc_probe_init();

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
      test_unconnected_self_check();
      break;

    case '2':
      drain_serial_input();
      test_sink_attach_wait();
      break;

    case '3':
      drain_serial_input();
      test_bc12_dp_0v6_dm_response();
      break;

    case 'o':
    case 'O':
      all_probe_lines_open();
      Serial.println("All probe lines are open.");
      break;

    case 'v':
    case 'V':
      print_all_voltages();
      break;

    case 'q':
    case 'Q':
      usbc_probe_led_all_off();
      Serial.println("LEDs off.");
      break;

    default:
      Serial.println("Unknown command.");
      break;
  }

  print_menu();
}
