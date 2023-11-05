#pragma once

#include <string>
#include <stdint.h>

/* Search direction */
#define FORWARD 1
#define BACKWARD 0

typedef struct {
    uint16_t totalNrOfRestarts;
    uint16_t successCounter;
    uint16_t rxBufferOverrunCntr;
} STATS_T;

class ArduinoData {
public:
    ArduinoData(const char* const data);
    std::string GetString() const;

    ~ArduinoData();

private:
    // int _id;
    // int _value;
    // int _timestamp;
    int8_t      _temperature;
    uint8_t     _humidity;
    uint16_t    _pressure;
    uint16_t    _ambientLight;
    uint16_t    _vcc;
    STATS_T     _stats;
    std::string _dataString;
};
