#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Replace with your network credentials
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// Define OLED specs
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA to pin 21, SCL to pin 22)
#define OLED_RESET -1  // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Enable builtin LED
#define LedBuiltIn 2

#define BUTTON_PIN 18      //Button to switch time format 24hrs 12hrs
#define DST_BUTTON_PIN 23  //DST daylight saving time

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
String IP;

int xPos = SCREEN_WIDTH;
int xPos2 = SCREEN_WIDTH;

int timeZoneOffset = -18000;  // for Eeatern Time USA
// Variables to save date and time
String formattedDate;
String dayStamp;
String timeStamp;
bool is24HourFormat = true;  // true for 24-hour format, false for 12-hour format
bool isDST = false;          // true if DST is enabled

// Store day and month names
const String Days[] = { "SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY" };
const String Months[] = { "JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE", "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER" };
uint8_t secondsIndex;  // Stores seconds
uint8_t minutesIndex;  // Stores minutes
uint8_t hoursIndex;    // Stores hours
uint8_t dayIndex;      // Stores day of week
uint8_t monthDay;      // Stores day of month
uint8_t monthIndex;    // Stores month
uint16_t year;         // Stores year

// Functions
void Time(bool forceUpdate);
int getMonth();     // returns month in integer form 1-12
int getMonthDay();  // returns day of the month in integer form 1 - 31
int getYear();      // returns year in integer form 2020+
void DefaultFrame();
// Draws line on x axis with animation at different heights
// starting y position, Ending y position
void DrawLineAnimated(int j, int k);
//  Draws line on defined sSarting x, Starting y, Ending y positions
void DrawXLine(int i, int j, int k);

// General string display functions
// String to be displayed, X cursor, Y, cursor, Font size
void DisplayRawText(String s, int i, int j, int k);
void AutoTextDisplay(String s, int i, int j, int k);


// Define states for the scrolling text
enum ScrollState {
  SCROLLING,
  PAUSED
};

ScrollState scrollState = SCROLLING;
unsigned long pauseStartTime = 0;

// Define a debounce delay time in milliseconds
const unsigned long debounceDelay = 50;
bool buttonState = HIGH;             // Current state of the button
bool lastButtonState = HIGH;         // Previous state of the button
unsigned long lastDebounceTime = 0;  // The last time the button state was toggled
bool dstButtonState = HIGH;          // Current state of the button
bool lastDSTButtonState = HIGH;
unsigned long lastDSTDebounceTime = 0;


void setup() {
  pinMode(LedBuiltIn, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);      // Configure button pin
  pinMode(DST_BUTTON_PIN, INPUT_PULLUP);  // Configure DST button pin
 
  // Initialize Serial Monitor
  Serial.begin(115200);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }
  display.setTextWrap(false);
  display.clearDisplay();
  display.display();

  AutoTextDisplay("Connecting to: ", 0, 0, 1);
  AutoTextDisplay(ssid, 0, 8, 1);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  String ipaddress = WiFi.localIP().toString();
  // Print local IP address and start web server
  AutoTextDisplay("Wifi Connected!", 0, 16, 1);
  AutoTextDisplay("IP address: ", 0, 24, 1);
  AutoTextDisplay(ipaddress, 0, 32, 1);
  delay(1000);
  display.clearDisplay();
  // Initialize a NTPClient to get time
  timeClient.begin();
  timeClient.setTimeOffset(timeZoneOffset);
  DisplayRawTime("LOADING...", 30, 1);
  DrawLineAnimated(45, 45);
  display.clearDisplay();
  delay(500);
}

void loop() {
  static unsigned long PassedTime{ 0 };  // Variable to check passed time without using delay
  static unsigned long PassedTime2{ 0 };
  // update the time
  timeClient.update();

  if (PassedTime <= millis() - 1000) {  // update time once per second
    Time(false);
    // display.clearDisplay();
    DefaultFrame();
    PassedTime = millis();
  }

  if (WiFi.status() == WL_CONNECTED && PassedTime2 <= millis() - 5) {
    PassedTime2 = millis();
    //DisplayScrollText("WiFi Connected!", 57, 1);
    DisplayScrollTextPaused("WiFi Connected!", 57, 1);
    //DisplayRawTime("WiFi Connected!", 57, 1);
    //DisplayRawText("WiFi", 0, 57, 1);
  }

  display.display();
  readButtons();
}

