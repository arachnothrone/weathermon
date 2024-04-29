/******************************
 * rpi weather monitor server *
 * (c) arachnothrone 2023     *
 ******************************/

#include <iostream>
#include <fstream>
//#include <string>
#include <vector>
#include <tuple>
#include <exception>

#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <cstring>
#include <algorithm>
#include <regex>

#include <signal.h>
// #include <stdatomic.h>
#include <atomic>

#include "rpiwserver.h"
#include "comm.h"
#include "datetime.h"

// #include <google/protobuf/io/zero_copy_stream_impl.h>
// #include <google/protobuf/text_format.h>
// #include <google/protobuf/message.h>
//#include "arduino_data.pb.h"

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
#define DATE_SEARCH_STEP 10                                         // search step in days

std::vector<std::string> executeCommand(const char* cmd);
std::string getLineFromLogFile(std::string timestamp);
std::string getLineFromLogFile3(const Date* refDate, const Time* refTime);
std::tuple<std::streampos, std::streampos> getDateRangeFromLogFile(std::ifstream &file, const Date &refDate);
std::streampos findBoundary(std::ifstream &file, const Date &refDate, bool direction, std::streampos intervalStart, std::streampos intervalEnd, std::streampos fileEnd);
std::string string_to_hex(const std::string& input);
bool containsDate(const std::string& line);
std::string getDate(const std::string& line);
std::string getTime(const std::string& line);
CMD_E getClientCmdCode(const std::string& data);
int validateCmdCode(const int& cmdCode, const std::string& data);

//static volatile atomic_int keepRunning = 1;
static volatile std::atomic<int> keepRunning; 

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
    this->ReadBuffer(data);
}

ArduinoData::ArduinoData() {
    _dataString = "";
    _temperature = 0.0;
    _humidity = 0.0;
    _pressure = 0.0;
    _ambientLight = 0;
    _vcc = 0;
    _stats = {0, 0, 0};
}

ArduinoData::~ArduinoData() {}

std::string ArduinoData::GetString() const
{
    return _dataString;
}

void ArduinoData::ReadBuffer(const char* const data)
{
    std::string dataStr(data);
    std::smatch matches;
    std::regex timePattern("Time: (\\d{4}/\\d{2}/\\d{2}\\s\\d{2}:\\d{2}:\\d{2})");  // (Time: \d{4}/\d{2}/\d{2}\s\d{2}:\d{2}:\d{2})
    std::regex dataPattern("");

    if (std::regex_search(dataStr, matches, timePattern)) {
        if (dataStr.length() == RECORD_LENGTH) {
            /* Timestamp found and data length is correct */
            _dataString = dataStr;

            //regex: ".*Temperature: (\d{2}.\d) C, Humidity: (\d{2}.\d) %, Pressure: (\d{3}.\d{2}) mmHg, Ambient:\s{1,2}(\d{3,4}), Vcc: (\d{1,4}) mV, STATS:\s{0,3}(\d{1,4})\s{1,4}(\d{1,4})\s{1,4}(\d{1,4})"
            std::regex dataPattern(".*Temperature: (\\d{2}\\.\\d) C, Humidity: (\\d{2}\\.\\d) %, Pressure: (\\d{3}\\.\\d{2}) mmHg, Ambient:\\s{1,2}(\\d{3,4}), Vcc: (\\d{1,4}) mV, STATS:\\s{0,3}(\\d{1,4})\\s{1,4}(\\d{1,4})\\s{1,4}(\\d{1,4})");
            std::smatch matches;
            if (std::regex_search(dataStr, matches, dataPattern)) {
                _temperature = std::stod(matches[1]);
                _humidity = std::stod(matches[2]);
                _pressure = std::stod(matches[3]);
                _ambientLight = std::stoi(matches[4]);
                _vcc = std::stoi(matches[5]);
                _stats = {
                    (uint16_t) std::stoi(matches[6]),
                    (uint16_t) std::stoi(matches[7]),
                    (uint16_t) std::stoi(matches[8])
                };

                _lastValidDataString = _dataString;
                std::cout << "Data received: T/H/P/A/Vcc: " << _temperature << "/" << _humidity << "/" << _pressure << "/" << _ambientLight << "/" << _vcc << std::endl;
            }

        } else {
            /* Timestamp found but data length is incorrect */
            _dataString = std::string("TiMe: ").append(matches[1]) + std::string(RECORD_LENGTH - 19 - 6 - 1, '.') + std::string("\n");
        }
    } else {
        /* No timestamp, ignore */
        _dataString = "";
    }
}

