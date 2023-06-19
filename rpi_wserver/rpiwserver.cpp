/******************************
 * rpi weather monitor server *
 * (c) arachnothrone 2023     *
 ******************************/

#include <iostream>
#include <fstream>
//#include <string>
#include <vector>
#include <exception>

#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <cstring>
#include <algorithm>
#include <regex>

#include <signal.h>
#include <stdatomic.h>

#include "rpiwserver.h"
#include "comm.h"
#include "datetime.h"

/* Define constants */
#define SERIAL_PORT_DEFAULT_PATTERN "/dev/cu.usbmodem*"             // macos
#define BAUD_RATE                   9600
#define SERIAL_PORT_INIT_TIMEOUT_S  6
#define SELECT_TIMEOUT_S            3
#define CMD_GET_DATA1               1                               // Arguments: "YYYY-MM-DD HH:MM:SS"


#if defined(OS_TYPE)
#if (OS_TYPE) == 2
#define SERIAL_PORT_PATTERN "/dev/serial/by-path/*usb*"             // raspbian
#else
#define SERIAL_PORT_PATTERN SERIAL_PORT_DEFAULT_PATTERN
#endif
#else
#define SERIAL_PORT_PATTERN SERIAL_PORT_DEFAULT_PATTERN
#define OS_TYPE 99
#endif

#define RECORD_LENGTH (138 + 1)                                     // 138 chars + \n
#define TIMESTAMP_LENGTH 19                                         // YYYY/MM/DD HH:MM:SS

std::vector<std::string> executeCommand(const char* cmd);
std::string getLineFromLogFile(std::string timestamp);
std::string getLineFromLogFile2(const Date* refDate, const Time* refTime);
std::string string_to_hex(const std::string& input);
bool containsDate(const std::string& line);
std::string getDate(const std::string& line);

static volatile atomic_int keepRunning = 1;

void readDataFromSerialPort() {
}

void signalHandler(int signo) {
    if (signo == SIGINT) {
        std::cout << "received SIGINT";
    } else {
        std::cout << "received signal: " << signo;
    }
    std::cout << ", stopping server..." << std::endl;
    keepRunning = 0;
}

// Time: 2023/04/29 19:58:47, Temperature: 25.6 C, Humidity: 32.5 %, Pressure: 735.06 mmHg, Ambient:  825, Vcc: 4744 mV, STATS:   0    1    0
// line len = 138
// Time: 2023/04/29 19:58:47 -> (Time: \d{4}/\d{2}/\d{2}\s\d{2}:\d{2}:\d{2})

class WeatherData {
  public:
    WeatherData();
    ~WeatherData();
    void parseData(char *data);
    void printData();
    void saveData();
  private:
    char *timestamp;
    float temperature;
    float humidity;
    float pressure;
    int ambient;
    int vcc;
};

std::string getSerialPortName() {
    std::string result = "";

    std::vector<std::string> cmdOutput = executeCommand(std::string("ls ").append(SERIAL_PORT_PATTERN).c_str());
    // for raspi: /dev/serial/by-path/platform-20980000.usb-usb-0\:1\:1.0 --> /dev/serial/by-path/*usb*


    std::cout << "List of usb serial ports found:" << std::endl;
    for (auto& line : cmdOutput) {
        std::cout << line << std::endl;
    }

    /* Use the first found port from the list */
    if (cmdOutput.size() > 0) {
        auto newStrEnd = std::remove(cmdOutput[0].begin(), cmdOutput[0].end(), '\n');
        result = std::string(cmdOutput[0].begin(), newStrEnd);
    }
    
    return result;
}

std::vector<std::string> executeCommand(const char* cmd) {
    std::vector<std::string> output;
    char buffer[256];

    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        std::cerr << "popen() failed!" << std::endl;
        return output;
    }

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output.emplace_back(buffer);
    }

    pclose(pipe);
    return output;
}