void readButtons() {
  // Read the state of the button
  int reading = digitalRead(BUTTON_PIN);

  // If the button state has changed, reset the debounce timer
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  // Check if the button state has been stable for the debounce delay
  if ((millis() - lastDebounceTime) > debounceDelay) {
    // If the button state has changed
    if (reading != buttonState) {
      buttonState = reading;
      // If the button is pressed
      if (buttonState == LOW) {
        is24HourFormat = !is24HourFormat;  // Toggle the time format
        timeClient.update();
        Time(true);  // Force update time
      }
    }
  }
  // Save the reading. Next time through the loop, it'll be the lastButtonState
  lastButtonState = reading;

  // Check if the DST button is pressed to toggle DST
  int dstReading = digitalRead(DST_BUTTON_PIN);

  if (dstReading != lastDSTButtonState) {
    lastDSTDebounceTime = millis();
  }

  if ((millis() - lastDSTDebounceTime) > debounceDelay) {
    if (dstReading != dstButtonState) {
      dstButtonState = dstReading;
      if (dstButtonState == LOW) {
        //Time();
        isDST = !isDST;  // Toggle DST
        if (!isDST) {
          timeZoneOffset += 3600;  // Add 1 hour for DST
        } else {
          timeZoneOffset -= 3600;  // Subtract 1 hour for standard time
        }
        timeClient.setTimeOffset(timeZoneOffset);
        //timeClient.forceUpdate(); // Force immediate update
        timeClient.update();
        Time(true);  // Force update time

        // Manually adjust the time
        //hoursIndex = (hoursIndex + (isDST ? 1 : -1)) % 24;
        //if (hoursIndex < 0) {
        //  hoursIndex += 24;
        //}
      }
    }
  }

  lastDSTButtonState = dstReading;
}

void DefaultFrame() {
  static bool blinkFlag = false;  // Flag to alternate the blinking colons
  //display.clearDisplay();
  //DisplayRawText("NTP CLOCK", 38, 0, 1);
  DisplayRawTime("ESP32 NTP CLOCK", 0, 1);

  DrawXLine(0, 9, 9);
  //DisplayRawText(Days[dayIndex] + ", the ", 0, 12, 1);
  DisplayRawTime(Days[dayIndex], 12, 1);

  //if (monthDay == 1) DisplayRawText(String(monthDay) + "st of " + Months[monthIndex - 1] + " " + year, 0, 21, 1);
  //else if (monthDay == 2) DisplayRawText(String(monthDay) + "nd of " + Months[monthIndex - 1] + " " + year, 0, 21, 1);
  //else if (monthDay == 3) DisplayRawText(String(monthDay) + "rd of " + Months[monthIndex - 1] + " " + year, 0, 21, 1);
  //else DisplayRawText(String(monthDay) + "th of " + Months[monthIndex - 1] + " " + year, 0, 21, 1);


  // if (monthDay == 1) DisplayRawText(Months[monthIndex - 1] + " " + String(monthDay) + "st, " + year, 0, 21, 1);
  // else if (monthDay == 2) DisplayRawText(Months[monthIndex - 1] + " " + String(monthDay) + "nd, " + year, 0, 21, 1);
  // else if (monthDay == 3) DisplayRawText(Months[monthIndex - 1] + " " + String(monthDay) + "rd, " + year, 0, 21, 1);
  // else DisplayRawText(Months[monthIndex - 1] + " " + String(monthDay) + "th, " + year, 0, 21, 1);

  if (monthDay == 1) DisplayRawTime(Months[monthIndex - 1] + " " + String(monthDay) + "st, " + year, 21, 1);
  else if (monthDay == 2) DisplayRawTime(Months[monthIndex - 1] + " " + String(monthDay) + "nd, " + year, 21, 1);
  else if (monthDay == 3) DisplayRawTime(Months[monthIndex - 1] + " " + String(monthDay) + "rd, " + year, 21, 1);
  else DisplayRawTime(Months[monthIndex - 1] + " " + String(monthDay) + "th, " + year, 21, 1);

  // Clear the field part for the time
  display.fillRect(0, 34, SCREEN_WIDTH, 16, SSD1306_BLACK);
  // Display time with blinking colons
  display.setTextSize(2);  // Set text size to 2 for time
  if (!is24HourFormat) display.setCursor(6, 34);
  if (is24HourFormat) display.setCursor(15, 34);
  uint8_t displayHours = hoursIndex;  // Store the hours to display
  String ampm = "";

  if (!is24HourFormat) {
    // Convert hours to 12-hour format
    if (hoursIndex >= 12) {
      ampm = "PM";
      if (hoursIndex > 12) {
        displayHours = hoursIndex - 12;
      }
    } else {
      ampm = "AM";
      if (hoursIndex == 0) {
        displayHours = 12;
      }
    }
  }

  // Handle 12-hour format without leading zero for hours
  if (!is24HourFormat && displayHours < 10) {
    display.print(" ");
  } else {
    display.print(displayHours < 10 ? "0" : "");  // Leading zero for hours in 24-hour format
  }
  display.print(displayHours);

  if (blinkFlag) {
    display.print(":");
  } else {
    display.print(" ");
  }

  display.print(minutesIndex < 10 ? "0" : "");  // Leading zero for minutes
  display.print(minutesIndex);

  if (!blinkFlag) {
    display.print(":");
  } else {
    display.print(" ");
  }

  display.print(secondsIndex < 10 ? "0" : "");  // Leading zero for seconds
  display.print(secondsIndex);

  // Display AM/PM for 12-hour format
  if (!is24HourFormat) {
    display.setTextSize(1);  // Set text size to 1 for AM/PM
    display.print(" ");
    display.print(ampm);  // Display AM/PM
  }
  // Update blink flag
  blinkFlag = !blinkFlag;

  DrawXLine(0, 52, 52); 
}

