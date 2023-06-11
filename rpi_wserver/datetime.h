#pragma once

#include <iostream>
#include <exception>
#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <set>
#include <exception>

struct Day {
    int value;
    explicit Day(int new_value) {
        value = new_value;
    }
};
struct Month {
    int value;
    explicit Month(int new_value) {
        value = new_value;
    }
};
struct Year {
    int value;
    explicit Year(int new_value) {
        value = new_value;
    }
};

struct Hour {
    int value;
    explicit Hour(int new_value) {
        value = new_value;
    }
};

struct Minute {
    int value;
    explicit Minute(int new_value) {
        value = new_value;
    }
};

struct Second {
    int value;
    explicit Second(int new_value) {
        value = new_value;
    }
};


class Date {
public:
    Date(const Day new_day, const Month new_month, const Year new_year);

    int GetYear() const;
    int GetMonth() const; 
    int GetDay() const;

    void Print() const;

private:
    int d;
    int m;
    int y;
};

class Time {
public:
    Time(const Hour new_hour, const Minute new_minute, const Second new_second);

    int GetHour() const;
    int GetMinute() const; 
    int GetSecond() const;

    void Print() const;

private:
    int h;
    int m;
    int s;
};

bool operator<(const Date& lhs, const Date& rhs);
bool operator==(const Date& lhs, const Date& rhs);

bool operator<(const Time& lhs, const Time& rhs);
bool operator==(const Time& lhs, const Time& rhs);

Date ParseDate(const std::string& rawdate, std::string& tail);
Time ParseTime(const std::string& rawtime);
