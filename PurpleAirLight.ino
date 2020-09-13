// Sketch that grabs AQI from Purple Air and lights an Adafruit RGB LED to show the status

//#define DEBUG       // Used to show/hide the debug messages on the serial monitor

#include <ESP8266HTTPClient.h>    // Include HTTP client library
#include <ESP8266WiFi.h>          // Include wi-fi library
#include <ArduinoJson.h>          // Include json parsing library
#include <Adafruit_NeoPixel.h>    // Include Neopixel library
#include <NTPClient.h>            // Library to get network time
#include <WiFiUdp.h>              // Needed for NTPClient

const char* ssid      = "Palm";
const char* password  = "!1234567890";

bool isDST = true;        // Set whether we are in daylight savings time. Should find a way to automate this...

WiFiUDP ntpUDP;

// By default 'pool.ntp.org' is used with 60 seconds update interval and
// no offset
NTPClient timeClient(ntpUDP);

// Define pin that neopixels are connected to and number of pixels
#define PIN 14
#define NUMPIXELS 24

// When setting up the NeoPixel library, we tell it how many pixels,
// and which pin to use to send signals. Note that for older NeoPixel
// strips you might need to change the third parameter -- see the
// strandtest example for more information on possible values.
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);




void setup() {

#ifdef DEBUG
  Serial.begin(9600);
  delay(10);
  Serial.println('\n');
#endif

  WiFi.begin(ssid, password);   // connect to network

#ifdef DEBUG
  Serial.print("Connecting to ");
  Serial.print(ssid);
  Serial.println(" ...");
#endif


  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {   // Wait for the wi-fi to connect
    delay (1000);

#ifdef DEBUG
    Serial.print(++i);
    Serial.print(' ');
#endif
  }

  pixels.begin(); // INITIALIZE NeoPixel strip object
  pixels.clear();

#ifdef DEBUG
  Serial.println('\n');
  Serial.println("Connection established!");
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());
#endif

  timeClient.begin();       // Start time client

  if (isDST) timeClient.setTimeOffset(-25200);    // Set timzone offset for daylight saving time
  else timeClient.setTimeOffset(-28800);          // Set timezone offset for standard time
}




// Function that calculates the air quality index from the PM2.5 value.
// Calculation is from Purple Air https://docs.google.com/document/d/15ijz94dXJ-YAZLi9iZ_RaBwrZ4KtYeCy08goGBwnbCU/edit#
float calcAQI(float Cp, float Ih, float Il, float BPh, float BPl) {

  float result;

  result = (((Ih - Il) / roundf((BPh - BPl)) * (Cp - BPl) + Il));
  return result;
}




void loop() {
  // Check WiFi Status
  if (WiFi.status() == WL_CONNECTED) {

    timeClient.update();      // Update time

    HTTPClient http;          //Object of class HTTPClient
    http.begin("http://www.purpleair.com/json?key=KMIGXBTGHDZ0C4C9&show=18503");
    int httpCode = http.GET();

    //Check the returning code
    if (httpCode > 0) {

      // Parse the response
      const size_t capacity = JSON_ARRAY_SIZE(2) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(30) + JSON_OBJECT_SIZE(40) + 1350;
      DynamicJsonDocument doc(capacity);

      deserializeJson(doc, http.getString());

      JsonObject results_0 = doc["results"][0];
      const char* results_0_Label = results_0["Label"];     // Location label of sensor
      float results_0_pm2_5_atm = results_0["pm2_5_atm"];   // PM2.5 reading from sensor

      float myAQI;

      // Part of air quality index calculation, again from Purple Air (see above for url)
      if (results_0_pm2_5_atm > 350.5) myAQI = calcAQI(results_0_pm2_5_atm, 500.0, 401.0, 500.0, 350.5);
      else if (results_0_pm2_5_atm > 250.5) myAQI = calcAQI(results_0_pm2_5_atm, 400.0, 301.0, 350.4, 250.5);
      else if (results_0_pm2_5_atm > 150.5) myAQI = calcAQI(results_0_pm2_5_atm, 300.0, 201.0, 250.4, 150.5);
      else if (results_0_pm2_5_atm > 55.5) myAQI = calcAQI(results_0_pm2_5_atm, 200.0, 151.0, 150.4, 55.5);
      else if (results_0_pm2_5_atm > 35.5) myAQI = calcAQI(results_0_pm2_5_atm, 150.0, 101.0, 55.4, 35.5);
      else if (results_0_pm2_5_atm > 12.1) myAQI = calcAQI(results_0_pm2_5_atm, 100.0, 51.0, 35.4, 12.1);
      else myAQI = calcAQI(results_0_pm2_5_atm, 50.0, 0.0, 12.0, 0.0);

#ifdef DEBUG
      Serial.print("Location: "); Serial.println(results_0_Label);
      Serial.print("PM2.5: "); Serial.println(results_0_pm2_5_atm);
      Serial.print("AQI: "); Serial.println(myAQI);
#endif

      int red_value = 0, blue_value = 0, green_value = 0;                      // Variables to hold rgb colors

      // Set color values based on AQI readings. Using thresholds from Purple Air
      if (myAQI > 200.0) red_value = 102, green_value = 0, blue_value = 102;   // Purple
      else if (myAQI > 150.0) red_value = 255, green_value = 0, blue_value = 0; // Red
      else if (myAQI > 100.0) red_value = 255, green_value = 153, blue_value = 0; // Orange
      else if (myAQI > 50.0) red_value = 255, green_value = 140, blue_value = 0; // Yellow
      else red_value = 0, green_value = 255, blue_value = 0;                   // Green

      // Assign color values to pixels
      for (int i = 0; i < NUMPIXELS; i++) {
        pixels.setPixelColor(i, pixels.Color(red_value, green_value, blue_value));
      }

      // Dim the light at night (between 9pm and 6am) and full brightness in the day
      if (timeClient.getHours() > 21 || timeClient.getHours() < 6) pixels.setBrightness(75);
      else pixels.setBrightness(255);

      pixels.show();    // Sends the command to the neopixels

#ifdef DEBUG
      Serial.println((String)"Red:" + red_value + " Green:" + green_value + " Blue:" + blue_value);
      Serial.println(timeClient.getHours());
#endif

    }
    http.end();   //Close connection
  }

  // Delay
  delay(10000);

}
