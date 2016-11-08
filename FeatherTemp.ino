
/**************************************************************************
 *
 * This is a simple project to monitor the temperature 
 * the result is logged to an MicroSD Card
 * an OLED display is also used to display the current value
 * 
 * This is an overall test of a few features of the Adafruit Feathers
 * and of the MCP9808.
 * 
 * This code is based on the demos provided by Adafruit.
 * 
 * As they say... they spend time and resources making those products and
 * the linked demos... aso support them by purchasing their products.
 * 
 * The products used for this projects are:
 * MCP 9808 Breakout http://www.adafruit.com/products/1782
 * Adafruit Feather M0 Basic Proto https://www.adafruit.com/product/2772
 * Adafruit FeatherWing Adalogger https://www.adafruit.com/products/2922
 * Adafruit FeatherWing OLED display https://www.adafruit.com/products/2900
 * 
 **************************************************************************/

#include <Wire.h>
#include <SPI.h>
#include <RTClib.h>
#include <SD.h>
#include "Adafruit_MCP9808.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"

// Some Debug help
//define DEBUG
#ifdef DEBUG
 #define DEBUG_PRINT(x)    Serial.print(x)
 #define DEBUG_PRINTLN(x)  Serial.println(x); Serial.flush()
#else
 #define DEBUG_PRINT(x)
 #define DEBUG_PRINTLN(x) 
#endif

// Create the MCP9808 temperature sensor object
Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();

// Create the Real Time Clock object
RTC_PCF8523 rtc;

// OLED Display Object
#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

#define BUTTON_A 9
#define BUTTON_B 6
#define BUTTON_C 5

// for SD Card
const int chipSelect = 10;

// Temperature logging to SD card interval.
const int refreshInterval = 10 * 60; // 10 minutes in seconds.

// Display prefs. 
// Do we display the degrees in *C or *F
volatile bool boolDegC=true;
// Is the display ON or OFF. Off saves power.
bool boolDisplayOn=true;
volatile bool boolDisplayToggle=false;

long lastLogFileUpdate=0;

// Let's get ready.
void setup() {
  #ifdef DEBUG
  Serial.begin(9600);
  #endif
  DEBUG_PRINTLN("FeatherTemp Project");
  
  // Make sure the temperature sensor is found
  if (!tempsensor.begin()) {
    DEBUG_PRINTLN("Couldn't find MCP9808!");
    while (1);
  }

  // Check for RealTime clock.
  if (! rtc.begin()) {
    DEBUG_PRINTLN("Couldn't find RTC");
    while (1);
  }
  // Adjust real-time clock to current compile time (make sure computer clock is correct).
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  
  // see if the SD card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    DEBUG_PRINTLN("Card failed, or not present");
    while (1);
  }
  DEBUG_PRINTLN("card initialized.");

  // Start the OLED Display
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  display.display();
  delay(1000);
 
  // Clear the display buffer.
  display.clearDisplay();
  display.display();

    // text display tests
  display.setTextSize(2);
  display.setTextColor(WHITE);

  DEBUG_PRINTLN("Display started");

  // Setup the buttons on the OLED FeatherWing
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);

  // register interrupt handler for the on/off button for the display.
  attachInterrupt(digitalPinToInterrupt(BUTTON_C), toggleDisplay, FALLING); 
  attachInterrupt(digitalPinToInterrupt(BUTTON_A), toggleDegC, FALLING); 
}

// Main loop.
void loop() {
  // Check if we have any thing to do...
  // either because the display is ON or because it is time to Log to the File
  DateTime now = rtc.now();
  bool boolLogToFile = lastLogFileUpdate + refreshInterval < now.unixtime();

  if (boolDisplayToggle) {
    boolDisplayOn = !boolDisplayOn;
    boolDisplayToggle = false;
    
    if (boolDisplayOn) {
      display.ssd1306_command(SSD1306_DISPLAYON); // To switch display back on
    } else {
      display.ssd1306_command(SSD1306_DISPLAYOFF); // To switch display off
    }
    DEBUG_PRINTLN("Display toggled.");
  }
  
  if (boolDisplayOn || boolLogToFile) {
    char stime[10];
    DateTime now = rtc.now();
    sprintf( stime, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());

    // Read and print out the temperature, then convert to *F
    float degC = getDegC();

    if (boolDisplayOn) {
      display.clearDisplay();
      display.setCursor(0,0);
      display.println(stime);

      if (boolDegC) {
        display.print(degC); display.println("*C"); 
      }
      else {
        float degF = degC * 9.0 / 5.0 + 32;
        display.print(degF); display.println("*F");
      }
  
      display.display(); // actually display all of the above
      DEBUG_PRINTLN("Display updated.");
    }

    if (boolLogToFile) {
      lastLogFileUpdate = now.unixtime();
      logToFile(now, stime, degC);
    }
    
    delay(1000);
  }
}

/* Called from interrupt to turn display on/off */
/* not linked to the timer used for the main refresh of the temperature. */
void toggleDisplay() {
  boolDisplayToggle = true;
}

/* Called from interrupt to toogle the display of degrees in *C or *F */
void toggleDegC() {
  boolDegC = !boolDegC;
}

void logToFile(DateTime now, char *stime, float degC) {
  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File logfile = SD.open("temp_log.txt", FILE_WRITE);

  // if the file is available, write to it:
  if (logfile) {
    logfile.print(now.unixtime()); // seconds since 1/1/1970
    logfile.print(",\"");

    char sdate[10];
    sprintf(sdate, "%02d/%02d/%02d", now.year(), now.month(), now.day());
    logfile.print(sdate);
    logfile.print(" ");
    logfile.print(stime);
    logfile.print("\",");
    logfile.println(degC, DEC);
    logfile.close();
    DEBUG_PRINTLN("Logged to SD.");
  }
}

float getDegC() {
  DEBUG_PRINTLN("Wakeup MCP9808.");
  tempsensor.shutdown_wake(0);   // Don't remove this line! required before reading temp
  delay(250); // DO NOT remove otherwise temp sensor might not update correctly.

  // Read and print out the temperature in *C
  float degC = tempsensor.readTempC();
  DEBUG_PRINT("Temp: "); DEBUG_PRINT(degC); DEBUG_PRINTLN("*C\t"); 

  DEBUG_PRINTLN("Shutdown MCP9808.");

  tempsensor.shutdown_wake(1); // shutdown MSP9808 - power consumption ~0.1 mikro Ampere

  return degC;
}
