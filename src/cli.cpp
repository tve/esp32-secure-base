#include <Arduino.h>
#include <stdlib.h>
#include "ESPSecureBase.h"

// initial value for _lineMax
constexpr int LMAX = 80;

void CommandParser::parseLine(char *line) {
    while (*line == ' ') line++; // skip leading spaces
    // if empty command, call default handler
    if (!*line) {
        if (_cbDefault) _cbDefault(this, "");
        return;
    }
    // find end of command = start of args
    _args = strchr(line, ' ');
    if (_args) *_args++ = 0; // null terminate the command
    // find and call command handler
    CBList *cbl = _cbList;
    while (cbl) {
        if (strcmp(line, cbl->name) == 0) {
            //printf("Dispatch for %s to 0x%08x\n", cbl->name, (uint32_t)cbl);
            cbl->fun(this, line);
            return;
        }
        cbl = cbl->next;
    }
    // oops, need to call default handler
    if (_cbDefault) _cbDefault(this, line);
}

char *CommandParser::getArg(void) {
    if (!_args) return NULL;
    while (*_args == ' ') _args++; // skip leading spaces
    if (!*_args) return NULL;
    char *arg = _args;
    _args = strchr(_args, ' ');
    if (_args) *_args++ = 0; // zero-terminate arg
    return arg;
}

void CommandParser::loop(void) {
    if (!_serial) return;
    if (_lineMax == 0) {
        // initial prompt
        _serial->write('>');
        _serial->write(' ');
        // alloc initial line buffer
        _line = (char *)malloc(LMAX);
        if (_line) _lineMax = LMAX;
    }
    // parse any chars that have accumulated in the serial buffer
    while (_serial->available()) {
        char ch = _serial->read();
        // handle end-of-line
        if (ch == '\n') {
            _serial->write('\r');
            _serial->write('\n');
            if (_lineCur > 0) {
                _line[_lineCur] = 0; // terminate string
                parseLine(_line); // dispatch
                _lineCur = 0;
            }
            _serial->write('>');
            _serial->write(' ');
            // shrink cmdline buffer if it got large
            if (_lineMax > LMAX*3/2) {
                char *l = (char *)realloc(_line, LMAX);
                if (l) {
                    _line = l;
                    _lineMax = LMAX;
                }
            }
        // handle carriage-return (skip)
        } else if (ch == '\r') {
        // handle backspace
        } else if (ch == '\b') {
            if (_lineCur > 0) _lineCur--;
            _serial->write('\b');
            _serial->write(' ');
            _serial->write('\b');
        // handle delete (same as backspace for now)
        } else if (ch == 0177) { // del
            if (_lineCur > 0) _lineCur--;
            _serial->write('\b');
            _serial->write(' ');
            _serial->write('\b');
        // handle arbitrary other char (store in buffer)
        } else {
            if (_lineCur >= _lineMax-1) {
                if (_lineMax > 0) {
                    char *l = (char *)realloc(_line, _lineMax*3/2); // allocate 50% more
                    if (!l) return; // oops!
                    _line = l;
                    _lineMax = _lineMax*3/2;
                } else {
                    _line = (char *)malloc(LMAX);
                    if (!_line) return; // oops!
                    _lineMax = LMAX;
                }
            }
            _serial->write(ch);
            _line[_lineCur++] = ch;
        }
    }
}
