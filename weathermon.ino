/*

Version without cuncurrency
All IIC operations are sequential

I2C addresses:

Found address: 60 (0x3C)     - OLED 4-line display
Found address: 60 (0x3C)     - OLED 8-lines
Found address: 87 (0x57)     - RTC 1 (eeprom??? AT24C32)
Found address: 104 (0x68)    - RTC 2 (CLOCK_ADDRESS)
Found address: 119 (0x77)    - pressure sensor
not found AM2320, 92 >>> 0x5C ?

*/

#include <SPI.h>      // for sd card
#include <SD.h>
#include <Wire.h>     // for AM2320
#include "Adafruit_Sensor.h"  // for AM2320, connect to I2C
#include "Adafruit_AM2320.h"
#include "Adafruit_BMP085.h"  // Pressure sensor, I2C (with pullup resistors onboard)
#include "DS3231.h"   // RTC
#include <U8x8lib.h>

#define PAMMHG                        (0.00750062)
#define RTC_READ_TASK_PERIOD_MS       (1000)
#define RTC_DISPLAY_TASK_PERIOD_MS    (2030)
#define SENSOR_READ_TASK_PERIOD_MS    (2095)
#define SENSOR_DISPLAY_TASK_PERIOD_MS (3100)
#define LOG_BUFFER_LEN                (106)
#define SD_LOG_FILE_NAME_LEN          (13)

#define MAIN_TASK_PERIOD_MS             (10000)
#define BLINK_SEPARATOR_TASK_PERIOD_MS  (500)

//AM2320 temp_humid;
Adafruit_AM2320 temp_humid = Adafruit_AM2320();
Adafruit_BMP085 bmp;
RTClib RTC;   // DS3231 Clock;  --> Clock.setClockMode(false);	// set to 24h

U8X8_SSD1306_128X32_UNIVISION_HW_I2C oled(/* reset=*/ U8X8_PIN_NONE, /* clock=*/ SCL, /* data=*/ SDA);   // pin remapping with ESP8266 HW I2C

File logWeatherMonitor;

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

static char     GV_LogFileName[SD_LOG_FILE_NAME_LEN] = "asddhhmm.txt";
static bool     GV_LogFileErrorIndicator = false;

static unsigned long timerOne = 0;
static unsigned long timerTwo = 0;


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
    , "Time: %04d/%02d/%02d %02d:%02d:%02d, Temperature: %2d.%1d C, Humidity: %2d.%1d %%, Pressure: %3d.%2d mmHg, Ambient: %4d\n"
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
}


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


void setup(){
  Wire.begin();
  Serial.begin(115200);
  
  oled.begin();
  oled.setPowerSave(0);
  oled.setFont(u8x8_font_chroma48medium8_r); // u8x8_font_chroma48medium8_r u8x8_font_px437wyse700a_2x2_r u8g2_font_courB12_tf 
  oled.setContrast(11);

  sdCardProgram();

  temp_humid.begin();   // init am2320
  bmp.begin();          // init bmp180

  
  oled.clear();
  oled.setCursor(0, 0);

  // Add a timestamp to the log file
  SdFile::dateTimeCallback(FAT_dateTime);

  DateTime now = RTC.now();
  int dd = now.day();
  int hh = now.hour();
  int mm = now.minute();

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

  timerOne = millis() + MAIN_TASK_PERIOD_MS;
  timerTwo = timerOne;
  
  /* uncomment to set the clock
   * and disable loop() content
   **/
  // DS3231 Clock;
  // Clock.setHour(18);
  // Clock.setMinute(53);
  // Clock.setSecond(00);
  /******************************/
}

/**
 * Callback function for SD File timestamp
 * (FAT_* macro's are a part of SdFat library, whic is wrapped by SD.h)
 */
void FAT_dateTime(uint16_t* date, uint16_t* time) {
  DateTime now = RTC.now();
  *date = FAT_DATE(now.year(), now.month(), now.day());
  *time = FAT_TIME(now.hour(), now.minute(), now.second());
}

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
  unsigned long currentMs = millis();
  static uint32_t stopMeasTimerUs = 0;
  uint32_t startMeasTimerUs = micros();
  
  // call reading RTC/sensors + writing to log/dislpay every 15 seconds
  // typical task execution time ~9 (8-12) us (for IIC devices only)
  if (currentMs - timerOne >= MAIN_TASK_PERIOD_MS)
  {
    clockReadFunc(&currentTimeDate);
    sensorReadFunc(&sensorsData);
    clockDisplayFunc(&currentTimeDate);
    sensorsDataLogFunc(&sensorsData, &currentTimeDate);
    timerOne = currentMs;
    Serial.print("Main task ET, us: ");
    Serial.println(stopMeasTimerUs);
  }
  
  stopMeasTimerUs = micros() - startMeasTimerUs;
  
  // flash hours-minutes separator every 0.5 sec
  if (currentMs - timerTwo >= BLINK_SEPARATOR_TASK_PERIOD_MS)
  {
    blinkClockSeparators();
    timerTwo = currentMs;
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