double ArduinoData::GetTemperature() const {
    return _temperature;
}

double ArduinoData::GetHumidity() const {
    return _humidity;
}

double ArduinoData::GetPressure() const {
    return _pressure;
}

uint16_t ArduinoData::GetAmbientLight() const {
    return _ambientLight;
}

uint16_t ArduinoData::GetVcc() const {
    return _vcc;
}

STATS_T ArduinoData::GetStats() const {
    return _stats;
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

DataMessage::DataMessage(const std::string wsLogFileName, const std::tuple<std::streampos, std::streampos> dataBlockRange)
{
    std::streampos upperBoundary = std::get<0>(dataBlockRange);
    std::streampos lowerBoundary = std::get<1>(dataBlockRange);

    _numRecords = lowerBoundary / RECORD_LENGTH - upperBoundary / RECORD_LENGTH + 1;

    _records = new RECORD_T[_numRecords];
    //_records = static_cast<RECORD_T*>(malloc(_numRecords * sizeof(RECORD_T)));

    _wsLogFile = std::ifstream(wsLogFileName, std::ios::in);

    // read _wsLogFile from upperBoundary to lowerBoundary into _records
    if (_wsLogFile.is_open())
    {
        std::string line;
        std::streampos currentLinePos = upperBoundary / RECORD_LENGTH;
        _wsLogFile.seekg (0, _wsLogFile.beg);
        
        std::regex timePattern("Time: ([0-9]{4}/[0-9]{2}/[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}), ");
        std::regex temperaturePattern("Temperature: ([0-9]+.[0-9]+) C, ");
        std::regex humidityPattern("Humidity: ([0-9]+.[0-9]+) %, ");
        std::regex pressurePattern("Pressure: ([0-9]+.[0-9]+) mmHg, ");
        std::regex ambientPattern("Ambient:\\s{1,}([0-9]+), ");
        std::regex vccPattern("Vcc: ([0-9]+) mV");
        // std::regex statsPattern("STATS: (\\s{1,}[0-9]{1,3}){3}");
        std::regex statsPattern("STATS:\\s{1,}([0-9]{1,3})\\s{1,}([0-9]{1,3})\\s{1,}([0-9]{1,3})");

        std::smatch matches;

        for (int i = 0; i < _numRecords; i++)
        {
            bool bParsed = false;

            _wsLogFile.seekg(currentLinePos * RECORD_LENGTH, std::ios::beg);
            getline(_wsLogFile, line);

            std::cout << "Line: " << line << std::endl;

            if (std::regex_search(line, matches, timePattern)) {
                bParsed = true;
            } else {bParsed = false;}
            if (bParsed == true && std::regex_search(line, matches, temperaturePattern)) {
                _records[i].temperature = stod(matches[1]);
            } else {bParsed = false;}
            if (bParsed == true && std::regex_search(line, matches, humidityPattern)) {
                _records[i].humidity = stod(matches[1]);
            } else {bParsed = false;}
            if (bParsed == true && std::regex_search(line, matches, pressurePattern)) {
                _records[i].pressure = stod(matches[1]);
            } else {bParsed = false;}
            if (bParsed == true && std::regex_search(line, matches, ambientPattern)) {
                _records[i].ambientLight = stoi(matches[1]);
            } else {bParsed = false;}
            if (bParsed == true && std::regex_search(line, matches, vccPattern)) {
                _records[i].vcc = stoi(matches[1]);
            } else {bParsed = false;}
            if (bParsed == true && std::regex_search(line, matches, statsPattern)) {
                _records[i].stats.totalNrOfRestarts = stoi(matches[1]);
                _records[i].stats.successCounter = stoi(matches[2]);
                _records[i].stats.rxBufferOverrunCntr = stoi(matches[3]);
            } else {bParsed = false;}

            currentLinePos + (std::streampos) 1;
        }
        _wsLogFile.close();
    }
}

DataMessage::~DataMessage()
{
    delete[] _records;
    // free(_records);
    // _records = nullptr;
}

void DataMessage::PrintMessage() const
{
    for (int i = 0; i < _numRecords; i++)
    {
        std::cout 
            << "Record " << i << ": " 
            << _records[i].temperature << ", " 
            << _records[i].humidity << ", " 
            << _records[i].pressure << ", " 
            << _records[i].ambientLight << ", " 
            << _records[i].vcc << ", " 
            << _records[i].stats.totalNrOfRestarts << ", " 
            << _records[i].stats.successCounter << ", " 
            << _records[i].stats.rxBufferOverrunCntr 
            << std::endl;
    }
}


/* Main function */
int main(int argc, char *argv[]) {
    int serial_fd = 0;
    std::string serialPort = "";
    FILE *wstationLogFile;
    ArduinoData arduinoReadings = ArduinoData();
    keepRunning = 1;

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
                /* Arduino data reveived */
                int num_bytes = read(serial_fd, &read_buf, sizeof(read_buf));
                if (num_bytes > 0) {
                    // Print received data to stdout
                    // std::cout << "bytes: " << num_bytes << "; " << read_buf;

                    // for (auto& c : read_buf) {
                    //     std::cout << std::hex << (int)c << " ";
                    // }
                    // std::cout << std::endl;

                    // std::string newstr = std::string(read_buf).substr(0, num_bytes - 2) + "some more stuff";
                    // std::string newstr = std::string(read_buf).substr(0, num_bytes - 20);

                    arduinoReadings.ReadBuffer(read_buf);

                    // Write received data to log file
                    //fprintf(wstationLogFile, "%s", read_buf);
                    if (arduinoReadings.GetString() != std::string("")) {
                        fprintf(wstationLogFile, "%s", arduinoReadings.GetString().c_str());
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
                /* Client request received */
                std::cout << "Phone client data received." << std::endl;
                auto data = phoneClient.Recv();
                auto cmd = getClientCmdCode(std::string(data));
                std::string dataTail;

                std::string serializedDataMsg;
                // ...QQQ... ArduionSensorsData sensorsDataMsg;
                // ...QQQ... ArduionSensorsData::Stats* stats = sensorsDataMsg.mutable_stats();

                switch (cmd) {
                case GETCURRENTDATA:
                    // sensorsDataMsg.set_temperature(arduinoReadings.GetTemperature());
                    // sensorsDataMsg.set_humidity(arduinoReadings.GetHumidity());
                    // sensorsDataMsg.set_pressure(arduinoReadings.GetPressure());
                    // sensorsDataMsg.set_ambientlight(arduinoReadings.GetAmbientLight());
                    // sensorsDataMsg.set_vcc(arduinoReadings.GetVcc());
                    // stats->set_totalnrofrestarts(arduinoReadings.GetStats().totalNrOfRestarts);
                    // stats->set_successcounter(arduinoReadings.GetStats().successCounter);
                    // stats->set_rxbufferoverruncntr(arduinoReadings.GetStats().rxBufferOverrunCntr);
                    // sensorsDataMsg.SerializeToString(&serializedDataMsg);
                    // phoneClient.Send(serializedDataMsg, serializedDataMsg.size());
                    break;
                case GETDATARANGE:
                    std::cout << "Phone data received bytes: " << data.size() << ", data: " << data << std::endl;
                    try
                    {
                        Date referenceDate = ParseDate(data, dataTail);
                        referenceDate.Print();

                        try {
                            Time referenceTime = ParseTime(dataTail);
                            referenceTime.Print();

                            auto foundline = getLineFromLogFile3(&referenceDate, &referenceTime);

                        } catch(const std::exception& e) {
                            std::cerr << e.what() << '\n';
                        }
                    }
                    catch(const std::exception& e)
                    {
                        std::cerr << e.what() << '\n';
                    }
                    break;
                default:
                    std::cout << "Unknown or invalid client command: " << data << ", size=" << data.size() << std::endl;
                    break;
                }

                std::string response = "DUMMY server response.";
                //phoneClient.Send(response, response.size());
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

std::string getLineFromLogFile3(const Date* refDate, const Time* refTime)
{
    std::string line, dateTimeBuffer;
    std::ifstream wstationLogFile("wstation.log", std::ios::in);
    Date date = *refDate;
    if (wstationLogFile.is_open())
    {
        std::tuple<std::streampos, std::streampos> range = getDateRangeFromLogFile(wstationLogFile, date);
        wstationLogFile.seekg(std::get<0>(range), std::ios::beg);
        getline(wstationLogFile, line);
        std::cout << "Interval First line: " << line << std::endl;
        wstationLogFile.seekg(std::get<1>(range), std::ios::beg);
        getline(wstationLogFile, line);
        std::cout << "Interval Last line : " << line << std::endl;

        wstationLogFile.close();

        DataMessage msg("wstation.log", range);
        msg.PrintMessage();

        return line;
    }

    return "";
}

std::tuple<std::streampos, std::streampos> getDateRangeFromLogFile(std::ifstream &file, const Date &refDate)
{
    std::streampos upperBoundary, lowerBoundary;
    std::streampos fbegin, fend;

    // find the size of the file
    fbegin = file.tellg();
    file.seekg(0, std::ios::end);
    fend = file.tellg();
    std::cout << "File size: " << (fend-fbegin) << ", beg=" << fbegin << ", end=" << fend << ", mid=" << (fend - fbegin) / 2 << std::endl;

    upperBoundary = findBoundary(file, refDate, BACKWARD, fbegin, fend, fend);
    lowerBoundary = findBoundary(file, refDate, FORWARD, upperBoundary, fend, fend);

    return std::make_tuple(upperBoundary, lowerBoundary);
}

/* Returns absolute position of the beginning of the line inside the interval's boundary */
std::streampos findBoundary(std::ifstream &file, const Date &refDate, bool direction, std::streampos intervalStart, std::streampos intervalEnd, std::streampos fileEnd)
{
    std::cout << "findBoundary: " << refDate.ToString() << ", dir=" << direction << ", start=" << intervalStart << ", end=" << intervalEnd << std::endl;
    std::string line, testLine, dateTimeBuffer;
    std::streampos currentLinePos, startLinePos, endLinePos;    /* Line number, numbering starts from '0' */

    int numRecords = intervalEnd / RECORD_LENGTH - intervalStart / RECORD_LENGTH + 1;
    int iterations = 1;
    bool endSearch = false;

    startLinePos = intervalStart / RECORD_LENGTH;
    endLinePos = intervalEnd / RECORD_LENGTH;
    currentLinePos = startLinePos + (endLinePos - startLinePos) / 2;

    file.seekg (0, file.beg);   // <-----------------

    while (!endSearch)
    {
        // Set read position in the middle of the given interval and read next line
        file.seekg(currentLinePos * RECORD_LENGTH, std::ios::beg);
        getline(file, line);
        
        std::string tsDateInLine = getDate(line);
        std::string tsTimeInLine = getTime(line);
        std::cout << "   Itr: " << iterations 
                    << ", numRecords=" << numRecords
                    << ", startLinePos=" << startLinePos
                    << ", currentLinePos=" << currentLinePos
                    << "(abs=" << currentLinePos * RECORD_LENGTH << ")"
                    << ", endLinePos=" << endLinePos
                    << ", searching for: " << refDate.ToString()
                    << ", tsDateInLine=" << tsDateInLine
                    << ", tsTimeInLine=" << tsTimeInLine
                    << ", Line in the middle: " << line << std::endl;

        if (tsDateInLine != "" && tsTimeInLine != "")
        {
            Date lineDate = ParseDate(tsDateInLine, dateTimeBuffer);
            Time lineTime = ParseTime(tsTimeInLine);

            if (lineDate == refDate)
            {
                /* Line contains reference date, check the previous/next line to determine if the line is actual boundary */
                std::streampos testLinePos = (direction == BACKWARD) ? (std::streampos) (currentLinePos - (std::streampos) 1) : (std::streampos) (currentLinePos + (std::streampos) 1);
                std::cout << "   Test line: " << testLinePos << std::endl;

                if (currentLinePos == 0 || (currentLinePos >= endLinePos - (std::streampos) 1))     /* Last line in the file is empty */
                {
                    return currentLinePos * RECORD_LENGTH;
                }

                file.seekg(testLinePos * RECORD_LENGTH, std::ios::beg);
                getline(file, testLine);

                if (getDate(testLine) != "")
                {
                    lineDate = ParseDate(getDate(testLine), dateTimeBuffer);
                    if (!(lineDate == refDate))
                    {
                        /* Previous line does not contain reference date, return current position */
                        std::cout << "----------> FOUND 1 !!!" << "currentLinePos=" << currentLinePos << ", abs=" << currentLinePos * RECORD_LENGTH << std::endl;
                        return currentLinePos * RECORD_LENGTH;
                    }
                    else
                    {
                        if (direction == BACKWARD)
                        {
                            currentLinePos = findBoundary(file, refDate, direction, startLinePos * RECORD_LENGTH, currentLinePos * RECORD_LENGTH, fileEnd) / RECORD_LENGTH;
                        }
                        else
                        {
                            currentLinePos = findBoundary(file, refDate, direction, currentLinePos * RECORD_LENGTH, endLinePos * RECORD_LENGTH, fileEnd) / RECORD_LENGTH;
                        }
                        
                        std::cout << "----------> FOUND 2 !!!" << "currentLinePos=" << currentLinePos << ", abs=" << currentLinePos * RECORD_LENGTH << std::endl;
                        return currentLinePos * RECORD_LENGTH;
                    }
                }
            }
            else if (lineDate < refDate)
            {
                /* Timestamp is after the line, search in the second half of the file */
                startLinePos = currentLinePos;
                currentLinePos = currentLinePos + (endLinePos - currentLinePos) / 2;
                std::cout << "----------> AFTER!!!" << "lineDate[" << lineDate.ToString() << "] < refDate[" << refDate.ToString() << "] = " << (lineDate < refDate) << std::endl;
            }
            else
            {
                /* Timestamp is before the line, search in the first half of the file */
                endLinePos = currentLinePos;
                currentLinePos = (currentLinePos - startLinePos) / 2;
                std::cout << "----------> BEFORE!!!" << "lineDate[" << lineDate.ToString() << "] > refDate[" << refDate.ToString() << "] = " << (refDate < lineDate) << std::endl;
            }
        }
        else
        {
            /* No timestamp in this line, search in the next line */
            currentLinePos = currentLinePos + (std::streampos) 1;
        }

        iterations++;

        if (startLinePos == endLinePos || iterations > numRecords) {
            endSearch = true;
        }
    }

    /* Shouldn't get here, leave for debug */
    std::cout << "----------> FOUND 3 !!!" << "currentLinePos=" << currentLinePos << ", abs=" << currentLinePos * RECORD_LENGTH << std::endl;
    
    return currentLinePos * RECORD_LENGTH;
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
    std::regex datePattern("Ti[m, M]e: (\\d{4}/\\d{2}/\\d{2})");  // (Time: \d{4}/\d{2}/\d{2}\s\d{2}:\d{2}:\d{2})
    
    if (std::regex_search(line, matches, datePattern)) {
        return true;
    }

    return false;
}

std::string getDate(const std::string& line)
{
    std::smatch matches;
    std::regex datePattern("Ti[m, M]e: (\\d{4}/\\d{2}/\\d{2})\\s(\\d{2}:\\d{2}:\\d{2})");  // (Time: \d{4}/\d{2}/\d{2}\s\d{2}:\d{2}:\d{2})
    
    if (std::regex_search(line, matches, datePattern)) {
        return matches[1];
    }

    return "";
}

std::string getTime(const std::string& line)
{
    std::smatch matches;
    std::regex timePattern("Ti[m, M]e: (\\d{4}/\\d{2}/\\d{2})\\s(\\d{2}:\\d{2}:\\d{2})");
    
    if (std::regex_search(line, matches, timePattern)) {
        return matches[2];
    }

    return "";
}

int validateCmdCode(const int& cmdCode, const std::string& data)
{
    int ret = INVALID_COMMAND;
    auto it = mCMDMap.find((CMD_E)cmdCode);

    if (it != mCMDMap.end())
    {
        unsigned long int cmdCodeStringLen = mCMDMap.find((CMD_E)cmdCode)->second.length();
        if (data.length() >= cmdCodeStringLen + 2)
        {
            std::string cmdCodeString = data.substr(2, cmdCodeStringLen);
            if (cmdCodeString == mCMDMap.find((CMD_E)cmdCode)->second)
            {
                return cmdCode;
            }
            else
            {
                return INVALID_COMMAND;
            }
        }
    }

    return ret;
}

CMD_E getClientCmdCode(const std::string& data)
{
    std::string cmdCodeString = data.substr(0, 2);
    try
    {
        int cmdCode = std::stoi(cmdCodeString);
        int cmdCodeValidated = validateCmdCode(cmdCode, data);
        return (CMD_E) cmdCodeValidated;
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return INVALID_COMMAND;
    }
}
