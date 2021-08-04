#include <SPI.h>
//#include <SD.h>
#include <Wire.h>
#include <Bounce.h>
#include <Encoder.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
//#include <Adafruit_INA219.h>
#include <Adafruit_SSD1351.h>
#include <Time.h>
#include "cm_bitmaps.h"
#include "Adafruit_FONA.h"

const uint16_t PB_AMBER = RGB888toRGB565("FFB642");
//const uint16_t PB_DARKAMBER = RGB888toRGB565("805B21");
const uint16_t PB_GREEN = RGB888toRGB565("1AFF80");
//const uint16_t PB_DARKGREEN = RGB888toRGB565("0D7F40");
const uint16_t PB_BLUE = RGB888toRGB565("2ECFFF");
//const uint16_t PB_DARKBLUE = RGB888toRGB565("176780");
const uint16_t PB_WHITE = RGB888toRGB565("C5FFFF");
//const uint16_t PB_DARKWHITE = RGB888toRGB565("627F7F");
uint16_t curUIColor = PB_BLUE;
//uint16_t curUIDarkColor = PB_DARKAMBER;

//definitions are in cm_bitmaps.h

HardwareSerial *fonaSerial = &Serial1;

Adafruit_FONA fona = Adafruit_FONA(PIN_FONA_RST);
Adafruit_SSD1351 screen = Adafruit_SSD1351(PIN_OLED_CS, PIN_OLED_DC, PIN_OLED_RST);
//Adafruit_INA219 currentSensor;
Encoder myEnc(PIN_ENCODER_A, PIN_ENCODER_B);
Bounce encoderButton = Bounce(PIN_ENCODER_BUTTON, 10);
Bounce redButton = Bounce(PIN_BUTTON_RED, 10);
Bounce blueButton = Bounce(PIN_BUTTON_BLUE, 10);

uint8_t readline(char *buff, uint8_t maxbuff, uint16_t timeout = 0);

String UIcolorNameString = "BLUE GREENAMBERWHITE";
String UIcolorString = "ZZZZZ";
String volumeString = "000%";
String dowNameString = "SUNMONTUEWEDTHUFRISAT";
String dowString = "ZZZ";
String monthNameString = "JANFEBMARAPRMAYJUNJULAUGSEPOCTNOVDEC";
String hourString = "00";
String minuteString = "00";
String dateString = "00";
String monthString = "ZZZ";
String yearString = "2015";
String tempVolumeString;
String tempHourString;
String tempMinuteString;
String tempDateString;
String tempMonthString;
char fonaReplyBuffer[255];
char incomingPhoneNumber[32] = {0};
char outgoingPhoneNumber[32] = {0};
String outgoingPhoneNumberString;

int curHour = 7;
int curMinute = 11;
int curDow = 0;
int curDate = 2;
int curMonth = 8;
int curYear = 2015;
int lastMinute = 0;

int monthLengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

int scrollPos = 0;
int lastScrollPos = 0;
int scrollMax = 2; // min is always 0
int lastRotPos  = 0;
int charHeight = 8;
int charWidth = 6;
int phoneCharCount = 0;

boolean encoderButtonPressed = false;
boolean redButtonPressed = false;
boolean blueButtonPressed = false;
boolean timeIs24Hr = true;
boolean timeIs12Hr = false;
boolean vibrateOnCall = true;
boolean ringOnCall = true;
boolean readyToCall = false;
boolean phoneIsRinging = false;
boolean outgoingCallConnected = false; // there are two unique statuses for calls, urgh
boolean phoneOnCall = false;
boolean motorState = false;

int curWatchDisplayMode = 0;
int curWatchUIColorMode = 0;
int curMode = 0;

float batteryVoltage = 0.0; // V
uint16_t batteryPercentage = 0.0; // %
int batteryPowerSymbol = 1;
int networkStrengthSymbol = 1;
uint8_t curVolume = 99;
uint8_t ringTone = 8; // upbeat mysterious tone
//float currentDraw = 0.0; // mA

unsigned long curRunTime = 0;
unsigned long lastRunTime = 0;
unsigned long lastRunData = 0;
unsigned long lastVibeTime = 0;
unsigned long vibeInterval = 700;
unsigned long updateIntervalTime = 1000; // get RTC data once every second
unsigned long updateIntervalData = 60000; // get power and network data once every minute
  // ****************************************************************[ END DECLARATION ]****************************************************************

void setup() { 

  pinMode(PIN_ENCODER_BUTTON, INPUT_PULLUP);
  pinMode(PIN_BUTTON_RED, INPUT_PULLUP);
  pinMode(PIN_BUTTON_BLUE, INPUT_PULLUP);
  pinMode(PIN_FONA_RING, INPUT_PULLUP); // although this isn't configured in the example FONA sketches, it works reliably this way
  //SD.begin(PIN_SD_CS); // SD is unused
  //currentSensor.begin(); // current sensor has been removed
  screen.begin(); // initialize the OLED display
  Serial.begin(9600);
  systemConfig(SYS_READ); // get the info from eeprom about UI color, volume, and call settings etc.
  drawSplashScreen(); // run before FONA setup since it takes a few seconds
  setSyncProvider(getTeensy3Time); // set the time provider to the internal RTC time
  setSyncInterval(3600); // update the provider every hour
  fonaSerial->begin(4800);
  fona.begin(*fonaSerial); // may need to add if statement check
  fona.setAudio(FONA_EXTAUDIO);
  fona.setMicVolume(FONA_EXTAUDIO, 10);
  setRingerSettings(); // this has to be called outside of config since the fona connection hasn't begun...
  fona.setVolume(curVolume);
  fona.playToolkitTone(6, 500);
  fona.callerIdNotification(true, PIN_FONA_RING); // enable incoming call ID
  fona.setRingTone(ringTone);
  screen.fillScreen(BLACK);
  getTimeInfo();
  getPowerInfo();
  drawLockScreen();
  getNetworkInfo();
  scrollPos = 0;
  lastMinute = curMinute; // avoid an immediate redraw at boot
  
} // ****************************************************************[ END SETUP ]****************************************************************