void AutoTextDisplay(String s, int x, int y, int txtSize) {
  DisplayRawText(s, x, y, txtSize);
  display.display();
}

void DisplayRawText(String s, int x, int y, int txtSize) {
  display.setTextSize(txtSize);  // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(x, y);
  display.println(s);
}

void DisplayRawTime(String s, int y, byte txtSize) {
  byte txtWidth;
  if (txtSize == 1) txtWidth = 6;
  if (txtSize == 2) txtWidth = 12;
  int center = display.width() / 2;
  int textLength = s.length() * txtWidth;
  display.setTextSize(txtSize);  // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(center - (textLength / 2), y);
  display.println(s);
}

void DisplayScrollText(String s, int y, byte txtSize) {
  byte txtWidth;
  if (txtSize == 1) txtWidth = 6;
  if (txtSize == 2) txtWidth = 12;
  int center = display.width() / 2;
  int textLength = s.length() * txtWidth;
  xPos--;
  display.fillRect(0, y, SCREEN_WIDTH, 8, SSD1306_BLACK);
  display.setTextSize(txtSize);  // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(xPos, y);
  display.println(s);
  if (xPos <= -textLength) {
    xPos = SCREEN_WIDTH + 20;
  }
}

void DisplayScrollTextPaused(String texMessage, int y, byte txtSize) {
  byte txtWidth;
  if (txtSize == 1) txtWidth = 6;
  if (txtSize == 2) txtWidth = 12;
  
  int TxtLength = texMessage.length() * txtWidth;
  // State machine to manage scrolling and pausing
  switch (scrollState) {
    case SCROLLING:
      xPos2--;

      // Check if the text should pause in the center of the display
      if (xPos2 == (SCREEN_WIDTH - TxtLength) / 2) {
        scrollState = PAUSED;
        pauseStartTime = millis();  // Record the start time of the pause
      }

      // Clears the bottom part of the display for the scrolling text
      display.fillRect(0, y, SCREEN_WIDTH, 8, SSD1306_BLACK);
      
      display.setTextSize(txtSize);
      display.setCursor(xPos2, y);
      display.print(texMessage);
      if (xPos2 <= -TxtLength - 10) {
        xPos2 = SCREEN_WIDTH + 10;
      }      
      break;
    case PAUSED:
      // Check if the pause duration has elapsed
      if (millis() - pauseStartTime >= 10000) {
        scrollState = SCROLLING;  // Resume scrolling
      }
      break;
  }  
}

void Time(bool forceUpdate) {
  digitalWrite(LedBuiltIn, HIGH);
  // Initialize at non-valid numbers so that the function updates their values on first call
  static uint8_t lastminute{ 61 };
  static uint8_t lasthour{ 25 };
  static uint8_t lastday;
  static uint8_t lastmonth;

  // The formattedDate comes with the following format:
  // 2018-05-28T16:00:13Z
  // We need to extract date and time
  formattedDate = timeClient.getFormattedTime();
  secondsIndex = timeClient.getSeconds();
  minutesIndex = timeClient.getMinutes();

  if (forceUpdate || lastminute != minutesIndex) {
    //if (lastminute != minutesIndex) {
    hoursIndex = timeClient.getHours();
    lastminute = minutesIndex;
    if (lasthour != hoursIndex) {
      dayIndex = timeClient.getDay();
      monthDay = getMonthDay();
      lasthour = hoursIndex;
      if (lastday != monthDay) {
        monthIndex = getMonth();
        lastday = monthDay;
        if (lastmonth != monthIndex) {
          year = getYear();
          lastmonth = monthIndex;
        }
      }
    }
  }
  digitalWrite(LedBuiltIn, LOW);
}

int getMonthDay() {
  int monthday;
  time_t rawtime = timeClient.getEpochTime();
  struct tm* ti;
  ti = localtime(&rawtime);
  monthday = (ti->tm_mday) < 10 ? 0 + (ti->tm_mday) : (ti->tm_mday);
  return monthday;
}

int getMonth() {
  int month;
  time_t rawtime = timeClient.getEpochTime();
  struct tm* ti;
  ti = localtime(&rawtime);
  month = (ti->tm_mon + 1) < 10 ? 0 + (ti->tm_mon + 1) : (ti->tm_mon + 1);
  return month;
}

int getYear() {
  int year;
  time_t rawtime = timeClient.getEpochTime();
  struct tm* ti;
  ti = localtime(&rawtime);
  year = ti->tm_year + 1900;
  return year;
}

void DrawLineAnimated(int y, int k) {
  uint8_t i;
  //display.clearDisplay();
  display.fillRect(0, y, SCREEN_WIDTH, 1, SSD1306_BLACK);
  for (i = 0; i < display.width(); i += 2) {
    display.drawLine(0, y, i, k, SSD1306_WHITE);
    display.display();  // Update screen with each newly-drawn line
  }
}

void DrawXLine(int x, int y, int k) {
  for (x; x < display.width(); x++) {
    display.drawLine(x, y, x, k, SSD1306_WHITE);
  }
}
