/*

The MIT License (MIT)

Copyright (c) 2021 @hanyazou

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include <string.h>
#include <TimerOne.h>

#define BUFSIZE 128
#define NARGS 16
#define arraysizeof(a) (sizeof(a)/sizeof(*(a)))

enum {
    CMD_HELP = 1,
    CMD_ON,
    CMD_OFF,
    CMD_NPORTS,
    CMD_READ,
    CMD_WATCH,
    CMD_RESET,
    CMD_DELAY,
    CMD_DEBUG,
};

const int PORT_DIGITAL = (1 << 0);
const int PORT_ANALOG = (1 << 1);
const int PORT_INPUT = (1 << 2);
const int PORT_OUTPUT = (1 << 3);
const int PORT_WATCH_RAISING = (1 << 4);
const int PORT_WATCH_FALLING = (1 << 5);
const int PORT_WATCH_VERBOSE = (1 << 6);
const int PORT_DEBOUNCE = (1 << 7);

const int PORT_WATCH = (PORT_WATCH_RAISING | PORT_WATCH_FALLING);
const int PORT_WATCH_MASK = (PORT_WATCH | PORT_WATCH_VERBOSE);

struct {
    int command_number;
    const char* command;
    const char* help;
} commands[] = {
    { CMD_ON,       "on",       "on <port>: turn on specified port" },
    { CMD_OFF,      "off",      "off <port>: turn off specified port" },
    { CMD_NPORTS,   "n?",       "show how many ports exist" },
    { CMD_READ,     "r",        "read specified port" },
    { CMD_WATCH,    "watch",    "watch specified port" },
    { CMD_DELAY,    "delay",    "delay <n>: wait for n (ms)" },
    { CMD_DEBUG,    "debug",    "debug {0|1}: set debug flag" },
    { CMD_RESET,    "reset",    "reset all settings" },
    { CMD_HELP,     "help",     "show this message" },
};

struct port {
    char* name;
    int pin;
    int attrs;
    int threshold;
    int value;
    int raw_value;
    int event;
    int count;
};

struct port port_defaults[] = {
    { "0",  10, PORT_DIGITAL | PORT_OUTPUT, },
    { "1",   9, PORT_DIGITAL | PORT_OUTPUT, },
    { "2",   5, PORT_DIGITAL | PORT_OUTPUT, },
    { "3",   6, PORT_DIGITAL | PORT_OUTPUT, },
    { "A0", A0, PORT_ANALOG | PORT_INPUT | PORT_DEBOUNCE, 100, },
    { "A1", A1, PORT_ANALOG | PORT_INPUT | PORT_DEBOUNCE, 100, },
    { "A2", A2, PORT_ANALOG | PORT_INPUT | PORT_DEBOUNCE, 100, },
    { "A3", A3, PORT_ANALOG | PORT_INPUT | PORT_DEBOUNCE, 100, },
};

struct port ports[arraysizeof(port_defaults)];

bool parse_error;
bool debug;

void reset(void) {
    memset(ports, 0, sizeof(ports));
    debug = false;
    for (int i = 0; i < arraysizeof(ports); i++) {
        struct port* p = &ports[i];
        *p = port_defaults[i];
        p->value = -1;
        if (p->attrs & PORT_DIGITAL) {
            if (p->attrs & PORT_OUTPUT) {
                pinMode(p->pin, OUTPUT);
                p->raw_value = p->value = 0;
                digitalWrite(p->pin, p->raw_value);
            } else {
                pinMode(p->pin, INPUT);
                p->raw_value = digitalRead(p->pin);
                p->value = p->raw_value ? 1 : 0;
            }
        }
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial)
        delay(1);  // wait for serial port to open
    Serial.println("Switch Controller");
    reset();
    Timer1.initialize(10000);
    Timer1.attachInterrupt(watch);  // watch to run every 10 milli seconds
}

void read_port(int port) {
    struct port* p = &ports[port];
    int value;
    if (p->attrs & PORT_ANALOG) {
        p->raw_value = analogRead(p->pin);
        value = p->threshold <  p->raw_value ? 1 : 0;
    } else {
        p->raw_value = digitalRead(p->pin);
        value = p->raw_value ? 1 : 0;
    }
    if (p->value == -1 || p->value == value) {
        p->value = value;
        p->count = 0;
        return;
    }
    if ((p->attrs & PORT_DEBOUNCE) && p->count++ < 2)
        return;
    if ((p->value == 0 && (p->attrs | PORT_WATCH_RAISING)) ||
        (p->value == 1 && (p->attrs | PORT_WATCH_FALLING))) {
        p->event++;
    }
    p->value = value;
    p->count = 0;
}

void show_port(int port) {
    if (port == -1) {
        for (port = 0; port < arraysizeof(ports); port++) {
            show_port(port);
        }
        return;
    }

    struct port* p = &ports[port];
    Serial.print("port");
    Serial.print(port);
    Serial.print(": pin=");
    Serial.print(p->pin);
    Serial.print(", name=");
    Serial.print(p->name);
    Serial.print(", value=");
    Serial.print(p->value);
    Serial.print(", raw=");
    Serial.print(p->raw_value);
    if (p->attrs & PORT_WATCH) {
        Serial.print(", threshold=");
        Serial.print(p->threshold);
        Serial.print(", event=");
        Serial.print(p->event);
    }
    Serial.println();
}

void watch(void) {
    for (int i = 0; i < arraysizeof(ports); i++) {
        struct port* p = &ports[i];
        if (!(p->attrs & PORT_WATCH))
            continue;
        int event = p->event;
        read_port(i);
        if ((p->attrs & PORT_WATCH_VERBOSE) && event != p->event)
            show_port(i);
    }
}

bool get_command(char* buf, int* value) {
    for (int i = 0; i < sizeof(commands)/sizeof(*commands); i++) {
        if (strcasecmp(buf, commands[i].command) == 0) {
            *value = commands[i].command_number;
            return true;
        }
    }
    return false;
}

bool get_port_number(char* buf, int* value) {
    for (int i = 0; i < arraysizeof(ports); i++) {
        if (strcasecmp(ports[i].name, buf) == 0) {
            *value = i;
            return true;
        }
    }
    return false;
}

bool get_integer(char* buf, int* value) {
    char* endptr;
    *value = strtol(buf, &endptr, 0);
    return (endptr == &buf[strlen(buf)]);
}

void execute(char* args[], int argcnt) {
    int command, value, port, attrs;
    if (argcnt == 0)
        return;
    if (!get_command(args[0], &command)) {
        Serial.print("unknown command, ");
        Serial.println(args[0]);
        return;
    }
    switch (command) {
    case CMD_HELP:
        for (int i = 0; i < sizeof(commands)/sizeof(*commands); i++) {
            if (commands[i].help) {
                Serial.print(commands[i].command);
                Serial.print("\t");
                Serial.println(commands[i].help);
            }
        }
        break;
    case CMD_ON:
    case CMD_OFF:
        if (argcnt < 2 || !get_port_number(args[1], &port)) {
            Serial.println("invalid argument");
            break;
        }
        if (command == CMD_ON) {
            value = HIGH;
        } else {
            value = LOW;
        }
        if (debug) {
            Serial.print("set port '");
            Serial.print(ports[port].name);
            Serial.print("' to ");
            Serial.print(value);
            Serial.println();
        }
        digitalWrite(ports[port].pin, value);
        if (!(ports[port].attrs & PORT_WATCH))
            read_port(port);
        break;
    case CMD_DELAY:
        if (argcnt < 2 || !get_integer(args[1], &value)) {
            Serial.println("invalid argument");
            break;
        }
        if (debug) {
            Serial.print("delay ");
            Serial.print(value);
            Serial.println(" ms");
        }
        delay(value);
        break;
    case CMD_NPORTS:
        Serial.println(arraysizeof(ports));
        break;
    case CMD_READ:
        if (argcnt < 2) {
            for (port = 0; port < arraysizeof(ports); port++) {
                read_port(port);
                show_port(port);
            }
            break;
        }
        if (argcnt != 2 || !get_port_number(args[1], &port)) {
            Serial.println("invalid argument");
            break;
        }
        read_port(port);
        show_port(port);
        break;
    case CMD_WATCH:
        if (argcnt < 2) {
            show_port(-1);
            break;
        }
        if (!get_port_number(args[1], &port)) {
            Serial.println("invalid port");
            break;
        }
        attrs = 0;
        value = ports[port].threshold;
        for (int i = 2; i < argcnt; i++) {
            if (strcasecmp(args[i], "raising") == 0) {
                attrs |= PORT_WATCH_RAISING;
            } else
            if (strcasecmp(args[i], "falling") == 0) {
                attrs |= PORT_WATCH_FALLING;
            } else
            if (strcasecmp(args[i], "both") == 0) {
                attrs |= PORT_WATCH;
            } else
            if (!get_integer(args[i], &value)) {
                Serial.println("invalid argument");
                break;
            }
        }
        if (attrs == 0) {
            attrs = (PORT_WATCH | PORT_WATCH_VERBOSE);
        }
        ports[port].attrs &= ~(PORT_WATCH_MASK);
        ports[port].attrs |= attrs;
        ports[port].threshold = value;
        break;
    case CMD_RESET:
            reset();
        break;
    case CMD_DEBUG:
        if (argcnt != 2 || !get_integer(args[1], &value)) {
            Serial.println("invalid argument");
            break;
        }
        debug = value;
        if (debug) {
            Serial.print("set debug flag to ");
            Serial.println(args[1]);
        }
        break;
    default:
        Serial.println("internal error");
        break;
    }
}

void parse(char* buf) {
    if (debug) {
        Serial.print("parse(): buf='");
        Serial.print(buf);
        Serial.println("'");
    }

    int len = strlen(buf);
    char* args[NARGS];
    int count = 0;
    int i = 0;
    while (i <= len) {
        // Ignore leading whitespace
        while (buf[i] == ' ' || buf[i] == '\t')
            i++;
        args[count] = &buf[i];
        if (debug) {
            Serial.print("args[");
            Serial.print(count);
            Serial.print("] = '");
            Serial.print(args[count]);
            Serial.println("'");
        }
        while (buf[i] != ';' && buf[i] != '\0' && buf[i] != ' ' && buf[i] != '\t') {
            i++;
        }
        if (args[count] != &buf[i]) {
            if (NARGS <= count) {
                Serial.println("parse error");
                return;
            }
            count++;
        }
        // Ignore trailing whitespace
        while (buf[i] == ' ' || buf[i] == '\t')
            buf[i++] = '\0';
        if (debug) {
            Serial.print("buf[");
            Serial.print(i);
            Serial.print("] = ");
            Serial.print(buf[i], HEX);
            Serial.println();
        }
        if (buf[i] == ';' || buf[i] == '\0') {
            buf[i++] = '\0';
            execute(args, count);
            count = 0;
        }
    }
}

void loop() {
    static char buf[BUFSIZE];
    static int bufcnt = 0;

    char c = Serial.read();
    if (c == -1) {
        delay(10);
        return;
    }
    if (debug) {
        Serial.print("read()=");
        Serial.println(c, HEX);
    }
    if (c == '\n' || c == '\r') {
        if (parse_error) {
            Serial.println("parse error");
        } else {
            buf[bufcnt] = '\0';
            parse(buf);
        }
        bufcnt = 0;
        parse_error = false;
        Serial.println("ok");
        return;
    }
    if (BUFSIZE - 1 <= bufcnt) {
        parse_error = true;
        return;
    }
    if (debug) {
        Serial.print("input=");
        Serial.print(c);
        Serial.print(", bufcnt=");
        Serial.println(bufcnt);
    }
    buf[bufcnt++] = c;

    return;
}