void loop() {

  getInput(); // buttons and encoder position and incoming call notifications
  curRunTime = millis();
  if (curRunTime - lastRunTime > updateIntervalTime){
    getTimeInfo(); // gets RTC time
    lastRunTime = curRunTime;
    checkForIncomingCall();
    getPhoneCallStatus();
  }
  if (curRunTime - lastRunData > updateIntervalData){ // these fona requests were slowing down the UI if polled too often
    getPowerInfo(); // only do these periodically, formats the battery bar
    getNetworkInfo(); // formats the signal indicator
    lastRunData = curRunTime;
  } 
  switch (curMode){
// ****************************************************************[ LOCK SCREEN
   case MODE_DISPLAY_LOCK: // displays the "lock" screen which shows the dow, time, and date (can be exited by pressing both the red and blue buttons
    if (curMinute != lastMinute){
      screen.fillScreen(BLACK);
      drawLockScreen();
      lastMinute = curMinute;
    }
    if (encoderButtonPressed){ // "confirm" exit condition     
      curMode = MODE_DISPLAY_SELECT_MODE;
      screen.fillScreen(BLACK);
      drawDisplayMenu();
      scrollPos = 0;
      drawWatchModeDescription(scrollPos);
      drawWatchModeBounding(scrollPos, curUIColor);    
    }
   break;
// ****************************************************************[ SELECT MODE
   case MODE_DISPLAY_SELECT_MODE: // allows cycling through the watch modes, pressing blue returns to the lock screen, pressing red sets pos to 0
    scrollMax = 2;
    checkRedBackToZeroPos();
    if (curMinute != lastMinute){
      screen.fillRect(31, 1, 60, 16, BLACK); // erase menu time
      drawDisplayMenuTime();
      screen.setTextSize(1);
      lastMinute = curMinute;
    } // really, this should dynamically happen for battery power and signal strength, but it can slide for now, this info bar should update out side of just this mode too
    if (scrollPos != lastScrollPos){ // sometimes has errors where the last position is in the opposite direction
      drawWatchModeBounding(lastScrollPos, BLACK); // so the last watch mode should be erased instead of the last scroll pos, which may be ineffective
      screen.fillRect(0, 44, screen.width(), charHeight, BLACK);
      lastScrollPos = scrollPos;
      drawWatchModeBounding(scrollPos, curUIColor); 
      drawWatchModeDescription(scrollPos);       
    }  
    if (blueButtonPressed){ 
      screen.fillScreen(BLACK);
      drawLockScreen();
      curMode = MODE_DISPLAY_LOCK;
    }
    if (encoderButtonPressed){
        drawWatchModeBounding(scrollPos, BLACK);
        clearActiveArea();
        screen.drawRoundRect(16 + (36 * scrollPos), 85, 23, 11, 3, curUIColor); // x, y, w, h, radius, color around the watchMode
        switch(scrollPos){
          case 0:
            curMode = MODE_SELECT_TIME_SETTING;
            curWatchDisplayMode = 0;
            drawTimeMenu();
            scrollPos = 0;
            drawTimeSettingBounding(scrollPos, curUIColor);
          break;
          case 1:
            curMode = MODE_COM_DIAL;
            curWatchDisplayMode = 1;
            drawComMenu();
            scrollPos = 0;
            drawComSettingBounding(scrollPos, curUIColor);
          break;
          case 2:
            curMode = MODE_SELECT_SYS_SETTING;
            curWatchDisplayMode = 2;
            drawSystemMenu();
            scrollPos = 0;
            drawSystemSettingBounding(scrollPos, curUIColor);
          break;
        }   
      }  
   break;
/**************************************************************************[ CLOCK SELECTION MODE ]**************************************************************************/   
   case MODE_SELECT_TIME_SETTING: // (watchMode 0) allows cycling through the various time settings, pressing blue returns to display select mode, red returns to top of the menu
    scrollMax = 5;
    checkRedBackToZeroPos();
    if (scrollPos != lastScrollPos){
      drawTimeSettingBounding(lastScrollPos, BLACK);
      lastScrollPos = scrollPos;
      drawTimeSettingBounding(scrollPos, curUIColor);        
    }
    checkBlueBackToModeSelection(); // edit to save time settings instead of this default function
    if (encoderButtonPressed){ // erase time selection, go to highlight the scroll pos, enter the scroll pos time mode
      drawTimeSettingBounding(lastScrollPos, BLACK);
      switch (scrollPos){
          case 0: // time mode
            drawTimeModeSetting(BLACK); // erase time mode selection
            drawTimeModeBounding(scrollPos, curUIColor);
            curMode =  MODE_SET_TIME_MODE;
            scrollMax = 1;
          break;
          case 1: // hour
            screen.setTextColor(curUIColor);
            screen.setCursor(60, 36);
            screen.print("<");
            screen.setCursor(77, 36);
            screen.print(">");
            
            curMode = MODE_SET_TIME_HOUR;
            if (timeIs12Hr){ 
              scrollMax = 11;
              scrollPos = curHour - 1;
            }
            if (timeIs24Hr){ 
              scrollMax = 23;
              scrollPos = curHour;
            }
          break;
          case 2: // minute
            screen.setTextColor(curUIColor);
            screen.setCursor(60, 44);
            screen.print("<");
            screen.setCursor(77, 44);
            screen.print(">");
            scrollPos = curMinute;
            curMode = MODE_SET_TIME_MINUTE;
            scrollMax = 59;
          break;
          case 3: // month
            screen.setTextColor(curUIColor);
            screen.setCursor(60, 52);
            screen.print("<");
            screen.setCursor(83, 52);
            screen.print(">");
            scrollPos = curMonth - 1;
            curMode = MODE_SET_TIME_MONTH;
            scrollMax = 11;
          break;
          case 4: // day
            screen.setTextColor(curUIColor);
            screen.setCursor(60, 60);
            screen.print("<");
            screen.setCursor(77, 60);
            screen.print(">");
            scrollPos = curDate;
            lastScrollPos = scrollPos;
            curMode = MODE_SET_TIME_DAY;
            scrollMax = monthLengths[curMonth - 1] - 1; // RTC days are 1-31, but the array is 0 - 31 
          break;
          case 5: // year
            //draw year bounding manual
            screen.setTextColor(curUIColor);
            screen.setCursor(60, 68);
            screen.print("<");
            screen.setCursor(89, 68);
            screen.print(">");
            scrollPos = curYear;
            curMode = MODE_SET_TIME_YEAR;
            scrollMax = 2100;
          break;
      } 
    }
   break;
//**************************************************************************[ TIME MODE
   case MODE_SET_TIME_MODE: // allows switching between 12 and 24 hour time, blue back to sel time setting, red to top of menu
    checkRedBackToZeroPos();
    if (scrollPos != lastScrollPos){
      drawTimeModeBounding(lastScrollPos, BLACK);
      lastScrollPos = scrollPos;
      drawTimeModeBounding(scrollPos, curUIColor);
    }  
    if (blueButtonPressed){
      drawTimeModeBounding(lastScrollPos, BLACK);
      drawTimeModeSetting(curUIColor);
      scrollPos = 0;
      drawTimeSettingBounding(scrollPos, curUIColor);
      curMode = MODE_SELECT_TIME_SETTING;
      scrollMax = 5;
    }
    if (encoderButtonPressed){
      drawTimeModeBounding(lastScrollPos, BLACK);
      if (scrollPos == 0){
        timeIs12Hr = true;
        timeIs24Hr = false;
      }
      if (scrollPos == 1){
        timeIs12Hr = false;
        timeIs24Hr = true;
      } 
      drawTimeModeSetting(curUIColor);
      systemConfig(SYS_WRITE);//save new time mode setting in EEPROM
      screen.fillRect(31, 1, 60, 16, BLACK); // erase menu time
      getTimeInfo();
      drawDisplayMenuTime();
      scrollPos = 0;
      drawTimeSettingBounding(scrollPos, curUIColor);
      curMode = MODE_SELECT_TIME_SETTING;
    }
   break;
//**************************************************************************[ SET HOUR
   case MODE_SET_TIME_HOUR: // allows setting the time (1-12 or 0-24), blue back to time settings, red sets hour to minimum
    checkRedBackToZeroPos();
    if (timeIs12Hr) {
      scrollMax = 11;
      if (scrollPos != lastScrollPos){        
        screen.fillRect(66, 36, 12, 8, BLACK);// erase old hour
        screen.setCursor(66, 36);
        if ((scrollPos + 1) < 10) screen.print(" "); // draw formatted new temp hour
        screen.print(String(scrollPos + 1)); // no zero hours in 12 hour time!  
        lastScrollPos = scrollPos;
      }
    }
    if (timeIs24Hr) {
      scrollMax = 23;
      if (scrollPos != lastScrollPos){        
        screen.fillRect(66, 36, 12, 8, BLACK);
        screen.setCursor(66, 36);
        if (scrollPos < 10) screen.print(" "); 
        screen.print(String(scrollPos)); 
        lastScrollPos = scrollPos;
      }
    }  
    if (blueButtonPressed){
      screen.setTextColor(BLACK);
      screen.setCursor(60, 36);
      screen.print("<");
      screen.setCursor(77, 36);
      screen.print(">");
      screen.fillRect(66, 36, 12, 8, BLACK);
      drawTimeMenu(); // redraw the actual hour
      scrollPos = 1;
      drawTimeSettingBounding(scrollPos, curUIColor);
      curMode = MODE_SELECT_TIME_SETTING;
      scrollMax = 5;
    }
    if (encoderButtonPressed){
      screen.setTextColor(BLACK);
      screen.setCursor(60, 36);
      screen.print("<");
      screen.setCursor(77, 36);
      screen.print(">");
      if (timeIs12Hr && hour() > 12) scrollPos += 12; // the RTC always stores hours as 24hr time
      if (timeIs12Hr) setTime(scrollPos + 1, curMinute, 0, curDate, curMonth, curYear); // save new hour in RTC
      if (timeIs24Hr) setTime(scrollPos, curMinute, 0, curDate, curMonth, curYear); 
      Teensy3Clock.set(now()); //actually write to the internal RTC
      screen.fillRect(31, 1, 60, 16, BLACK); //erase menu time
      getTimeInfo();
      drawTimeMenu();
      drawDisplayMenuTime();        
      scrollPos = 1;
      drawTimeSettingBounding(scrollPos, curUIColor);
      curMode = MODE_SELECT_TIME_SETTING;
      scrollMax = 5;
    }
   break;
//**************************************************************************[ SET MINUTE
   case MODE_SET_TIME_MINUTE: // allows setting the minute (0-59), blue back to time settings, red sets minute to 0  
    checkRedBackToZeroPos();
    if (scrollPos != lastScrollPos){      
      screen.fillRect(66, 44, 12, 8, BLACK);//erase old minute
      screen.setCursor(66, 44);
      if (scrollPos < 10) screen.print("0"); 
      screen.print(String(scrollPos)); 
      lastScrollPos = scrollPos;
    }
    if (blueButtonPressed){
      screen.setTextColor(BLACK);
      screen.setCursor(60, 44);
      screen.print("<");
      screen.setCursor(77, 44);
      screen.print(">");
      screen.fillRect(66, 44, 12, 8, BLACK);
      drawTimeMenu(); // redraw the actual minute
      scrollPos = 2;
      drawTimeSettingBounding(scrollPos, curUIColor);
      curMode = MODE_SELECT_TIME_SETTING;
      scrollMax = 5;
    }
    if (encoderButtonPressed){
      screen.setTextColor(BLACK);
      screen.setCursor(60, 44);
      screen.print("<");
      screen.setCursor(77, 44);
      screen.print(">");
      screen.fillRect(66, 44, 12, 8, BLACK);
      setTime(curHour, scrollPos, 0, curDate, curMonth, curYear); // save new hour in RTC
      Teensy3Clock.set(now()); //actually write to the internal RTC
      screen.fillRect(31, 1, 60, 16, BLACK); //erase menu time
      getTimeInfo();
      drawTimeMenu();
      drawDisplayMenuTime();
      scrollPos = 2;
      drawTimeSettingBounding(scrollPos, curUIColor);
      curMode = MODE_SELECT_TIME_SETTING;
      scrollMax = 5;
    }
   break; 
// ****************************************************************[ SET MONTH
   case MODE_SET_TIME_MONTH: // allows setting the month (1-12), blue back to time settings, red sets month to January
    checkRedBackToZeroPos();
     if (scrollPos != lastScrollPos){
      screen.fillRect(66, 52, 18, 8, BLACK); // erase old month
      monthString.setCharAt(0, monthNameString.charAt((scrollPos) * 3));
      monthString.setCharAt(1, monthNameString.charAt((scrollPos) * 3 + 1));
      monthString.setCharAt(2, monthNameString.charAt((scrollPos) * 3 + 2));
      screen.setCursor(66, 52);
      screen.print(monthString);
      lastScrollPos = scrollPos;
    }
    if (blueButtonPressed){
      screen.setTextColor(BLACK);
      screen.setCursor(60, 52);
      screen.print("<");
      screen.setCursor(83, 52);
      screen.print(">");
      screen.fillRect(66, 52, 18, 8, BLACK);
      drawTimeMenu(); // redraw the actual month
      scrollPos = 3;
      drawTimeSettingBounding(scrollPos, curUIColor);
      curMode = MODE_SELECT_TIME_SETTING;
      scrollMax = 5;
    }
    if (encoderButtonPressed){
      screen.setTextColor(BLACK); // erase month bounding
      screen.setCursor(60, 52);
      screen.print("<");
      screen.setCursor(83, 52);
      screen.print(">");
      screen.fillRect(66, 52, 18, 8, BLACK);
      setTime(curHour, curMinute, 0, curDate, scrollPos+1, curYear); // save new month in RTC
      Teensy3Clock.set(now());
      getTimeInfo();
      drawTimeMenu();
      scrollPos = 3;
      drawTimeSettingBounding(scrollPos, curUIColor);
      curMode = MODE_SELECT_TIME_SETTING;
      scrollMax = 5;
    }
   break;
// ****************************************************************[ SET DAY
   case MODE_SET_TIME_DAY: // allows setting the day of the month (0 - 28, 30, or 31 depending on the month), blue back to time settings, red sets day to 1st
    checkRedBackToZeroPos();
    if (scrollPos != lastScrollPos){     
      screen.fillRect(66, 60, 12, 8, BLACK);//erase old day      
      screen.setCursor(66, 60);
      if (scrollPos + 1 < 10) screen.print("0"); //draw new day
      screen.print(String(scrollPos + 1)); // no zeroeth dates
      lastScrollPos = scrollPos;
    }
    if (blueButtonPressed){
      screen.setTextColor(BLACK); // erase day bounding
      screen.setCursor(60, 60);
      screen.print("<");
      screen.setCursor(77, 60);
      screen.print(">");
      screen.fillRect(66, 60, 12, 8, BLACK);
      drawTimeMenu(); // redraw the actual day
      scrollPos = 4;
      drawTimeSettingBounding(scrollPos, curUIColor);
      curMode = MODE_SELECT_TIME_SETTING;
      scrollMax = 5;
    }
    if (encoderButtonPressed){  
      screen.setTextColor(BLACK);
      screen.setCursor(60, 60);
      screen.print("<");
      screen.setCursor(77, 60);
      screen.print(">");
      setTime(curHour, curMinute, 0, scrollPos + 1, curMonth, curYear); // save new day in RTC
      Teensy3Clock.set(now());
      getTimeInfo();
      drawTimeMenu();
      scrollPos = 4;
      drawTimeSettingBounding(scrollPos, curUIColor);
      curMode = MODE_SELECT_TIME_SETTING;
      scrollMax = 5;
    }
   break;
// ****************************************************************[ SET YEAR
   case MODE_SET_TIME_YEAR: // allows setting the year (0-9999), blue back to time settings without entering, red sets year to 0000
    if (redButtonPressed){
      scrollPos = 2000; // default min year
    }
    if (scrollPos != lastScrollPos){      
      screen.fillRect(66, 68, 24, 8, BLACK);//erase old year
      screen.setCursor(66, 68);
      screen.print(scrollPos);
      lastScrollPos = scrollPos;
    }
    if (blueButtonPressed){
      screen.setTextColor(BLACK); // erase day bounding
      screen.setCursor(60, 68);
      screen.print("<");
      screen.setCursor(89, 68);
      screen.print(">");
      screen.fillRect(66, 68, 24, 8, BLACK);
      drawTimeMenu(); // redraw the actual year
      scrollPos = 5;
      drawTimeSettingBounding(scrollPos, curUIColor);
      curMode = MODE_SELECT_TIME_SETTING;
      scrollMax = 5;
    }
    if (encoderButtonPressed){     
      screen.setTextColor(BLACK); //erase year bounding
      screen.setCursor(60, 68);
      screen.print("<");
      screen.setCursor(89, 68);
      screen.print(">");
      setTime(curHour, curMinute, 0, curDate, curMonth, scrollPos); // save new day in RTC
      Teensy3Clock.set(now());
      getTimeInfo();
      drawTimeMenu();
      scrollPos = 5;
      drawTimeSettingBounding(scrollPos, curUIColor);
      curMode = MODE_SELECT_TIME_SETTING;
      scrollMax = 5;
    }
   break;
/**************************************************************************[ COMMUNICATOR MODE ]**************************************************************************/ 
   case MODE_COM_DIAL: // allows dialing via scroll, blue back to display select mode, red clears a character 
    scrollMax = 13; // 0-9 * # DIAL DELETE
    checkRedBackToZeroPos();
    if (scrollPos != lastScrollPos){
      drawComSettingBounding(lastScrollPos, BLACK);
      lastScrollPos = scrollPos;
      drawComSettingBounding(scrollPos, curUIColor);        
    }
    if (encoderButtonPressed){
      if (scrollPos < 12) phoneCharCount++;
      //Serial.print("phoneCharCount: "); Serial.print(phoneCharCount);
      screen.setCursor(19 + (charWidth * phoneCharCount), 30);
      switch (scrollPos){
        case 0:             
          outgoingPhoneNumberString += 0;
          screen.print(outgoingPhoneNumberString.charAt(phoneCharCount - 1)); // print before fona command for immediate visual feedback, string is 0 indexed
          //Serial.print(" You pressed: "); Serial.print('0'); Serial.print(" String is: "); Serial.println(outgoingPhoneNumberString);
          fona.playDTMF('0');
        break;
        case 1:
          outgoingPhoneNumberString += 1;
          screen.print(outgoingPhoneNumberString.charAt(phoneCharCount - 1));
          //Serial.print(" You pressed: "); Serial.print('1'); Serial.print(" String is: "); Serial.println(outgoingPhoneNumberString);
          fona.playDTMF('1');
        break;
        case 2:
          outgoingPhoneNumberString += 2;
          screen.print(outgoingPhoneNumberString.charAt(phoneCharCount - 1));
          //Serial.print(" You pressed: "); Serial.print('2'); Serial.print(" String is: "); Serial.println(outgoingPhoneNumberString);
          fona.playDTMF('2');
        break;
        case 3:
          outgoingPhoneNumberString += 3;
          screen.print(outgoingPhoneNumberString.charAt(phoneCharCount - 1));
          //Serial.print(" You pressed: "); Serial.print('3'); Serial.print(" String is: "); Serial.println(outgoingPhoneNumberString);
          fona.playDTMF('3');
        break;
        case 4:
          outgoingPhoneNumberString += 4;
          screen.print(outgoingPhoneNumberString.charAt(phoneCharCount - 1));
          //Serial.print(" You pressed: "); Serial.print('4'); Serial.print(" String is: "); Serial.println(outgoingPhoneNumberString);
          fona.playDTMF('4');
        break;
        case 5:
          outgoingPhoneNumberString += 5;
          screen.print(outgoingPhoneNumberString.charAt(phoneCharCount - 1));
          //Serial.print(" You pressed: "); Serial.print('5'); Serial.print(" String is: "); Serial.println(outgoingPhoneNumberString);
          fona.playDTMF('5');
        break;
        case 6:
          outgoingPhoneNumberString += 6;
          screen.print(outgoingPhoneNumberString.charAt(phoneCharCount - 1));
          //Serial.print(" You pressed: "); Serial.print('6'); Serial.print(" String is: "); Serial.println(outgoingPhoneNumberString);
          fona.playDTMF('6');
        break;
        case 7:
          outgoingPhoneNumberString += 7;
          screen.print(outgoingPhoneNumberString.charAt(phoneCharCount - 1));
          //Serial.print(" You pressed: "); Serial.print('7'); Serial.print(" String is: "); Serial.println(outgoingPhoneNumberString);
          fona.playDTMF('7');
        break;
        case 8:
          outgoingPhoneNumberString += 8;
          screen.print(outgoingPhoneNumberString.charAt(phoneCharCount - 1));
          //Serial.print(" You pressed: "); Serial.print('8'); Serial.print(" String is: "); Serial.println(outgoingPhoneNumberString);
          fona.playDTMF('8');
        break;
        case 9:
          outgoingPhoneNumberString += 9;
          screen.print(outgoingPhoneNumberString.charAt(phoneCharCount - 1));
          //Serial.print(" You pressed: "); Serial.print('9'); Serial.print(" String is: "); Serial.println(outgoingPhoneNumberString);
          fona.playDTMF('9');
        break;
        case 10: // #
          outgoingPhoneNumberString += '#';
          screen.print(outgoingPhoneNumberString.charAt(phoneCharCount - 1));
          //Serial.print(" You pressed: "); Serial.print('#'); Serial.print(" String is: "); Serial.println(outgoingPhoneNumberString);
          fona.playDTMF('#');
        break;
        case 11: // *
          outgoingPhoneNumberString += '*';
          screen.print(outgoingPhoneNumberString.charAt(phoneCharCount - 1));
          //Serial.print(" You pressed: "); Serial.print('*'); Serial.print(" String is: "); Serial.print(outgoingPhoneNumberString); 
          fona.playDTMF('*');
        break;
        case  12: // DIAL
          if (outgoingPhoneNumberString.length() > 2){
            outgoingPhoneNumberString.toCharArray(outgoingPhoneNumber, 32);            
            screen.fillScreen(BLACK);
            screen.setCursor(40, 30);
            screen.print("Calling:");
            screen.setCursor((128 - (outgoingPhoneNumberString.length() * charWidth)) >> 1, 46); // print the number centered
            screen.print(outgoingPhoneNumber); //Serial.print("Calling:");Serial.print(outgoingPhoneNumber);           
            fona.callPhone(outgoingPhoneNumber);
            phoneCharCount = 0;
            outgoingPhoneNumberString.remove(0);
            //fona.enableOutgoingCallStatus();
            //getPhoneCallStatus();
            curMode = MODE_COM_OUTGOING;
          }
        break;
        case  13: // DELETE
          if (outgoingPhoneNumberString.length() > 0){
            outgoingPhoneNumberString.remove(phoneCharCount - 1);
            phoneCharCount--;
            //Serial.print(" You pressed: "); Serial.print("DELETE"); Serial.print(" String is: "); Serial.print(outgoingPhoneNumberString); Serial.print(" phoneCharCount: "); Serial.println(phoneCharCount);
            screen.setCursor(25 + (charWidth * phoneCharCount), 30);
            screen.setTextColor(curUIColor, BLACK);
            screen.print(' '); //delete screen char           
          }
        break;
      } // END SWITCHCASE
    }   
    if (blueButtonPressed){
      screen.fillRect(19, 30, (charWidth * phoneCharCount), 8, BLACK); //clear number
      phoneCharCount = 0;
      outgoingPhoneNumberString.remove(0); // remove all chars to the end of the string
      clearActiveArea();//erase active area
      scrollPos = 1;
      drawWatchModeDescription(scrollPos);
      screen.drawRoundRect(16 + 36, 85, 23, 11, 3, BLACK);
      drawWatchModeBounding(scrollPos, curUIColor); 
      curMode = MODE_DISPLAY_SELECT_MODE;
    }
   break;
// ****************************************************************[ INCOMING CALL
   case MODE_COM_INCOMING: // shows the incoming phone number, blue does nothing, red cancels the call
     checkRedToHangUp();
     if (vibrateOnCall){
       if (curRunTime - lastVibeTime > vibeInterval){
         motorState = !motorState; // just switch back and forth
         if (motorState){
           fona.setPWM(2000);
         } else {
           fona.setPWM(0);
         }
         lastVibeTime = curRunTime; 
       }            
     }
     if (encoderButtonPressed){
       fona.setPWM(0); // turn off the motor if it was left on
       fona.pickUp();
       getPhoneCallStatus();
       screen.fillScreen(BLACK);
       screen.setTextSize(1);
       screen.setTextColor(curUIColor);
       screen.setCursor(25, 36);
       screen.print("ON CALL WITH:");
       screen.setCursor(34, 52);
       screen.print(incomingPhoneNumber);
       screen.setCursor(34, 88);
       scrollPos = curVolume;
       screen.print("VOLUME:");drawVolumeLevel(curVolume, curUIColor);
       scrollMax = 100; // volume limit 
       curMode = MODE_COM_ACTIVE;
     }
   break;
// ****************************************************************[ OUTGOING CALL
   case MODE_COM_OUTGOING: // shows the outgoing call, blue does nothing, red cancels the calls
      checkRedToHangUp();
      if (encoderButtonPressed){ // there is a specific command for testing the status of outgoing calls, but it isn't replying at all for now, more to do!
        clearActiveArea(); // for now we'll have to manually confirm a connected call, bleh
        screen.setTextSize(1);
        screen.setTextColor(curUIColor);
        screen.setCursor(25, 36);
        screen.print("ON CALL WITH:");
        screen.setCursor((128 - (outgoingPhoneNumberString.length() * charWidth)) >> 1, 52);
        screen.print(outgoingPhoneNumber);
        screen.setCursor(34, 88);
        scrollPos = curVolume;
        screen.print("VOLUME:");drawVolumeLevel(curVolume, curUIColor);
        scrollMax = 100; // volume limit 
        curMode = MODE_COM_ACTIVE;
      }
   break;
// ****************************************************************[ CALL ACTIVE
   case MODE_COM_ACTIVE: // shows the phone number, length of call, and allows setting the volume, blue does nothing, red cancels the call   
     if (scrollPos != lastScrollPos){
       screen.fillRect(82, 88, 24, 8, BLACK); // erase old vol number
       screen.setCursor(82, 88);
       drawVolumeLevel(scrollPos, curUIColor); 
       fona.setVolume(curVolume);           
       lastScrollPos = scrollPos;
     }
     if (phoneOnCall == false){
       redButtonPressed = true; // trigger a hang up if the other party cancels
     }
     checkRedToHangUp();    
   break;
/**************************************************************************[ SYSTEM MODE ]**************************************************************************/    
   case MODE_SELECT_SYS_SETTING: // allows cycling through the system settings, blue back to select mode, red returns to top of menu
     scrollMax = 3;
     checkRedBackToZeroPos();
     if (scrollPos != lastScrollPos){
       drawSystemSettingBounding(lastScrollPos, BLACK);
       lastScrollPos = scrollPos;
       drawSystemSettingBounding(scrollPos, curUIColor);        
     }
     checkBlueBackToModeSelection();
     if (encoderButtonPressed){
      drawSystemSettingBounding(scrollPos, BLACK);
      switch (scrollPos){
        case 0: // vibrate
          drawVibeSetting(vibrateOnCall, BLACK);//erase notification setting
          drawVibeSettingBounding(0, curUIColor);
          curMode = MODE_SET_SYS_VIBE;
          scrollPos = 0;
          scrollMax = 1;
        break; 
        case 1: // chime
          drawToneSetting(ringOnCall, BLACK);//erase notification setting
          drawToneSettingBounding(0, curUIColor);
          curMode = MODE_SET_SYS_CHIME;
          scrollPos = 0;
          scrollMax = 1;
        break;
        case 2: // volume
          curMode = MODE_SET_SYS_VOLUME;
          drawVolumeBounding(curUIColor);
          scrollPos = curVolume;
          scrollMax = 100;
        break;
        case 3: // UI color
          curMode = MODE_SET_SYS_UI_COLOR;
          drawUIColorBounding(curUIColor);
          scrollPos = curWatchUIColorMode;
          scrollMax = 3;
        break;
      } 
     }  
   break;
// ****************************************************************[ SET VIBRATION
   case MODE_SET_SYS_VIBE: // allows setting the watch to pulse or not during incoming calls, blue back to sys settings, red sets returns to pos 0     
     checkRedBackToZeroPos();
     if (scrollPos != lastScrollPos){
       drawVibeSettingBounding(lastScrollPos, BLACK);
       lastScrollPos = scrollPos;
       drawVibeSettingBounding(scrollPos, curUIColor);        
     }
     if (blueButtonPressed){
       drawVibeSettingBounding(lastScrollPos, BLACK); // erase bounding
       drawVibeSetting(vibrateOnCall, curUIColor);
       scrollPos = 0;
       drawSystemSettingBounding(scrollPos, curUIColor);
       curMode = MODE_SELECT_SYS_SETTING;
     }
     if (encoderButtonPressed){
       if (scrollPos == 0)vibrateOnCall = true;
       if (scrollPos == 1)vibrateOnCall = false;
       systemConfig(SYS_WRITE); //save new vibration setting to eeprom 
       drawVibeSettingBounding(scrollPos, BLACK); // erase bounding
       drawVibeSetting(vibrateOnCall, curUIColor);
       scrollPos = 0;
       drawSystemSettingBounding(scrollPos, curUIColor);
       curMode = MODE_SELECT_SYS_SETTING;
     }
   break;
// ****************************************************************[ SET CHIME
   case MODE_SET_SYS_CHIME: // allows setting if the watch makes an audible tone during incoming calls, blue back to sys settings, red returns pos 0
     checkRedBackToZeroPos();
     if (scrollPos != lastScrollPos){
       drawToneSettingBounding(lastScrollPos, BLACK);
       lastScrollPos = scrollPos;
       drawToneSettingBounding(scrollPos, curUIColor);        
     }
     if (blueButtonPressed){
       drawToneSettingBounding(lastScrollPos, BLACK);
       drawToneSetting(ringOnCall, curUIColor);
       scrollPos = 1;
       drawSystemSettingBounding(scrollPos, curUIColor);
       curMode = MODE_SELECT_SYS_SETTING;
     }
     if (encoderButtonPressed){
       if (scrollPos == 0)ringOnCall = true; 
       if (scrollPos == 1)ringOnCall = false;
       setRingerSettings();
       systemConfig(SYS_WRITE); //save new tone setting to eeprom 
       drawToneSettingBounding(scrollPos, BLACK); // erase bounding
       drawToneSetting(ringOnCall, curUIColor);
       scrollPos = 1;
       drawSystemSettingBounding(scrollPos, curUIColor);
       curMode = MODE_SELECT_SYS_SETTING;
     }
   break;
// ****************************************************************[ SET VOLUME
   case MODE_SET_SYS_VOLUME: // allows setting the system volume via scroll, blue back to sys settings, red sets volume to 0
     checkRedBackToZeroPos();
     if (scrollPos != lastScrollPos){
      screen.fillRect(78, 64, 24, 8, BLACK); // erase old volume
      screen.setCursor(78, 64);
      drawVolumeLevel(scrollPos, curUIColor);
      lastScrollPos = scrollPos;        
     }
     if (blueButtonPressed){
       drawVolumeBounding(BLACK);
       screen.fillRect(78, 64, 24, 8, BLACK);
       screen.setCursor(78, 64);
       drawVolumeLevel(curVolume, curUIColor); // draw original
       scrollPos = 2;
       drawSystemSettingBounding(scrollPos, curUIColor);
       curMode = MODE_SELECT_SYS_SETTING;
     }
     if (encoderButtonPressed){
       drawVolumeBounding(BLACK);
       curVolume = scrollPos;
       fona.setVolume(curVolume);
       systemConfig(SYS_WRITE); // save new volume setting to eeprom 
       scrollPos = 2;
       drawSystemSettingBounding(scrollPos, curUIColor);
       curMode = MODE_SELECT_SYS_SETTING;
     }
   break;
// ****************************************************************[ SET UI COLOR
   case MODE_SET_SYS_UI_COLOR: // allows setting what the default UI color is, blue back to sys settings, red sets chime to tone 0
     checkRedBackToZeroPos();
     if (scrollPos != lastScrollPos){
      screen.fillRect(78, 72, 30, 8, BLACK);//erase old ui
      screen.setCursor(78, 72);
      drawUIColorString(scrollPos, curUIColor);
      lastScrollPos = scrollPos;        
     }
     if (blueButtonPressed){
       screen.fillRect(78, 72, 30, 8, BLACK);//erase old ui
       drawUIColorBounding(BLACK);
       screen.setCursor(78, 72);
       drawUIColorString(curWatchUIColorMode, curUIColor);
       scrollPos = 3;
       drawSystemSettingBounding(scrollPos, curUIColor);
       curMode = MODE_SELECT_SYS_SETTING;
     }
     if (encoderButtonPressed){
       curWatchUIColorMode = scrollPos;
       switch (curWatchUIColorMode){//set new ui color
        case 0:
          curUIColor = PB_BLUE;
        break;
        case 1:
          curUIColor = PB_GREEN;
        break;
        case 2:
          curUIColor = PB_AMBER;
        break;
        case 3:
          curUIColor = PB_WHITE;
        break;
       }
       screen.fillScreen(BLACK); // erase whole screen and redraw!
       systemConfig(SYS_WRITE); //save new UI setting to eeprom 
       scrollPos = 3;
       drawDisplayMenu();
       drawSystemMenu();
       screen.drawRoundRect(16 + (36 * 2), 85, 23, 11, 3, curUIColor); //selected SYS
       drawSystemSettingBounding(scrollPos, curUIColor);
       curMode = MODE_SELECT_SYS_SETTING;
     }

  } // END MAIN SWITCHCASE

  if (blueButtonPressed){ // clear any button flags that haven't been processed in the switchcase
    blueButtonPressed = false;
  }
  if (redButtonPressed){
    redButtonPressed = false;
  }
  if (encoderButtonPressed){
    encoderButtonPressed = false;
  }

} // ***********************************************************************************[ END MAIN LOOP ]*********************************************************************************** 