std::string parseDataToJsonRegex(char *data) {
    std::string dataStr(data);
    std::string jsonStr = "{";

    std::regex timePattern("Time: ([0-9]{4}/[0-9]{2}/[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}), ");
    std::regex temperaturePattern("Temperature: ([0-9]+.[0-9]+) C, ");
    std::regex humidityPattern("Humidity: ([0-9]+.[0-9]+) %, ");
    std::regex pressurePattern("Pressure: ([0-9]+.[0-9]+) mmHg, ");
    std::regex ambientPattern("Ambient:\\s{1,}([0-9]+), ");
    std::regex vccPattern("Vcc: ([0-9]+) mV");
    std::regex statsPattern("STATS: (\\s{1,}[0-9]{1,3}){3}");

    std::smatch matches;

    if (std::regex_search(dataStr, matches, timePattern)) {
        jsonStr.append("\"Time\": \"").append(matches[1]).append("\", ");
    }
    if (std::regex_search(dataStr, matches, temperaturePattern)) {
        jsonStr.append("\"Temperature\": \"").append(matches[1]).append("\", ");
    }
    if (std::regex_search(dataStr, matches, humidityPattern)) {
        jsonStr.append("\"Humidity\": \"").append(matches[1]).append("\", ");
    }
    if (std::regex_search(dataStr, matches, pressurePattern)) {
        jsonStr.append("\"Pressure\": \"").append(matches[1]).append("\", ");
    }
    if (std::regex_search(dataStr, matches, ambientPattern)) {
        jsonStr.append("\"Ambient\": \"").append(matches[1]).append("\", ");
    }
    if (std::regex_search(dataStr, matches, vccPattern)) {
        jsonStr.append("\"Vcc\": \"").append(matches[1]).append("\", ");
    }
    if (std::regex_search(dataStr, matches, statsPattern)) {
        jsonStr.append("\"STATS\": \"").append(matches[1]).append("\"");
    }

    jsonStr.append("}");

    return jsonStr;
}

int initializeSerialPort(const std::string* const pSerialPort, int* const pSerialFd) {
    /* Open port */
    *pSerialFd = open((*pSerialPort).c_str(), O_RDWR | O_NOCTTY);

    if (*pSerialFd > 0) {
        struct termios tty;
        memset(&tty, 0, sizeof tty);
        cfsetospeed(&tty, B9600);               // Output speed 9600 baud
        cfsetispeed(&tty, B9600);               // Input speed 9600 baud
        tty.c_cflag |= (CLOCAL | CREAD);        // Ignore modem controls
        tty.c_cflag &= ~CSIZE;                  // Mask the character size bits
        tty.c_cflag |= CS8;                     // 8-bit chars
        tty.c_cflag &= ~PARENB;                 // NNo parity
        tty.c_cflag &= ~CSTOPB;                 // 1 stop bit
        tty.c_cflag &= ~CRTSCTS;                // No flow control
        tty.c_cc[VMIN] = 138;                   // Minimum number of characters to read
        tty.c_cc[VTIME] = 5;                    // Time to wait for data (*0.1 seconds)
        tcsetattr(*pSerialFd, TCSANOW, &tty);   // Set port attributes (TCSANOW = make changes immediately)
        std::cout << "Serial port initialized, port name: " << *pSerialPort << ", port fd: " << *pSerialFd << std::endl;
    } else {
        char errno_buffer[256];
        strerror_r(errno, errno_buffer, 256);
        std::cout << "Error opening serial port, errno: " << errno << ", " << errno_buffer << std::endl;
        return 1;
    }
    return 0;
}

ArduinoData::ArduinoData(const char* const data) {
    std::string dataStr(data);
    std::smatch matches;
    std::regex timePattern("Time: (\\d{4}/\\d{2}/\\d{2}\\s\\d{2}:\\d{2}:\\d{2})");  // (Time: \d{4}/\d{2}/\d{2}\s\d{2}:\d{2}:\d{2})
    
    if (std::regex_search(dataStr, matches, timePattern)) {
        if (dataStr.length() == RECORD_LENGTH) {
            /* Timestamp found and data length is correct */
            _dataString = dataStr;
        } else {
            /* Timestamp found but data length is incorrect */
            _dataString = std::string("TiMe: ").append(matches[1]) + std::string(RECORD_LENGTH - 19 - 6 - 1, '.') + std::string("\n");
        }
    } else {
        /* No timestamp, ignore */
        _dataString = "";
    }
}

ArduinoData::~ArduinoData() {}

std::string ArduinoData::GetString() const {
    return _dataString;
}

