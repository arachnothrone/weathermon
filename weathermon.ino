/*

Version with Thread

To build with SD card set BUILD_WITH_SD_CARD to 1, 
this require additional 10 kB of program memory

I2C addresses:

Found address: 60 (0x3C)     - OLED 4-line display
Found address: 60 (0x3C)     - OLED 8-lines
Found address: 87 (0x57)     - RTC 1 (eeprom??? AT24C32)
Found address: 104 (0x68)    - RTC 2 (CLOCK_ADDRESS)
Found address: 119 (0x77)    - pressure sensor
not found AM2320, 92 >>> 0x5C ?

*/

#define BUILD_WITH_SD_CARD  0

#if BUILD_WITH_SD_CARD
#include <SPI.h>      // for sd card
#include <SD.h>
#endif

#include <Thread.h>
#include <Wire.h>     // for AM2320
#include "Adafruit_Sensor.h"  // for AM2320, connect to I2C
#include "Adafruit_AM2320.h"
#include "Adafruit_BMP085.h"  // Pressure sensor, I2C (with pullup resistors onboard)
#include "DS3231.h"   // RTC
#include <U8x8lib.h>
#include <SoftwareSerial.h>

#define PAMMHG                        (0.00750062)
#define RTC_READ_TASK_PERIOD_MS       (1000)
#define RTC_DISPLAY_TASK_PERIOD_MS    (2030)
#define SENSOR_READ_TASK_PERIOD_MS    (2095)
#define SENSOR_DISPLAY_TASK_PERIOD_MS (3100)
#define LOG_BUFFER_LEN                (106)
#define SD_LOG_FILE_NAME_LEN          (13)

#define MAIN_TASK_PERIOD_MS             (10000)
#define BLINK_SEPARATOR_TASK_PERIOD_MS  (500)
#define BLUETOOTH_TASK_PERIOD_MS        (600)

#define BT_READBUFFER_SIZE              (11)
#define BT_SENDBUFFER_SIZE              (105)

//AM2320 temp_humid;
Adafruit_AM2320 temp_humid = Adafruit_AM2320();
Adafruit_BMP085 bmp;
RTClib RTC;   // DS3231 Clock;  --> Clock.setClockMode(false);	// set to 24h

U8X8_SSD1306_128X32_UNIVISION_HW_I2C oled(/* reset=*/ U8X8_PIN_NONE, /* clock=*/ SCL, /* data=*/ SDA);   // pin remapping with ESP8266 HW I2C

#if BUILD_WITH_SD_CARD
File logWeatherMonitor;
#endif

SoftwareSerial swSerial(8, 9); // RX, TX

Thread mainTask = Thread();             // Main task, query all sensors every 10 seconds
Thread blinkSeparatorTask = Thread();   // Blink HH:MM separator on display every 500 ms
Thread communicationTask = Thread();    // BT communication task, run every 600 ms

typedef struct {
  int year;
  int mnth;
  int day;
  int hrs;
  int min;
  int sec;
  bool flashSeparators;
  bool winterTime;
  bool summerTime;
} TimeDateStorage;

typedef struct
{
  float temperature;    // C
  float humidity;       // %
  float pressure;       // mmHg
  int   ambientLight;   // 0 - 1010 (dark)
  char  am2320error[20];  // "AM_ERR: CRC Failed" or "AM_ERR: OFFLINE"
} SensDataStorage;

static TimeDateStorage currentTimeDate = {0, 0, 0, 0, 0, 0, true, false, false};
static SensDataStorage sensorsData = {0, 0, 0, 0};

#if BUILD_WITH_SD_CARD
static char     GV_LogFileName[SD_LOG_FILE_NAME_LEN] = "wmddhhmm.txt";
static bool     GV_LogFileErrorIndicator = false;
#endif