void setRingerSettings(){
  
  if (ringOnCall == true){
    fona.setRinger(false); // 0 = normal sound
    fona.setRingerVolume(curVolume); // write ringer volume   
  }
  if (ringOnCall == false){
    fona.setRinger(true);
    //write ringer volume
  }
  
}

void systemConfig(boolean whichMode){

  boolean tempClockMode;
  
  if (whichMode == SYS_READ){
    curWatchUIColorMode = int(EEPROM.read(SYS_ADDRESS_UI_COLOR));
    vibrateOnCall = EEPROM.read(SYS_ADDRESS_VIBE_SETTING);  
    ringOnCall = EEPROM.read(SYS_ADDRESS_SOUND_SETTING);
    curVolume = EEPROM.read(SYS_ADDRESS_VOLUME_LEVEL);
    tempClockMode = EEPROM.read(SYS_ADDRESS_CLOCK_MODE);
    if (tempClockMode == 1){
      timeIs24Hr = true;
      timeIs12Hr = false;
    } else {
      timeIs24Hr = false;
      timeIs12Hr = true;
    }
    
    switch (curWatchUIColorMode){//set new ui color
        case 0:
          curUIColor = PB_BLUE;
        break;
        case 1:
          curUIColor = PB_GREEN;
        break;
        case 2:
          curUIColor = PB_AMBER;
        break;
        case 3:
          curUIColor = PB_WHITE;
        break;
       }
  }
  
  if (whichMode == SYS_WRITE){
    EEPROM.write(SYS_ADDRESS_UI_COLOR , byte(curWatchUIColorMode)); 
    EEPROM.write(SYS_ADDRESS_VIBE_SETTING, vibrateOnCall);  
    EEPROM.write(SYS_ADDRESS_SOUND_SETTING, ringOnCall);
    EEPROM.write(SYS_ADDRESS_VOLUME_LEVEL, curVolume);
    if (timeIs24Hr == true){
    EEPROM.write(SYS_ADDRESS_CLOCK_MODE, 1);
    } else {
    EEPROM.write(SYS_ADDRESS_CLOCK_MODE, 0);
    }
  }
  
} // END FUNCTION systemConfig(whichMode);

