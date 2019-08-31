// Command Parser - Copyright (c)2019 by Thorsten von Eicken - MIT License

#include <string.h>

class CommandParser {
public:

    CommandParser()
        : _cbList(NULL)
        , _cbDefault(NULL)
        , _line(NULL)
        , _lineMax(0)
        , _lineCur(0)
        , _args(NULL)
        , _serial(NULL)
    {};

    // CommandParser using a serial port as input
    CommandParser(HardwareSerial *serial)
        : CommandParser()
    {
        _serial = serial;
    };

    typedef std::function<void(CommandParser *, const char *command)> callback;

    // addCommand registers a callback for a command
    void addCommand(const char *command, callback fun) {
        CBList *cbl = new CBList(command, fun, _cbList);
        _cbList = cbl;
    };

    // setDefault registers a callback if no registered command matches
    void setDefault(callback fun) { _cbDefault = fun; };

    // parseLine parses an entire line (no terminating \n or \r necessary) and dispatches to the
    // appropriate command function. parseLine is useful if commands are received via network
    // packets, for example, via MQTT. It goes with the CommandParser()` constructor.
    void parseLine(char *line);

    // loop polls the serial port passed into the `CommandParser(HardwareSerial &serial)` and performs input
    // processing (handling backspaces and such) until the end of line is received. At that point it
    // calls parseLine internally to perform the dispatch.
    void loop(void);

    // getArg can be called by a command callback function to get a pointer to the next argument
    // token.
    char *getArg(void);

    // getRest can be called by a command callback function to get a pointer to the rest of the
    // command line.
    char *getRest(void) { return _args; }


//private: // stuff intended to be private but not enforced...

    struct CBList {
        const char *name;
        callback fun;
        CBList *next;
        CBList(const char *n, callback f, CBList *nxt) : name(n), fun(f), next(nxt) {}
    };
    CBList *_cbList;
    callback _cbDefault;

    char *_line;
    int16_t _lineMax;
    int16_t _lineCur;
    char *_args;

    HardwareSerial *_serial;
};

