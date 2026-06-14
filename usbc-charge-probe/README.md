# USB-C Charge Probe

USB-C Charge Probe is a small Arduino UNO R4 based probe for observing low-speed USB-C charging behavior.

It is intended for experiments around:

* USB-C CC1/CC2 voltage behavior
* Type-C Rp/Rd attach behavior
* VBUS voltage observation
* USB BC 1.2 style D+/D- voltage probing
* Apple/Samsung/QC-like charger signature observation
* Simple 0.6 V stimulus on D+ or D-

This is not a USB protocol analyzer and not a USB PD PHY.

## What this probe can do

* Read VBUS through a resistor divider
* Read CC1, CC2, D+, and D- voltages
* Drive CC1/CC2 as Rd
* Drive CC1/CC2 as default Rp
* Drive D+ or D- with a fixed approximately 0.6 V level
* Drive D+ or D- with a PWM-smoothed voltage
* Discharge D+/D- smoothing capacitors
* Run basic self-tests from the serial menu
* Indicate test result with LEDs

## What this probe cannot do

* It does not pass through USB data signals.
* It does not support USB HS/FS/LS communication.
* It does not decode or transmit USB PD messages.
* It is not a USB-IF compliance tester.
* It is not intended to be inserted inline between a host and a device.

The D+/D- lines intentionally have capacitors for PWM smoothing, so USB data communication is not expected to work through this board.

## Hardware

Target board:

* Arduino UNO R4
* Arduino UNO shield style universal prototyping board
* USB Type-C receptacle breakout board
* Chip resistors
* Two capacitors for D+/D- PWM smoothing
* Three LEDs

## Wiring

Final wiring used by the firmware:

```text
A0 = VBUS sense, through 100k/22k divider
A1 = CC2 sense, jumper from CC2 node
A2 = CC1
A3 = D-
A4 = D+
A5 = CC2 node, not used as ADC

D2 = CC2 Rd, 5.1k
D3 = CC2 Rp default, 56k

D4 = D+ high side, 68k
D5 = D+ low/PWM side, 10k

D6 = D- low/PWM side, 10k
D7 = D- high side, 68k

D8 = CC1 Rd, 5.1k
D9 = CC1 Rp default, 56k

D10 = Blue LED, active low
D11 = Red LED, active low
D12 = Green LED, active low
```

### VBUS divider

```text
VBUS ---- 100k ----+---- A0
                   |
                  22k
                   |
                  GND
```

Scale factor:

```text
A0 = VBUS * 22 / (100 + 22)
VBUS = A0 * (100 + 22) / 22
```

Approximate readings:

```text
5 V  -> 0.90 V at A0
9 V  -> 1.62 V at A0
12 V -> 2.16 V at A0
15 V -> 2.70 V at A0
20 V -> 3.61 V at A0
```

This divider is intended for observing normal USB-C/PD VBUS levels up to about 20 V.

### CC wiring

```text
CC2:
  D2 ---- 5.1k ----+---- CC2 ---- A5
  D3 ---- 56k -----+
                   +---- A1  // ADC sense jumper

CC1:
  D8 ---- 5.1k ----+---- CC1 ---- A2
  D9 ---- 56k -----+
```

The 5.1k resistors are used to emulate Rd.

The 56k resistors are used to emulate default Rp.

Note: A5 is physically connected to the CC2 node, but the UNO R4 A5/SCL ADC reading showed an offset-like behavior at low voltage. CC2 is read via A1 using a jumper from the CC2 node.

### D+/D- wiring

```text
D+:
  D4 ---- 68k ----+
                  +---- D+ ---- A4
  D5 ---- 10k ----+
                  |
                 Cdp
                  |
                 GND

D-:
  D6 ---- 10k ----+
                  +---- D- ---- A3
  D7 ---- 68k ----+
                  |
                 Cdm
                  |
                 GND
```

D+ fixed 0.6 V mode:

```text
D4 = HIGH
D5 = LOW

D+ = 5 V * 10k / (68k + 10k)
   ≈ 0.64 V
```

D- fixed 0.6 V mode:

```text
D7 = HIGH
D6 = LOW

D- = 5 V * 10k / (68k + 10k)
   ≈ 0.64 V
```

D5 and D6 are PWM-capable pins and can be used to generate a PWM-smoothed voltage on D+ and D-.

## Important hardware notes

Some USB Type-C receptacle breakout boards have built-in 5.1k pull-down resistors on CC1 and CC2.

For this probe, those built-in CC pull-down resistors should be removed. Otherwise, CC1/CC2 cannot be switched cleanly between open, Rd, and Rp states.

The Type-C receptacle GND must be connected to Arduino GND.

## Firmware API

The firmware provides a small API:

```cpp
void usbc_probe_init(void);

bool usbc_probe_set(usbc_probe_signal_t signal, usbc_probe_state_t state);
bool usbc_probe_set(usbc_probe_signal_t signal, float voltage);

float usbc_probe_read(usbc_probe_signal_t signal);
```

Signals:

```cpp
USBC_PROBE_CC1
USBC_PROBE_CC2
USBC_PROBE_DP
USBC_PROBE_DM
USBC_PROBE_VBUS
```

States:

```cpp
USBC_PROBE_OPEN
USBC_PROBE_RD
USBC_PROBE_RP_DEFAULT
USBC_PROBE_0V6
USBC_PROBE_DISCHARGE
```

Examples:

```cpp
usbc_probe_set(USBC_PROBE_CC1, USBC_PROBE_RD);
usbc_probe_set(USBC_PROBE_CC2, USBC_PROBE_RD);

usbc_probe_set(USBC_PROBE_DP, USBC_PROBE_0V6);
usbc_probe_set(USBC_PROBE_DM, USBC_PROBE_OPEN);

float vbus = usbc_probe_read(USBC_PROBE_VBUS);
float cc1  = usbc_probe_read(USBC_PROBE_CC1);
float dp   = usbc_probe_read(USBC_PROBE_DP);
```

Passing a float voltage to `usbc_probe_set()` enables PWM voltage mode for D+ or D-:

```cpp
usbc_probe_set(USBC_PROBE_DP, 0.6f);
usbc_probe_set(USBC_PROBE_DM, 2.0f);
```

This uses PWM and the smoothing capacitor. It is not a precision voltage source.

## Serial menu

After startup, the firmware displays a serial menu.

```text
1: Unconnected self-check
2: Sink attach / VBUS wait
o: Open all probe lines
v: Print voltages once
q: Turn LEDs off
```

### Test 1: Unconnected self-check

This test should be run with nothing connected to the Type-C receptacle.

It checks:

* High-side paths
* Low-side/Rd-side paths
* CC Rd behavior
* CC Rp default behavior
* D+ fixed 0.6 V
* D- fixed 0.6 V
* D+ PWM output
* D- PWM output
* D+/D- discharge

If the test passes, the green LED turns on.

If the test fails, the red LED blinks.

### Test 2: Sink attach / VBUS wait

This test sets both CC1 and CC2 to Rd and waits for a USB-C Source to be connected.

It prints:

* VBUS
* CC1 voltage
* CC2 voltage

When VBUS is present and one of the CC pins shows an attach-like voltage, the test passes and the green LED turns on.

Press `q` to abort.

## LED behavior

The LEDs are active low.

```text
D10 = Blue
D11 = Red
D12 = Green
```

Green means pass or attached.

Red blinking means failure or error.

## Notes

This board is a low-speed analog experiment tool. It is useful for seeing what is happening on CC1/CC2/D+/D-/VBUS as voltages, especially when debugging BC 1.2 or Type-C attach behavior.

For USB PD message transmission or reception, use a proper USB PD PHY or TCPC such as FUSB302.

For PD packet capture and general USB-C power analysis, use a dedicated analyzer such as a Power-Z device.