void drawUIColorBounding(uint16_t whichColor){
  
  screen.setTextColor(whichColor);
  screen.setCursor(72, 72);
  screen.print("<     >");

}

void drawVolumeBounding(uint16_t whichColor){

  screen.setTextColor(whichColor);
  screen.setCursor(72, 64);
  screen.print("<    >");
  
}

void drawToneSetting(boolean whichSetting, uint16_t whichColor){

  screen.setTextColor(whichColor);
  if (whichSetting == SYS_ON){
    screen.setCursor(72, 56);
    screen.print("[");
    screen.setCursor(95, 56);
    screen.print("]");
  } 
  if (whichSetting == SYS_OFF) {
    screen.setCursor(72 + 24, 56);
    screen.print("[");
    screen.setCursor(95 + 24, 56);
    screen.print("]");
  }
  
}

void drawVibeSetting(boolean whichSetting, uint16_t whichColor){

  screen.setTextColor(whichColor);
  if (whichSetting == SYS_ON){
    screen.setCursor(72, 48);
    screen.print("[");
    screen.setCursor(95, 48);
    screen.print("]");
  } 
  if (whichSetting == SYS_OFF) {
    screen.setCursor(72 + 24, 48);
    screen.print("[");
    screen.setCursor(95 + 24, 48);
    screen.print("]");
  }
  
}

