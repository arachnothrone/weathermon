#include "datetime.h"

bool operator<(const Date& lhs, const Date& rhs) {
    if (lhs.GetYear() == rhs.GetYear()) {
        if (lhs.GetMonth() == rhs.GetMonth()) {
            return lhs.GetDay() < rhs.GetDay();
        }
        return lhs.GetMonth() < rhs.GetMonth();
    }
    return lhs.GetYear() < rhs.GetYear();
}

bool operator==(const Date& lhs, const Date& rhs) {
    return lhs.GetYear() == rhs.GetYear() &&
        lhs.GetMonth() == rhs.GetMonth() &&
        lhs.GetDay() == rhs.GetDay();
}

Date::Date(const Day new_day, const Month new_month, const Year new_year) {
    y = new_year.value;
    if (new_month.value > 12 || new_month.value < 1) {
        throw std::logic_error("Invalid month: " + std::to_string(new_month.value));
    }
    m = new_month.value;
    if (new_day.value > 31 || new_day.value < 1) {
        throw std::logic_error("Invalid day: " + std::to_string(new_day.value));
    }
    d = new_day.value;
}

int Date::GetYear() const {
    return y;
}
int Date::GetMonth() const {
    return m;
}
int Date::GetDay() const {
    return d;
}

void Date::Print() const 
{
    std::cout 
        << y << "/" 
        << std::setfill('0') << std::setw(2) << m << "/" 
        << std::setfill('0') << std::setw(2) << d 
        << std::endl;
}

Date ParseDate(const std::string& rawdate) {
    std::istringstream iss(rawdate);
    int iyear, imonth, iday;
    std::string year, month, day;
    if (getline(iss, year, '-')) {
        std::cout << "==> y_" << year << "_" << std::endl;
        if (getline(iss, month, '-')) {
            getline(iss, day);
        } else {
            throw std::runtime_error("Wrong date format: " + rawdate);
        }
    } else {
        throw std::runtime_error("Wrong date format: " + rawdate);
    }
    
    // iss >> year >> month >> day;
    std::istringstream syear(year);
    syear >> iyear;
    std::istringstream smonth(month);
    smonth >> imonth;
    std::istringstream sday(day);
    sday >> iday;

    std::cout << "Y=" << iyear << " M=" << imonth << " D=" << iday << std::endl;
    
    if (imonth > 12 || imonth < 1) {
        throw std::runtime_error("Month value is invalid: " + std::to_string(imonth));
    }

    if (iday > 31 || iday < 1) {
        throw std::runtime_error("Day value is invalid: " + std::to_string(iday));
    }
    
    return Date(Day(iday), Month(imonth), Year(iyear));
}