void clockReadFunc(TimeDateStorage* tds)
{
  /* Read current date and time & display */
  DateTime now = RTC.now();
  tds->year = now.year();
  tds->mnth = now.month();
  tds->day = now.day();
  tds->hrs = now.hour();
  tds->min = now.minute();
  tds->sec = now.second();

  // ------------------------------------ Start of DO NOT REMOVE ------------------------------------
  /* TODO: daylight saving time adjustment:
   * use DS3231 eeprom to store winterTime/summerTime
   */
  /* Check for and set the daylight saving time */
  // if ((tds->sec > 35 || tds->sec <=10) && (tds->winterTime == false))
  // {
  //   Serial.print("W::Winter="); Serial.print(tds->winterTime);
  //   Serial.print(" W::Summer="); Serial.println(tds->summerTime);
    
  //   // set winter time, +1 hour
  //   Serial.print("Seting winter time\n");
  //   adjustDaylightSavingTime(tds, 1);

  //   tds->winterTime = true;
  //   tds->summerTime = false;
  // }

  // if ((tds->sec <= 35 && tds->sec > 10) && (tds->summerTime == false))
  // {
  //   Serial.print("S::Winter="); Serial.print(tds->winterTime);
  //   Serial.print(" S::Summer="); Serial.println(tds->summerTime);
    
  //   // set summer time, -1 hour
  //   Serial.print("Setting summer time\n");
  //   adjustDaylightSavingTime(tds, -1);

  //   tds->winterTime = false;
  //   tds->summerTime = true;
  // }
  // ------------------------------------- End of DO NOT REMOVE -------------------------------------
}

// ------------------------------------ Start of DO NOT REMOVE ------------------------------------
/* TODO: daylight saving time adjustment */
// void adjustDaylightSavingTime(TimeDateStorage* pTimeDateStorage, int8_t adjValue)
// {
//   DS3231 Clock;
//   bool h12_format;
//   bool pm;

//   Serial.print(String("Old Clock: ") + pTimeDateStorage->hrs + ":" + pTimeDateStorage->min + ":" + pTimeDateStorage->sec);
//   Clock.setHour(pTimeDateStorage->hrs + adjValue);
//   Serial.print(String(", New Clock: ") + (Clock.getHour(h12_format, pm)) + ":" + (pTimeDateStorage->min) + ":" + pTimeDateStorage->sec + " \n");
// }
// ------------------------------------- End of DO NOT REMOVE -------------------------------------

void clockDisplayFunc(TimeDateStorage* tds)
{
  /* Display clock values on the screen */
  
  printRtcValueOnDisplay(0, 0, tds->hrs);

  printRtcValueOnDisplay(3, 0, tds->min);

  printRtcValueOnDisplay(6, 0, tds->day);
  oled.print("-");

  printRtcValueOnDisplay(9, 0, tds->mnth);
  oled.print("-");

  oled.setCursor(12, 0);
  oled.print(tds->year);
}

/**
 * Print Hour, Minute, Second, Date or Month to occupy 2 decimal places
 * (when value is "1" -> print "01")
 */
void printRtcValueOnDisplay(uint8_t xPos, uint8_t yPos, uint8_t value)
{
  oled.setCursor(xPos, yPos);
  if (value < 10)
  {
    oled.print("0");
  }
  oled.print(value);
}

/**
 * Read AM2320 - temperature and humidity, 
 *      BMP085 - pressure,
 *             - ambient light
 * params:
 *      pSensorsData      [OUT]
 */
void sensorReadFunc(SensDataStorage* pSensorsData)
{
  pSensorsData->temperature = temp_humid.readTemperature();
  pSensorsData->humidity = temp_humid.readHumidity();
  pSensorsData->pressure = bmp.readPressure() * PAMMHG;
  pSensorsData->ambientLight = analogRead(A0);
} 

/**
 * Log data to serial interface, display and file 
 * 
 * params:
 *      pSensorsData      [IN]
 *      pCurrentTimeDate  [IN]
 */
