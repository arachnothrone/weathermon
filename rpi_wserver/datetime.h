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

class Date {
public:
    Date(const Day new_day, const Month new_month, const Year new_year);
    // {
    //     y = new_year.value;
    //     if (new_month.value > 12 || new_month.value < 1) {
    //         throw std::logic_error("Invalid month: " + std::to_string(new_month.value));
    //     }
    //     m = new_month.value;
    //     if (new_day.value > 31 || new_day.value < 1) {
    //         throw std::logic_error("Invalid day: " + std::to_string(new_day.value));
    //     }
    //     d = new_day.value;
    // }

    int GetYear() const;
    // {
    //     return y;
    // }
    int GetMonth() const; 
    // {
    //     return m;
    // }
    int GetDay() const;
    // {
    //     return d;
    // }

    void Print() const;
    // {
    //     std::cout << y << "/" << m << "/" << d << std::endl;
    // }
private:
    int d;
    int m;
    int y;
};

bool operator<(const Date& lhs, const Date& rhs);

bool operator==(const Date& lhs, const Date& rhs);

Date ParseDate(const std::string& rawdate);
