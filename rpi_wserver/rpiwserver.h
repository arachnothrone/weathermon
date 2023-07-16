#pragma once

#include <string>

/* Search direction */
#define FORWARD 1
#define BACKWARD 2

class ArduinoData {
public:
    ArduinoData(const char* const data);
    std::string GetString() const;

    ~ArduinoData();

private:
    // int _id;
    // int _value;
    // int _timestamp;
    std::string _dataString;
};
