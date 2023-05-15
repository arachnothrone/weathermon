/******************************
 * rpi weather monitor server *
 * (c) arachnothrone 2023     *
 ******************************/

#include <iostream>
#include <fstream>
#include <string>
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

/* Define constants */
#define SERIAL_PORT_DEFAULT_PATTERN "/dev/cu.usbmodem*"             // macos
#define BAUD_RATE 9600

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

std::vector<std::string> executeCommand(const char* cmd);

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


    std::cout << "List of usb serialports found:" << std::endl;
    for (auto& line : cmdOutput) {
        std::cout << line << std::endl;
    }

    if (cmdOutput.size() > 0) {
        auto newStrEnd = std::remove(cmdOutput[0].begin(), cmdOutput[0].end(), '\n');
        result = std::string(cmdOutput[0].begin(), newStrEnd);
    }

    std::cout << ">>> reading usb port: " << result << std::endl;
    
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


// Main function
int main(int argc, char *argv[]) {
    int serial_fd = 0;
    FILE *wstationLogFile;
    wstationLogFile = fopen("wstation.log", "a+");

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    printf("OS_TYPE: %d\n", (int)OS_TYPE);
    std::string serialPort;
    if (argc > 1) {
        serialPort = argv[1];
    } else {
        serialPort = getSerialPortName();
        std::cout << "Connecting to serial port: " << serialPort << std::endl;
    }

    

    try {
        std::cout << "port name: " << serialPort << ", cstr: " << serialPort.c_str() << std::endl;
        serial_fd = open(serialPort.c_str(), O_RDWR | O_NOCTTY);

        if (serial_fd > 0) {
            std::cerr << "Serial port fd: " << serial_fd << std::endl;
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
            tcsetattr(serial_fd, TCSANOW, &tty);    // Set port attributes (TCSANOW = make changes immediately)
        } else {
            char errno_buffer[256];
            strerror_r(errno, errno_buffer, 256);
            std::cerr << "Error opening serial port, errno: " << errno << ", " << errno_buffer << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    char read_buf[199];
    int num_ready_fds = 0;

    /* Set up file descriptofs for select() */
    fd_set read_fds;
    FD_ZERO(&read_fds);

    /* Add socket fd to monitoring */
    FD_SET(serial_fd, &read_fds);

    /* Set up select() timeout */
    struct timeval timeout = {.tv_sec = 3, .tv_usec = 0};

    while (keepRunning) {

        memset(&read_buf, '\0', sizeof(read_buf));

        num_ready_fds = select(serial_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (num_ready_fds < 0) {
            std::cerr << "select() error";
            std::cout 
            << ", sfd=" << serial_fd 
            << ", numfds=" << num_ready_fds
            << ", rfds=" << *read_fds.fds_bits
            << std::endl;
        } else if (num_ready_fds == 0) {
            // Select timeout
        } else {
            // Check ready descriptors
            if (FD_ISSET(serial_fd, &read_fds)) {                
                int num_bytes = read(serial_fd, &read_buf, sizeof(read_buf));
                if (num_bytes > 0) {
                    // Print received data to stdout
                    std::cout << "bytes: " << num_bytes << "; " << read_buf;

                    // for (auto& c : read_buf) {
                    //     std::cout << std::hex << (int)c << " ";
                    // }
                    // std::cout << std::endl;

                    // Write received data to log file
                    //fprintf(wstationLogFile, "%s", read_buf);
                    fprintf(wstationLogFile, "%s\n", parseDataToJsonRegex(read_buf).c_str());
                    fflush(wstationLogFile);
                
                } else {
                    std::cout << "read() error" << std::endl;
                }
            } else {
                std::cout << "select() error: unknown fd" << std::endl;
            }
        }

        timeout.tv_sec = 3;
        timeout.tv_usec = 0;

        FD_SET(serial_fd, &read_fds);
    }

    // Close serial port
    std::cout << "Closing serial/logfile descriptors." << std::endl;
    close(serial_fd);
    fclose(wstationLogFile);

    return 0;
}