void drawToneSettingBounding(int whichSelection, uint16_t whichColor){ // All "bounding" functions can be reduced to a universal one
  
  screen.setTextColor(whichColor);
  screen.setCursor(72 + (whichSelection * 24), 56);
  screen.print("<");
  screen.setCursor(95 + (whichSelection * 24), 56);
  screen.print(">");
          
}

void drawVibeSettingBounding(int whichSelection, uint16_t whichColor){
  
  screen.setTextColor(whichColor);
  screen.setCursor(72 + (whichSelection * 24), 48);
  screen.print("<");
  screen.setCursor(95 + (whichSelection * 24), 48);
  screen.print(">");
          
}

void drawUIColorString(int whichSelection, uint16_t whichColor){

  if (whichColor != BLACK){
    switch (whichSelection){
      case 0:
        whichColor = PB_BLUE;
      break;
      case 1:
        whichColor = PB_GREEN;
      break;
      case 2:
        whichColor = PB_AMBER;
      break;
      case 3:
        whichColor = PB_WHITE;
      break;
    }
  }
  screen.setTextColor(whichColor, BLACK);
  UIcolorString.setCharAt(0, UIcolorNameString.charAt(0 + (whichSelection * 5)));
  UIcolorString.setCharAt(1, UIcolorNameString.charAt(1 + (whichSelection * 5)));
  UIcolorString.setCharAt(2, UIcolorNameString.charAt(2 + (whichSelection * 5)));
  UIcolorString.setCharAt(3, UIcolorNameString.charAt(3 + (whichSelection * 5)));
  UIcolorString.setCharAt(4, UIcolorNameString.charAt(4 + (whichSelection * 5)));
  screen.print(UIcolorString);
  
}

