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