void sensorsDataLogFunc(SensDataStorage* const pSensorsData, TimeDateStorage* const pCurrentTimeDate)
{
  char logBuffer[LOG_BUFFER_LEN];

  sprintf(logBuffer
    , "Time: %04d/%02d/%02d %02d:%02d:%02d, Temperature: %2d.%1d C, Humidity: %2d.%1d %%, Pressure: %3d.%02d mmHg, Ambient: %4d\n"
    , pCurrentTimeDate->year, pCurrentTimeDate->mnth, pCurrentTimeDate->day
    , pCurrentTimeDate->hrs, pCurrentTimeDate->min, pCurrentTimeDate->sec
    , (int)pSensorsData->temperature, (int)(pSensorsData->temperature * 10) % 10
    , (int)pSensorsData->humidity, (int)(pSensorsData->humidity * 10) % 10
    , (int)pSensorsData->pressure, (int)(pSensorsData->pressure * 100) % 100
    , pSensorsData->ambientLight
    );
  
  // String logString = String("Time: ") 
  //   + (pCurrentTimeDate->year < 10 ? "0" : ""  ) + pCurrentTimeDate->year
  //   + (pCurrentTimeDate->mnth < 10 ? "/0" : "/") + pCurrentTimeDate->mnth
  //   + (pCurrentTimeDate->day  < 10 ? "/0" : "/") + pCurrentTimeDate->day
  //   + (pCurrentTimeDate->hrs  < 10 ? " 0" : " ") + pCurrentTimeDate->hrs
  //   + (pCurrentTimeDate->min  < 10 ? ":0" : ":") + pCurrentTimeDate->min 
  //   + (pCurrentTimeDate->sec  < 10 ? ":0" : ":") + pCurrentTimeDate->sec + "," 
  //   + " Temperature: " + pSensorsData->temperature 
  //   + " C, Humidity: " + pSensorsData->humidity
  //   + " %, Pressure: " + pSensorsData->pressure
  //   + " mmHg, Ambient: " + pSensorsData->ambientLight
  //   + "\n";

  // Serial.print(logString);
  Serial.print(logBuffer);

  oled.setCursor(0, 1);
  oled.print("Temp: ");
  oled.print(int(pSensorsData->temperature));
  oled.print(" C  ");
  oled.setCursor(0, 2);
  oled.print("Humd: ");
  oled.print(int(pSensorsData->humidity));
  oled.print(" %");
  oled.setCursor(0, 3);
  oled.print("Prss: ");
  oled.print(int(pSensorsData->pressure));
  oled.print(" mmHg");

#if BUILD_WITH_SD_CARD
  // -------------- log to the file
  if (GV_LogFileErrorIndicator != true)
  {
    logWeatherMonitor = SD.open(GV_LogFileName, FILE_WRITE); //O_APPEND
    if(logWeatherMonitor)
    {
      logWeatherMonitor.print(logBuffer);
      // logAsklitt.flush();
      // logAsklitt.close();
    }
    else
    {
      /* Error indicator */
      oled.drawString(15, 3, "*");
      // oled.setCursor(11, 2);
      // oled.print(logAsklitt);
      // Serial.println(logAsklitt.availableForWrite());
    }
    logWeatherMonitor.close();
  }
#endif
}