void drawTimeModeBounding(int whichSelection, uint16_t whichColor){
  
  screen.setCursor(54, 28);
  screen.setTextSize(1);
  screen.setTextColor(whichColor);
  screen.setCursor(60 + (23 * whichSelection), 28);
  screen.print("<");
  screen.setCursor(78 + (23 * whichSelection), 28);
  screen.print(">");
  
}

void checkRedToHangUp(){
  
  if (redButtonPressed){  
      screen.fillScreen(BLACK);
      screen.setCursor(25, 44);
      screen.setTextSize(1);
      screen.setTextColor(curUIColor);
      screen.print("CALL CANCELED");
      fona.hangUp();
      delay(1500);
      screen.fillScreen(BLACK);
      drawLockScreen();
      curMode = MODE_DISPLAY_LOCK;
    }
    
}

void getPhoneCallStatus(){

  uint8_t callStatus = fona.getCallStatus();

  //Serial.print("Call Status: "); Serial.println(callStatus);
  
  if (callStatus == COM_STATUS_READY){
    readyToCall = true;
  } else {
    readyToCall = false;
  }
  if (callStatus == COM_STATUS_ERROR){
    readyToCall = false;
  }
  if (callStatus == COM_STATUS_UNKNOWN){
    readyToCall = false;
  }
  if (callStatus == COM_STATUS_ACTIVE){
    phoneOnCall = true;
  } else {
    phoneOnCall = false;
  }
  if (callStatus == COM_STATUS_RINGING){
    phoneIsRinging = true;
  } else {
    phoneIsRinging = false;
  }
  
}

