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

#define BUFSIZE 8
#define NPORTS 4
#define NARGS 8

uint8_t SwPins[NPORTS] = { 10, 9, 5, 6 };
char buf[BUFSIZE];
int bufcnt;
int args[NARGS];
int argcnt;
bool parse_error;
bool debug = false;

#define CMD_HELP 1
#define CMD_ON 2
#define CMD_OFF 3
#define CMD_NPORTS 4
#define CMD_DELAY 5
#define CMD_DEBUG 6

struct {
    int command_number;
    const char* command;
    const char* help;
} commands[] = {
    { CMD_ON,       "on",       "on <port>: turn on specified port" },
    { CMD_OFF,      "off",      "off <port>: turn off specified port" },
    { CMD_NPORTS,   "n?",       "show how many ports exist" },
    { CMD_DELAY,    "delay",    "delay <n>: wait for n (ms)" },
    { CMD_DEBUG,    "debug",    "debug {0|1}: set debug flag" },
    { CMD_HELP,     "help",     "show this message" },
};

void reset_parser() {
    parse_error = false;
    bufcnt = 0;
    argcnt = 0;
}

void setup() {
    Serial.begin(115200);
    while (!Serial)
        delay(1);  // wait for serial port to open
    Serial.println("Switch Controller");
    reset_parser();
    for (int i = 0; i < NPORTS; i++)
        pinMode(SwPins[i], OUTPUT);
}

void parse_buf() {
    if (debug) {
        Serial.print("argcnt=");
        Serial.print(argcnt);
        Serial.print(", bufcnt=");
        Serial.print(bufcnt);
        Serial.print(", buf=");
        Serial.println(buf);
    }
    if (bufcnt == 0)
        return;

    if (NARGS <= argcnt) {
        parse_error = true;
        bufcnt = 0;
        return;
    }

    if (argcnt == 0) {
        for (int i = 0; i < sizeof(commands)/sizeof(*commands); i++) {
            if (strncasecmp(buf, commands[i].command, bufcnt) == 0) {
                args[argcnt++] = commands[i].command_number;
                bufcnt = 0;
                return;
            }
        }
        parse_error = true;
        bufcnt = 0;
        return;
    }

    char* endptr;
    args[argcnt++] = strtol(buf, &endptr, 0);
    if (endptr != &buf[bufcnt]) {
        parse_error = true;
    }
    bufcnt = 0;

    return;
}

void execute() {
    int value;
    if (argcnt == 0)
        return;
    switch (args[0]) {
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
        if (argcnt < 2 || NPORTS <= args[1]) {
            Serial.println("invalid argument");
            break;
        }
        if (args[0] == CMD_ON) {
            value = HIGH;
        } else {
            value = LOW;
        }
        if (debug) {
            Serial.print("set port ");
            Serial.print(args[1]);
            Serial.print(" to ");
            Serial.print(value);
            Serial.println();
        }
        digitalWrite(SwPins[args[1]], value);
        break;
    case CMD_DELAY:
        if (argcnt < 2) {
            Serial.println("invalid argument");
            break;
        }
        if (debug) {
            Serial.print("delay ");
            Serial.print(args[1]);
            Serial.println(" ms");
        }
        delay(args[1]);
        break;
    case CMD_NPORTS:
        Serial.println(NPORTS);
        break;
    case CMD_DEBUG:
        if (argcnt != 2) {
            Serial.println("invalid argument");
            break;
        }
        debug = args[1];
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

void loop() {
    char c = Serial.read();
    if (c == -1) {
        delay(10);
        return;
    }
    if (debug) {
        Serial.print("read()=");
        Serial.println(c, HEX);
    }
    if (c == '\n' || c == ';') {
        parse_buf();
        if (parse_error) {
            Serial.println("parse error");
        } else {
            execute();
        }
        reset_parser();
        if (c == '\n')
            Serial.println("ok");
        return;
    }
    if (parse_error || (argcnt != 0 && c == ' ') || c == '\r') {
        // just ignore
        return;
    }
    if (c == ',' || c == ' ') {
        parse_buf();
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
    buf[bufcnt] = '\0';

    return;
}