#if BUILD_WITH_SD_CARD
void sdCardProgram()
{
  // SD reader is connected to SPI pins (MISO/MOSI/SCK/CS, 5V pwr)
  // setup sd card variables
  Sd2Card card;
  SdVolume volume;
  SdFile root;

  oled.drawString(0, 0, "SD Card Init...");

  if (!card.init(SPI_HALF_SPEED, SD_CHIP_SELECT_PIN))  // SPI_HALF_SPEED, 4    SPI_FULL_SPEED SD_CHIP_SELECT_PIN
  {
    int sdErrCode = card.errorCode();
    int sdErrData = card.errorData();
    
    oled.drawString(0, 1, "SD Init Failed.");
    oled.drawString(0, 2, "ErrCode/Data:");
    oled.setCursor(0, 3);
    oled.print(sdErrCode);
    oled.setCursor(5, 3);
    oled.print(sdErrData);
    delay(4000);
  }
  else
  {
    oled.drawString(0, 1, "SD Init OK.");
  }
  delay(500);
  //char cardType[10];
  String cardType = "----";
  switch (card.type()) {
    case SD_CARD_TYPE_SD1:
      cardType = "SD1";
      break;
    case SD_CARD_TYPE_SD2:
      cardType = "SD2";
      break;
    case SD_CARD_TYPE_SDHC:
      cardType = "SDHC";
      break;
    default:
      cardType = "--";
  }
  oled.drawString(0, 2, "Card Type: ");
  oled.setCursor(11, 2);
  oled.print(cardType);
  delay(1300);
  if (!volume.init(card)) {
    oled.drawString(0, 3, "No FAT partition");
    delay(3000);
    oled.clear();
  }
  else {
    oled.clear();
    uint32_t volumeSize;
    volumeSize = volume.blocksPerCluster();
    volumeSize *= volume.clusterCount();
    volumeSize /= 2;                           /* One SD card block is 512 bytes */
    oled.print("Vol. size (Kb):\n");
    oled.print(volumeSize);
    delay(1500);
    oled.clear();

    SD.begin(SPI_HALF_SPEED, SD_CHIP_SELECT_PIN);
  }
}
#endif

void communicationFunc(SensDataStorage* const pSensorsData, TimeDateStorage* const pCurrentTimeDate)
{
  if (swSerial.available())
  {
    // 0x0D, 0x0A - CR, LF
    char readBuffer[BT_READBUFFER_SIZE] = {0};
    char sendBuffer[BT_SENDBUFFER_SIZE] = {0};
    int i = 0;
    int j = 0;
    int k = 0;

    while (swSerial.available() && (i < BT_READBUFFER_SIZE - 1)) 
    {
      readBuffer[i] = swSerial.read();
      i++;
    }

    // DEBUG print received to console
    for (j = 0; j < i; j++)
    {
      Serial.print(readBuffer[j], HEX);
      Serial.print(" ");
    }

    Serial.print("\n");
    Serial.println(readBuffer);
    Serial.print("Bytes received: ");
    Serial.println(i);

    // Flush the rest of the BT sw serial buffer if received more than BT_READBUFFER_SIZE
    if (swSerial.available())
    {
      while (swSerial.available())
      {
        swSerial.read();
        k++;
      }
      Serial.print(k);
      Serial.print(" extra bytes dumped.\n");
    }

    oled.setCursor(13, 2);
    oled.print("->");

    // Time: 2021/02/06 16:29:43, Temperature: 26.9 C, Humidity: 13.8 %, Pressure: 744.43 mmHg, Ambient:  936
    sprintf(sendBuffer
    , "Time: %04d/%02d/%02d %02d:%02d:%02d, Temperature: %2d.%1d C, Humidity: %2d.%1d %%, Pressure: %3d.%2d mmHg, Ambient: %4d\n"
    , pCurrentTimeDate->year, pCurrentTimeDate->mnth, pCurrentTimeDate->day
    , pCurrentTimeDate->hrs, pCurrentTimeDate->min, pCurrentTimeDate->sec
    , (int)pSensorsData->temperature, (int)(pSensorsData->temperature * 10) % 10
    , (int)pSensorsData->humidity, (int)(pSensorsData->humidity * 10) % 10
    , (int)pSensorsData->pressure, (int)(pSensorsData->pressure * 100) % 100
    , pSensorsData->ambientLight
    );
    
    swSerial.write(sendBuffer);

    oled.setCursor(13, 2);
    oled.print("  ");
  }
}