void checkForIncomingCall(){

  if (fona.incomingCallNumber(incomingPhoneNumber)){ // clearScreen, buzz and tone, show number, pick up
    screen.fillScreen(BLACK);
    screen.setTextSize(0);
    screen.setTextColor(curUIColor);
    screen.setCursor(10, 36);
    screen.print("INCOMING CALL FROM:");
    screen.setCursor(34, 52);
    screen.print(incomingPhoneNumber);
    curMode = MODE_COM_INCOMING;  
  }
    
}

void clearActiveArea(){
  screen.fillRect(0, 16, screen.width(), 69, BLACK);
}

void checkRedBackToZeroPos(){
  if (redButtonPressed) scrollPos = 0;
}

void checkBlueBackToModeSelection(){
  
  if (blueButtonPressed){
      screen.drawRoundRect(16 + (36 * curWatchDisplayMode), 85, 23, 11, 3, BLACK);
      clearActiveArea();
      drawWatchModeBounding(curWatchDisplayMode, curUIColor);
      curMode = MODE_DISPLAY_SELECT_MODE;
      lastScrollPos = curWatchDisplayMode; // this way the select mode properly erases the section symbols if 
      drawWatchModeDescription(curWatchDisplayMode);
      scrollPos = curWatchDisplayMode;     
    }
       
}

void drawWatchModeDescription(int whichIndex){
  
  screen.setTextSize(1);
  screen.setTextColor(curUIColor);
  switch(whichIndex){
    case 0:
      screen.setCursor(22, 44);
      screen.print("CLOCK SETTINGS");
    break;
    case 1:
      screen.setCursor(7, 44);
      screen.print("COMMUNICATOR KEYPAD");
    break;
    case 2:
      screen.setCursor(4, 44);
      screen.print("SYSTEM CONFIGURATION");
    break;
  }
      
}

void drawVolumeLevel(int whichSelection, uint16_t whichColor){

  screen.setTextColor(whichColor);
  tempVolumeString = String(whichSelection);

  if (whichSelection < 100){
     volumeString.setCharAt(0, ' ');
     if (whichSelection < 10){
       volumeString.setCharAt(1, ' ');
       volumeString.setCharAt(2, tempVolumeString.charAt(0));
     } else {
       volumeString.setCharAt(1, tempVolumeString.charAt(0));
       volumeString.setCharAt(2, tempVolumeString.charAt(1));
     }
  } else {
    volumeString.setCharAt(0, tempVolumeString.charAt(0));
    volumeString.setCharAt(1, tempVolumeString.charAt(1));
    volumeString.setCharAt(2, tempVolumeString.charAt(2));
  }
  screen.println(volumeString);
  
}

void drawSystemMenu(){
  
  screen.setCursor(6, 24);
  screen.setTextSize(1);
  screen.setTextColor(curUIColor);
  
  //screen.println("POWER USE: 00 WH");
  screen.print("  BATTERY:  ");screen.print(batteryPercentage);screen.println("%");
  screen.print("    SIGNAL:  "); screen.print(fona.getRSSI()); screen.println("dB");
  screen.println();
  //draw line here
  screen.println("   VIBRATE:  ON  OFF");
  screen.println("      TONE:  ON  OFF");
  screen.print("    VOLUME:  "); drawVolumeLevel(curVolume, curUIColor);
  screen.print("  UI COLOR:  "); drawUIColorString(curWatchUIColorMode, curUIColor);
  drawVibeSetting(vibrateOnCall, curUIColor);
  drawToneSetting(ringOnCall, curUIColor);
  
}

void drawComMenu(){
   
  screen.setTextSize(1);
  screen.setTextColor(curUIColor);
  //screen.setCursor(25, 30);
  //screen.print("(000) 000-0000"); // dummy number
  screen.drawRect(22, 27, 89, 13, curUIColor);
  screen.setCursor(10, 52);
  screen.print("0 1 2 3 4 5 6 7 8 9");
  screen.setCursor(22, 68);
  screen.print("* # DIAL DELETE");

  //draw number box
  
}

void drawTimeModeSetting(uint16_t whichColor){ // update to parentheses
  
  int whichSelection = 0;
  if(timeIs12Hr) whichSelection = 0;
  if(timeIs24Hr) whichSelection = 1;
  screen.setTextColor(whichColor);
  screen.setCursor(60 + (23 * whichSelection), 28);
  screen.print("[");
  screen.setCursor(78 + (23 * whichSelection), 28);
  screen.print("]");
  
}

void drawTimeMenu(){
  
  screen.setCursor(6, 28);
  screen.setTextSize(1);
  screen.setTextColor(curUIColor, BLACK);
  screen.println("   MODE:  12  24");
  screen.print("    HOUR:  ");screen.println(hourString);
  screen.print("  MINUTE:  ");screen.println(minuteString);
  screen.print("   MONTH:  ");screen.println(monthString);
  screen.print("     DAY:  ");screen.println(dateString);
  screen.print("    YEAR:  ");screen.println(yearString);
  drawTimeModeSetting(curUIColor);
  
}
void drawDisplayMenuTime(){
  
  screen.setCursor(33, 1);
  screen.setTextSize(2);
  screen.setTextColor(curUIColor);
  screen.print(hourString);screen.print(":");screen.print(minuteString);
  screen.setTextSize(1);
  
}
void drawDisplayMenu(){

  switch(batteryPowerSymbol){
    case 1:
    screen.drawBitmap(0, 0, cmbat1, 32, 16, curUIColor); // full
    break;
    case 2:
    screen.drawBitmap(0, 0, cmbat2, 32, 16, curUIColor);
    break; 
    case 3:
    screen.drawBitmap(0, 0, cmbat3, 32, 16, curUIColor);
    break; 
    case 4:
    screen.drawBitmap(0, 0, cmbat4, 32, 16, curUIColor); 
    break; 
    case 5:
    screen.drawBitmap(0, 0, cmbat5, 32, 16, curUIColor); // empty
    break;  
  }
  drawDisplayMenuTime();
  screen.drawBitmap(95, 0, cmsigsym, 16, 16, curUIColor); // mod based on net status
  switch(networkStrengthSymbol){
    case 1:
    screen.drawBitmap(111, 0, cmsig1, 16, 16, curUIColor);
    break;
    case 2:
    screen.drawBitmap(111, 0, cmsig2, 16, 16, curUIColor);
    break; 
    case 3:
    screen.drawBitmap(111, 0, cmsig3, 16, 16, curUIColor);
    break; 
    case 4:
    screen.drawBitmap(111, 0, cmsig4, 16, 16, curUIColor);
    break; 
    case 5:
    screen.drawBitmap(111, 0, cmsig5, 16, 16, curUIColor);
    break;  
  }
  screen.setCursor(19, 87);
  screen.setTextSize(1);
  screen.println("CLK   COM   SYS");

}

void drawLockScreen(){
  //dow
  screen.setCursor(46, 0);
  screen.setTextSize(2);
  screen.setTextColor(curUIColor);
  screen.print(dowString);
  //time
  screen.setCursor(4, 30); 
  screen.setTextSize(4);
  screen.print(hourString);screen.print(":");screen.print(minuteString);  
  //date
  screen.setCursor(28, 80); 
  screen.setTextSize(2);
  screen.print(monthString);screen.print(" ");screen.print(dateString);
  //screen.print(yearString);
}

void drawTimeSettingBounding(int whichSetting, uint16_t whichColor){

  screen.setTextColor(whichColor);
  screen.setCursor(0, 28 + (charHeight * whichSetting));
  screen.print("<");
  screen.setCursor(52, 28 + (charHeight * whichSetting));
  screen.print(">");
  
}

