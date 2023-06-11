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

bool operator<(const Time& lhs, const Time& rhs) {
    if (lhs.GetHour() == rhs.GetHour()) {
        if (lhs.GetMinute() == rhs.GetMinute()) {
            return lhs.GetSecond() < rhs.GetSecond();
        }
        return lhs.GetMinute() < rhs.GetMinute();
    }
    return lhs.GetHour() < rhs.GetHour();
}

bool operator==(const Time& lhs, const Time& rhs) {
    return lhs.GetHour() == rhs.GetHour() &&
        lhs.GetMinute() == rhs.GetMinute() &&
        lhs.GetSecond() == rhs.GetSecond();
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

Date ParseDate(const std::string& rawdate, std::string& tail) {
    std::istringstream iss(rawdate);
    int iyear, imonth, iday;
    std::string year, month, day;
    if (getline(iss, year, '/')) {
        std::cout << "==> y_" << year << "_" << std::endl;
        if (getline(iss, month, '/')) {
            if (getline(iss, day, ' ')) {
                getline(iss, tail);
            } else if (getline(iss, day)) {
                tail = "";
            } else {
                throw std::runtime_error("Wrong date format: " + rawdate);
            }
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

Time::Time(const Hour new_hour, const Minute new_minute, const Second new_second) {
    h = new_hour.value;
    if (new_minute.value > 59 || new_minute.value < 0) {
        throw std::logic_error("Invalid minutes: " + std::to_string(new_minute.value));
    }
    m = new_minute.value;
    if (new_second.value > 59 || new_second.value < 0) {
        throw std::logic_error("Invalid seconds: " + std::to_string(new_second.value));
    }
    s = new_second.value;
}

int Time::GetHour() const {
    return h;
}

int Time::GetMinute() const {
    return m;
}

int Time::GetSecond() const {
    return s;
}

void Time::Print() const {
    std::cout 
        << std::setfill('0') << std::setw(2) << h << ":" 
        << std::setfill('0') << std::setw(2) << m << ":" 
        << std::setfill('0') << std::setw(2) << s 
        << std::endl;
}

Time ParseTime(const std::string& rawtime) {
    std::istringstream iss(rawtime);
    int ihour, iminute, isecond;
    std::string hour, minute, second;
    if (getline(iss, hour, ':')) {
        if (getline(iss, minute, ':')) {
            getline(iss, second);
        } else {
            throw std::runtime_error("Wrong time format: " + rawtime);
        }
    } else {
        throw std::runtime_error("Wrong time format: " + rawtime);
    }
    
    // iss >> hour >> minute >> second;
    std::istringstream shour(hour);
    shour >> ihour;
    std::istringstream sminute(minute);
    sminute >> iminute;
    std::istringstream ssecond(second);
    ssecond >> isecond;

    if (ihour > 23 || ihour < 0) {
        throw std::runtime_error("Hour value is invalid: " + std::to_string(ihour));
    }

    if (iminute > 59 || iminute < 0) {
        throw std::runtime_error("Minute value is invalid: " + std::to_string(iminute));
    }

    if (isecond > 59 || isecond < 0) {
        throw std::runtime_error("Second value is invalid: " + std::to_string(isecond));
    }
    
    return Time(Hour(ihour), Minute(iminute), Second(isecond));
}