/**
 * Main Task funcion:
 * 
 * Call reading RTC/sensors + writing to log/dislpay every 10 seconds
 * typical task execution time ~9 (8-12) ms (for IIC devices only)
 */
void mainTaskFunction()
{
  clockReadFunc(&currentTimeDate);
  sensorReadFunc(&sensorsData);
  clockDisplayFunc(&currentTimeDate);
  sensorsDataLogFunc(&sensorsData, &currentTimeDate);
}

/**
 * Communication Task funcion:
 * 
 * Check if there is a request arrived to the BT module, 
 * perform appropriate response
 */
void communicationTaskFunction()
{
  communicationFunc(&sensorsData, &currentTimeDate);
}

/**
 * Blink Separator Task funcion:
 * 
 * Flash hours-minutes separator every 0.5 sec
 */
void blinkSeparatorTaskFunction()
{
  blinkClockSeparators();
}

void setup()
{
  // Set up serial interfaces
  Wire.begin();
  Serial.begin(115200);
  swSerial.begin(9600);                     // BT HC-06 module connected to pin 8, 9
  
  // Set up OLED display
  oled.begin();
  oled.setPowerSave(0);
  oled.setFont(u8x8_font_chroma48medium8_r); // u8x8_font_chroma48medium8_r u8x8_font_px437wyse700a_2x2_r u8g2_font_courB12_tf 
  oled.setContrast(11);

#if BUILD_WITH_SD_CARD
  sdCardProgram();
#endif

  // Set up sensors
  temp_humid.begin();   // init am2320
  bmp.begin();          // init bmp180

  oled.clear();
  oled.setCursor(0, 0);

  // Configure tasks
  mainTask.onRun(mainTaskFunction);
  communicationTask.onRun(communicationTaskFunction);
  blinkSeparatorTask.onRun(blinkSeparatorTaskFunction);

  // Configure tasks periods
  mainTask.setInterval(MAIN_TASK_PERIOD_MS);
  communicationTask.setInterval(BLUETOOTH_TASK_PERIOD_MS);
  blinkSeparatorTask.setInterval(BLINK_SEPARATOR_TASK_PERIOD_MS);

#if BUILD_WITH_SD_CARD
  // Add a timestamp to the log file
  SdFile::dateTimeCallback(FAT_dateTime);
#endif

  DateTime now = RTC.now();
  int dd = now.day();
  int hh = now.hour();
  int mm = now.minute();

#if BUILD_WITH_SD_CARD
  sprintf(GV_LogFileName, "wm%02d%02d%02d.txt", dd, hh, mm);
  oled.setCursor(0, 0);
  oled.print(GV_LogFileName);
  delay(3000);
  oled.drawString(0, 0, "             ");
  
  logWeatherMonitor = SD.open(GV_LogFileName, FILE_WRITE);
  if(logWeatherMonitor)
  {
    logWeatherMonitor.print(F("--- Starting new log entry (non-concurrent version) ---\n"));
    // logWeatherMonitor.print(F("Time, Light_sensor_value, Active_time_(sec), Previous_state, Current_state, dynamicIlluminanceThr, FullTask(ms), SDlogTask(ms), 2xLcdTaskTimerVal(ms)\n"));
  }
  else
  {
    GV_LogFileErrorIndicator = true;
    oled.drawString(0, 0, "FILE_ERROR_01");
    delay(2000);
    oled.drawString(0, 0, "             ");

    /* Error indicator */
    oled.drawString(15, 3, "x");
    delay(1000);
  }
  logWeatherMonitor.close();
#endif
  
  /* uncomment to set the clock
   * and disable loop() content
   **/
  // DS3231 Clock;
  // Clock.setHour(18);
  // Clock.setMinute(53);
  // Clock.setSecond(00);
  /******************************/
}

#if BUILD_WITH_SD_CARD
/**
 * Callback function for SD File timestamp
 * (FAT_* macro's are a part of SdFat library, whic is wrapped by SD.h)
 */