void drawComSettingBounding(int whichSelection, uint16_t whichColor){

  screen.setTextColor(whichColor);
  if (whichSelection < 10){
    screen.setCursor(4 + (2 * charWidth * whichSelection), 52);
    screen.print("<");
    screen.setCursor(15 + (2 * charWidth * whichSelection), 52);
    screen.print(">");
  } else {
    switch (whichSelection){ // the next row of characters is oddly formatted
      case 10:
        screen.setCursor(16, 68);
        screen.print("<");
        screen.setCursor(16 + 11, 68);
        screen.print(">");
      break;
      case 11:
        screen.setCursor(28, 68);
        screen.print("<");
        screen.setCursor(39, 68);
        screen.print(">");
      break;
      case 12:
        screen.setCursor(40, 68);
        screen.print("<");
        screen.setCursor(69, 68);
        screen.print(">");
      break;
      case 13:
        screen.setCursor(70, 68);
        screen.print("<");
        screen.setCursor(111, 68);
        screen.print(">");
      break;
    }
  }
  
}

void drawSystemSettingBounding(int whichSetting, uint16_t whichColor){

  screen.setTextColor(whichColor);
  screen.setCursor(0, 48 + (charHeight * whichSetting));
  screen.print("<");
  screen.setCursor(64, 48 + (charHeight * whichSetting));
  screen.print(">");
  
}

void drawWatchModeBounding(int whichMode, uint16_t whichColor){ // modes CLK = 0, COM = 1, SYS = 2

  screen.setTextColor(whichColor);
  screen.setCursor(12 + (36 * whichMode), 87);
  screen.print("<");
  screen.setCursor(37 + (36 * whichMode), 87);
  screen.print(">");
  
}

void drawSplashScreen(){
  
  screen.fillScreen(BLACK);
  screen.fillRect(0, 24, 128, 2, curUIColor); 
  screen.drawBitmap(-32, 36, cmlogo, 128, 24, curUIColor); // really janky fix
  screen.drawBitmap(96, 35, cmlogo, 128, 24, curUIColor); // for some reason, LCD Assistant shifted the bitmap over 32 pixels, so it has a wrap-aroundish effect 
  screen.fillRect(0, 70, 128, 2, curUIColor);  
  
}

int getDayOfWeek(int tempYear, int tempMonth, int tempDay){ // 1 <= m <= 12,  y > 1752 (in the U.K.) Credit to Tomohiko Sakamoto via Wikipedia 

  const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  tempYear -= tempMonth < 3;
  return (tempYear + tempYear/4 - tempYear/100 + tempYear/400 + t[tempMonth-1] + tempDay) % 7; // 0 for Sunday, 1 for Monday etc.
  
}

void getPowerInfo(){
  
  //batteryVoltage = currentSensor.getBusVoltage_V();
  //currentDraw = currentSensor.getCurrent_mA();
  fona.getBattPercent(&batteryPercentage);

  if (batteryPercentage > 80) batteryPowerSymbol = 1;
  if (batteryPercentage > 60 && batteryPercentage <= 80) batteryPowerSymbol = 2;
  if (batteryPercentage > 40 && batteryPercentage <= 60) batteryPowerSymbol = 3;
  if (batteryPercentage > 20 && batteryPercentage <= 40) batteryPowerSymbol = 4;
  if (batteryPercentage <= 20) batteryPowerSymbol = 5;

} // END FUNCTION getPowerDraw();

void getTimeInfo(){

  curHour = hour();
  curMinute = minute();
  curDate = day();
  curMonth = month();
  curYear = year();
  if (timeIs12Hr && curHour > 12) curHour -= 12;
 
  tempHourString = String(curHour);
  tempMinuteString = String(curMinute);
  tempDateString = String(curDate);
  //tempMonthString = String(curMonth);
  yearString = String(curYear);
  curDow = getDayOfWeek(curYear, curMonth, curDate);
  
  dowString.setCharAt(0, dowNameString.charAt(curDow * 3));
  dowString.setCharAt(1, dowNameString.charAt(curDow * 3 + 1));
  dowString.setCharAt(2, dowNameString.charAt(curDow * 3 + 2));

  monthString.setCharAt(0, monthNameString.charAt((curMonth-1) * 3));
  monthString.setCharAt(1, monthNameString.charAt((curMonth-1) * 3 + 1));
  monthString.setCharAt(2, monthNameString.charAt((curMonth-1) * 3 + 2));
   
   if (curHour < 10){
     hourString.setCharAt(0, ' '); // add a leading space
     hourString.setCharAt(1, tempHourString.charAt(0));
   } else {
     hourString.setCharAt(0, tempHourString.charAt(0));
     hourString.setCharAt(1, tempHourString.charAt(1));
   }
   if (curMinute < 10){
     minuteString.setCharAt(0, '0'); // add a leading 0
     minuteString.setCharAt(1, tempMinuteString.charAt(0));
   } else {
     minuteString.setCharAt(0, tempMinuteString.charAt(0));
     minuteString.setCharAt(1, tempMinuteString.charAt(1));
   }
   if (curDate < 10){
     dateString.setCharAt(0, '0'); // add a leading 0
     dateString.setCharAt(1, tempDateString.charAt(0));
   } else {
     dateString.setCharAt(0, tempDateString.charAt(0));
     dateString.setCharAt(1, tempDateString.charAt(1));
   }
 /*  if (curMonth < 10){ // This is always displayed in 3 letter characters, but could be in numbers
     monthString.setCharAt(0, '0'); // add a leading 0
     monthString.setCharAt(1, tempMonthString.charAt(0));
   } else {
     monthString.setCharAt(0, tempMonthString.charAt(0));
     monthString.setCharAt(1, tempMonthString.charAt(1));
   }*/
}

void getNetworkInfo(){

  uint8_t networkRSSI = fona.getRSSI();
  int networkStrengthSymbol = 1;
  networkStrengthSymbol = map(networkRSSI, 0, 30, 4, 0); //this is super coarse
  
}

void getInput(){
  
  scrollPos += getEncoder(); // modify the relative position of scrollPos +/- 1
  if (scrollPos > scrollMax) scrollPos = 0;
  if (scrollPos < 0) scrollPos = scrollMax;
  
  if(encoderButton.update()){ // any button change?
    if (encoderButton.fallingEdge()){ //rotaryButton.fallingEdge(); = press || rotaryButton.risingEdge(); = release 
      encoderButtonPressed = true; // used as a flag that will be interpreted differently in the various modes
    }
  }
  if(redButton.update()){
    if (redButton.fallingEdge())redButtonPressed = true; 
  }
  if(blueButton.update()){
    if (blueButton.fallingEdge())blueButtonPressed = true; 
  }
  
} // END FUNCTION getInput()

int getEncoder(){
  
  int curRotPos = myEnc.read();
  int rotaryDelta = 0;
  
  if (abs(curRotPos - lastRotPos) >= 4) { // a single detent in the encoder usually returns 3 unique presses, this limits false rotations
    if (curRotPos - lastRotPos > -1){ // positive increase
      rotaryDelta = -1; 
    } else {
      rotaryDelta = 1;
    }
    lastRotPos = curRotPos;
  } else {
    rotaryDelta = 0; // no change in position
  }
  
  return rotaryDelta;
  
} // END FUNCTION getEncoder()

uint16_t RGB888toRGB565(const char *rgb32_str_){ // Credit to Tom Carpenter on the Arduino forum for an RGB888 to RGB565 function!

  typedef union {
    uint16_t integer;
    struct{
      uint8_t low;
      uint8_t high;
    };
  } Byter;
  
  byte red;
  byte green;
  Byter rgb16;
                          
  green = hexToNibble(rgb32_str_[2])<<4;
  green |= hexToNibble(rgb32_str_[3])&0xC;
  rgb16.low = hexToNibble(rgb32_str_[4])<<4;
  rgb16.low |= hexToNibble(rgb32_str_[5])&0x8;
  rgb16.high = hexToNibble(rgb32_str_[0])<<4;
  rgb16.high |= hexToNibble(rgb32_str_[1])&0x8;
  rgb16.low >>= 3;
  rgb16.integer |= (green << 3);
  
  return rgb16.integer;
}
inline byte hexToNibble(char hex) {
  if (hex & _BV(6)){
    hex += 9;
  }
  return hex;
  
} // END FUNCTION RGB888toRGB565()

time_t getTeensy3Time(){
  
  return Teensy3Clock.get();
  
} // END FUNCTION getTeensy3Time()

