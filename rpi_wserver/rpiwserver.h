#pragma once

#include <string>


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