void FAT_dateTime(uint16_t* date, uint16_t* time) {
  DateTime now = RTC.now();
  *date = FAT_DATE(now.year(), now.month(), now.day());
  *time = FAT_TIME(now.hour(), now.minute(), now.second());
}
#endif

void blinkClockSeparators()
{
  static bool flashSeparators = true;
  char separator;

  separator = flashSeparators ? ':' : ' ';

  oled.setCursor(2, 0);
  oled.print(separator);

  flashSeparators = !flashSeparators;
}

/**
 * Main loop
 */
void loop()
{
  if (mainTask.shouldRun())
  {
    mainTask.run();
  }

  if (blinkSeparatorTask.shouldRun())
  {
    blinkSeparatorTask.run();
  }

  if (communicationTask.shouldRun())
  {
    communicationTask.run();
  }
}

//---------------------------
// Sketch uses 28172 bytes (98%) of program storage space. Maximum is 28672 bytes.                                                ---> add sd file timestamp
// Global variables use 1804 bytes (70%) of dynamic memory, leaving 756 bytes for local variables. Maximum is 2560 bytes.
// Sketch uses 27854 bytes (97%) of program storage space. Maximum is 28672 bytes.
// Global variables use 1747 bytes (68%) of dynamic memory, leaving 813 bytes for local variables. Maximum is 2560 bytes.
// Sketch uses 27784 bytes (96%) of program storage space. Maximum is 28672 bytes.                                                ---> removed Thread; 70 bytes released
// Global variables use 1726 bytes (67%) of dynamic memory, leaving 834 bytes for local variables. Maximum is 2560 bytes.
// Sketch uses 24900 bytes (86%) of program storage space. Maximum is 28672 bytes.                                                ---> removed Adafruit libs: TempHumid and Bmp; 2884 bytes released
// Global variables use 1648 bytes (64%) of dynamic memory, leaving 912 bytes for local variables. Maximum is 2560 bytes.
// Sketch uses 23674 bytes (82%) of program storage space. Maximum is 28672 bytes.                                                ---> removed RTC; 1126 bytes released
// Global variables use 1646 bytes (64%) of dynamic memory, leaving 914 bytes for local variables. Maximum is 2560 bytes.
// Sketch uses 12928 bytes (45%) of program storage space. Maximum is 28672 bytes.                                                ---> removed SD and SPI; 10746 bytes released
// Global variables use 837 bytes (32%) of dynamic memory, leaving 1723 bytes for local variables. Maximum is 2560 bytes.
// Sketch uses 17328 bytes (60%) of program storage space. Maximum is 28672 bytes.                                                ---> all functionality without SD card. 
// Global variables use 925 bytes (36%) of dynamic memory, leaving 1635 bytes for local variables. Maximum is 2560 bytes.
// Sketch uses 27942 bytes (97%) of program storage space. Maximum is 28672 bytes.                                                ---> SD returned back
// Global variables use 1752 bytes (68%) of dynamic memory, leaving 808 bytes for local variables. Maximum is 2560 bytes.
// Sketch uses 18212 bytes (63%) of program storage space. Maximum is 28672 bytes.                                                ---> no SD, + SoftwareSerial
// Global variables use 1022 bytes (39%) of dynamic memory, leaving 1538 bytes for local variables. Maximum is 2560 bytes.
// Sketch uses 19466 bytes (67%) of program storage space. Maximum is 28672 bytes.                                                ---> add BT communication
// Global variables use 1168 bytes (45%) of dynamic memory, leaving 1392 bytes for local variables. Maximum is 2560 bytes.
// Sketch uses 20034 bytes (69%) of program storage space. Maximum is 28672 bytes.                                                ---> Thread returned back
// Global variables use 1221 bytes (47%) of dynamic memory, leaving 1339 bytes for local variables. Maximum is 2560 bytes.