std::string string_to_hex(const std::string& input) {
    static const char* const lut = "0123456789ABCDEF";
    size_t len = input.length();

    std::string output;
    output.reserve(2 * len);
    for (size_t i = 0; i < len; ++i)
    {
        const unsigned char c = input[i];
        output.push_back(lut[c >> 4]);
        output.push_back(lut[c & 15]);
    }
    return output;
}

/* Main function */
int main(int argc, char *argv[]) {
    int serial_fd = 0;
    std::string serialPort = "";
    FILE *wstationLogFile;

    SocketConnection phoneClient(RX_PORT);
    
    wstationLogFile = fopen("wstation.log", "a+");

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    printf("OS_TYPE: %d\n", (int)OS_TYPE);
    
    if (argc > 1) {
        serialPort = argv[1];
    } else {
        serialPort = getSerialPortName();
        std::cout << "Initializing serial port: " << serialPort << "..." << std::endl;
    }

    int initResult = 1;
    while ((initResult = initializeSerialPort(&serialPort, &serial_fd)) != 0 && keepRunning) {
        std::cout << "Serial port initialization failed, retrying in " << SERIAL_PORT_INIT_TIMEOUT_S << " sec ..." << std::endl;
        sleep(SERIAL_PORT_INIT_TIMEOUT_S);

        if (serialPort == "") {serialPort = getSerialPortName();}
    }

    if (initResult != 0) {return 1;}

    std::cout << "Initialization done, port name: " << serialPort << ", fd: " << serial_fd << std::endl;

    char read_buf[199];
    int num_ready_fds = 0;

    /* Set up file descriptofs for select() */
    fd_set read_fds;
    FD_ZERO(&read_fds);

    /* Add socket fd to monitoring */
    FD_SET(serial_fd, &read_fds);
    FD_SET(phoneClient.GetSocketFd(), &read_fds);

    /* Set up select() timeout */
    struct timeval timeout = {.tv_sec = SELECT_TIMEOUT_S, .tv_usec = 0};

    /* Server loop */
    while (keepRunning) {

        memset(&read_buf, '\0', sizeof(read_buf));

        num_ready_fds = select(serial_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (num_ready_fds < 0) {
            std::cerr << "select() error";
            std::cout 
            << ", sfd=" << serial_fd 
            << ", numfds=" << num_ready_fds
            << ", rfds.fds_bits=" << *read_fds.fds_bits
            << std::endl;
        } else if (num_ready_fds == 0) {
            /* Select timeout */
        } else {
            /* Check ready descriptors */
            if (FD_ISSET(serial_fd, &read_fds)) {                
                int num_bytes = read(serial_fd, &read_buf, sizeof(read_buf));
                if (num_bytes > 0) {
                    // Print received data to stdout
                    //std::cout << "bytes: " << num_bytes << "; " << read_buf;

                    // for (auto& c : read_buf) {
                    //     std::cout << std::hex << (int)c << " ";
                    // }
                    // std::cout << std::endl;

                    // std::string newstr = std::string(read_buf).substr(0, num_bytes - 2) + "some more stuff";
                    // std::string newstr = std::string(read_buf).substr(0, num_bytes - 20);

                    ArduinoData receivedData(read_buf);

                    // Write received data to log file
                    //fprintf(wstationLogFile, "%s", read_buf);
                    if (receivedData.GetString() != std::string("")) {
                        fprintf(wstationLogFile, "%s", receivedData.GetString().c_str());
                        //fprintf(wstationLogFile, "%s\n", parseDataToJsonRegex(read_buf).c_str());
                        fflush(wstationLogFile);
                    }

                } else {
                    /* Read error, reinitialize serial connection */
                    std::cout << "Port read error, num_bytes="<< num_bytes << std::endl;
                    std::cout << "Re-initializing serial port." << std::endl;
                    
                    close(serial_fd);
                    sleep(SERIAL_PORT_INIT_TIMEOUT_S);

                    while (initializeSerialPort(&serialPort, &serial_fd) != 0 && keepRunning) {
                        std::cout << "Re-initializing failed, retrying..." << std::endl;
                        sleep(SERIAL_PORT_INIT_TIMEOUT_S);
                    }

                    FD_ZERO(&read_fds);
                    FD_SET(serial_fd, &read_fds);
                }
            } else if (FD_ISSET(phoneClient.GetSocketFd(), &read_fds)) {
                std::cout << "Phone client data received." << std::endl;
                auto data = phoneClient.Recv();
                std::cout << "Phone data received bytes: " << data.size() << ", data: " << data << std::endl;
                
                std::string dataTail;
                try {
                    Date referenceDate = ParseDate(data, dataTail);
                    referenceDate.Print();

                    try {
                        Time referenceTime = ParseTime(dataTail);
                        referenceTime.Print();

                        auto foundline = getLineFromLogFile2(&referenceDate, &referenceTime);
                        std::cout << "Found line: " << foundline << std::endl;

                    } catch(const std::exception& e) {
                        std::cerr << e.what() << '\n';
                    }
                } catch(const std::exception& e) {
                    std::cerr << e.what() << '\n';
                }

                

                std::string response = "DUMMY server response.";
                phoneClient.Send(response, response.size());
            } else {
                std::cout << "select() error: unknown fd(" << serial_fd << ")." << std::endl;
            }
        }

        timeout.tv_sec = SELECT_TIMEOUT_S;
        timeout.tv_usec = 0;

        FD_SET(serial_fd, &read_fds);
        FD_SET(phoneClient.GetSocketFd(), &read_fds);
    }

    /* Close serial port, socket and file */
    std::cout << "Closing all descriptors." << std::endl;
    close(serial_fd);
    close(phoneClient.GetSocketFd());
    fclose(wstationLogFile);

    return 0;
}


std::string getLineFromLogFile(std::string timestamp) {
    std::string line;
    std::ifstream wstationLogFile("wstation.log");
    if (wstationLogFile.is_open()) {
        while (getline(wstationLogFile, line)) {
            if (line.find(timestamp) != std::string::npos) {
                return line;
            }
        }
        wstationLogFile.close();
    }
    return "";
}

std::string getLineFromLogFile2(const Date* refDate, const Time* refTime) {
    std::string line, dateTimeBuffer;
    std::ifstream wstationLogFile("wstation.log", std::ios::in);
    if (wstationLogFile.is_open()) {
        std::streampos begin, end, currentPos, startPos, endPos;
        // find the size of the file
        begin = wstationLogFile.tellg();
        wstationLogFile.seekg(0, std::ios::end);
        end = wstationLogFile.tellg();
        std::cout << "File size: " << (end-begin) << ", beg=" << begin << ", end=" << end << ", mid=" << (end-begin)/2 << std::endl;

        int numRecords = end / RECORD_LENGTH;
        int iterations = 1;
        bool endSearch = false;

        currentPos = numRecords / 2;
        startPos = begin;
        endPos = end / RECORD_LENGTH;

        wstationLogFile.seekg (0, wstationLogFile.beg);

        while (!endSearch) {
            // Set read position in the middle of the file and read next line
            wstationLogFile.seekg(currentPos * RECORD_LENGTH, std::ios::beg);
            getline(wstationLogFile, line);
            
            std::string tsDateInLine = getDate(line);
            std::cout << "Itr: " << iterations 
                        << ", startPos=" << startPos
                        << ", currentPos=" << currentPos
                        << "(abs=" << currentPos * RECORD_LENGTH << ")"
                        << ", endPos=" << endPos
                        << ", searching for: " << refDate->ToString()
                        << ", tsDateInLine=" << tsDateInLine
                        << ", Line in the middle: " << line << std::endl;
            
            // find the timestamp in the line
            //std::size_t found = line.find(refDate->ToString());
            

            if (tsDateInLine != "") { //found != std::string::npos
                //std::size_t found = line.find(refDate->ToString());
                
                //Date lineDate = ParseDate(line.substr(found, TIMESTAMP_LENGTH), dateTimeBuffer);
                Date lineDate = ParseDate(tsDateInLine, dateTimeBuffer);
                
                if (lineDate == *refDate) {
                    /*
                    // timestamp is found, check if the time is before or after the line
                    std::string lineTime = line.substr(TIMESTAMP_LENGTH + 1, TIMESTAMP_LENGTH);
                    Time lineTime = ParseTime(dateTimeBuffer);
                    std::cout << "Line time: " << lineTime << std::endl;
                    
                    if (lineTime.compare(refTime->ToString()) < 0) {
                        // time is after the line, search in the second half of the file
                        startPos = currentPos;
                        currentPos = (endPos - currentPos) / 2;
                    } else if (lineTime.compare(refTime->ToString()) > 0) {
                        // time is before the line, search in the first half of the file
                        endPos = currentPos;
                        currentPos = (currentPos - startPos) / 2;
                    } else {
                        // time is found, return the line
                        return line;
                    }*/
                    std::cout << "----------> FOUND!!!" << std::endl;
                    endSearch = true;
                } else if (lineDate < *refDate) {
                    // timestamp is after the line, search in the second half of the file
                    startPos = currentPos;
                    currentPos = currentPos + (endPos - currentPos) / 2;
                    std::cout << "----------> AFTER!!!" << std::endl;
                } else {
                    // timestamp is before the line, search in the first half of the file
                    endPos = currentPos;
                    currentPos = (currentPos - startPos) / 2;
                    std::cout << "----------> BEFORE!!!" << std::endl;
                }
            } else {
                // // if timestamp is not found, check if the timestamp is before or after the line
                // std::string lineTimestamp = line.substr(0, TIMESTAMP_LENGTH);
                // std::cout << "Line timestamp: " << lineTimestamp << std::endl;
                // if (lineTimestamp.compare(timestamp) < 0) {
                //     // timestamp is after the line, search in the second half of the file
                //     begin = wstationLogFile.tellg();
                // } else {
                //     // timestamp is before the line, search in the first half of the file
                //     end = wstationLogFile.tellg();
                // }

                /* Search in the next line */
            }

            iterations++;

            if (startPos == endPos || iterations > numRecords) {
                endSearch = true;
            }
        }


        // while (begin < end) {
        //     std::streampos mid = begin + (end - begin) / 2;
        //     wstationLogFile.seekg(mid);
        //     std::getline(wstationLogFile, line);
        //     std::cout << "Line in the middle: " << line << std::endl;
        //     if (line.find(timestamp) != std::string::npos) {
        //         return line;
        //     } else if (line.compare(timestamp) < 0) {
        //         begin = mid + 1;
        //     } else {
        //         end = mid;
        //     }
        //}
        
        
        
        
        // // set read position in the middle of the file and read next line
        // wstationLogFile.seekg((end-begin)/2, std::ios::beg);
        // getline(wstationLogFile, line);
        // std::cout << "Line in the middle: " << line << std::endl;
        
        // // find the timestamp in the line
        // std::size_t found = line.find(timestamp);
        // if (found != std::string::npos) {
        //     return line;
        // }


        // while (getline(wstationLogFile, line)) {
        //     if (line.find(timestamp) != std::string::npos) {
        //         return line;
        //     }
        // }
        wstationLogFile.close();
        return line;
    }
    return "";
}

std::string getLinesFromLogFile(std::string timestamp1, std::string timestamp2) {
    std::string line;
    std::string lines;
    std::ifstream wstationLogFile("wstation.log");
    if (wstationLogFile.is_open()) {
        while (getline(wstationLogFile, line)) {
            if (line.find(timestamp1) != std::string::npos) {
                lines += line + "\n";
                while (getline(wstationLogFile, line)) {
                    if (line.find(timestamp2) != std::string::npos) {
                        lines += line + "\n";
                        return lines;
                    }
                    lines += line + "\n";
                }
            }
        }
        wstationLogFile.close();
    }
    return "";
}

bool containsDate(const std::string& line)
{
    std::smatch matches;
    std::regex datePattern("Time: (\\d{4}/\\d{2}/\\d{2})");  // (Time: \d{4}/\d{2}/\d{2}\s\d{2}:\d{2}:\d{2})
    
    if (std::regex_search(line, matches, datePattern)) {
        return true;
    }

    return false;
}

std::string getDate(const std::string& line)
{
    std::smatch matches;
    std::regex datePattern("Time: (\\d{4}/\\d{2}/\\d{2})");  // (Time: \d{4}/\d{2}/\d{2}\s\d{2}:\d{2}:\d{2})
    
    if (std::regex_search(line, matches, datePattern)) {
        return matches[1];
    }

    return "";
}