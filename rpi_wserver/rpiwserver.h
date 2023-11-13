#pragma once

#include <string>
#include <stdint.h>
#include <map>

/* Search direction */
#define FORWARD 1
#define BACKWARD 0

typedef struct {
    uint16_t totalNrOfRestarts;
    uint16_t successCounter;
    uint16_t rxBufferOverrunCntr;
} STATS_T;

typedef struct {
    double      temperature;
    double      humidity;
    double      pressure;
    uint16_t    ambientLight;
    uint16_t    vcc;
    STATS_T     stats;
} RECORD_T;

typedef enum {
    INVALID_COMMAND = -1,
    GETCURRENTDATA = 1,
    GETDATARANGE,
} CMD_E;

static const std::map<CMD_E, std::string> mCMDMap = {
    {GETCURRENTDATA, "GETCURRENTDATA"},
    {GETDATARANGE, "GETDATARANGE"},
};

class ArduinoData {
public:
    ArduinoData(const char* const data);
    ArduinoData();
    std::string GetString() const;
    void ReadBuffer(const char* const data);
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

class DataMessage {
public:
    DataMessage(const std::string wsLogFileName, const std::tuple<std::streampos, std::streampos> dataBlockRange);
    void PrintMessage() const;
    ~DataMessage();
private:

    int _numRecords;
    RECORD_T* _records;
    std::ifstream _wsLogFile;
};
