/*
    W. Hoogervorst, based on based on the 'CameraWebServer' example from the Arduino IDE for the AI Thinker ESP32-CAM board
    2.9 inch Waveshare e-paper B/W/R
    resolution 296x128
*/

#include "esp_camera.h"
#include <WiFi.h>
#include <credentials.h> (Wifi and MQTT credentials)
#include <PubSubClient.h>

// include library, include base class, make path known
#include <GxEPD.h>
#include <GxGDEW029Z10/GxGDEW029Z10.h>    // 2.9" b/w/r

// FreeFonts from Adafruit_GFX
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/WHSymbolMono18pt7b.h>

#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>

// for SPI pin definitions see e.g.:
// C:\Users\xxx\Documents\Arduino\hardware\espressif\esp32\variants\lolin32\pins_arduino.h

//GxIO_Class io(SPI, /*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16); // arbitrary selection of 17, 16
//GxEPD_Class display(io, /*RST=*/ 16, /*BUSY=*/ 4); // arbitrary selection of (16), 4

SPIClass  epaper_SPI(HSPI); // define the SPI for the e-paper on the HSPI pins (CLK 14, MOSI 13, CS 15)
/*
  #define CS 15
  #define DC 4
  #define RST 2
  #define BUSY 16
*/
GxIO_Class io(epaper_SPI, /*CS*/ 15, /*DC*/ 4, /*RST*/ 2);
GxEPD_Class display(io, /*RST*/ 2,  /*BUSY*/ 16 );

// Select camera model
#define CAMERA_MODEL_AI_THINKER

#include "camera_pins.h"

#define WIFI_CONNECT_TIMEOUT_S 15
// define TABS and LINES for text and symbols
#define FIRSTLINE 22
#define FIRSTHALFLINE 40
#define SECONDLINE 48
#define THIRDLINE 73
#define FOURTHLINE 98
#define FIFTHLINE 122

#define FIRSTTAB 5
#define SECONDTAB 80
#define THIRDTAB 105
#define FOURTHTAB 180
#define FIFTHTAB 210
#define SIXTHTAB 245
#define SEVENTHTAB 265
#define XMAX 296

#define THICKNESS 3
#define OFFSET 5
#define CIRCLESIZE 7

const char* mqtt_id = "hall";
WiFiClient espClient;
PubSubClient client(espClient);
uint32_t time1, time2, debouncestart;

char* doors_topic = "sensor/doorsgroup";
char* voltagemonitors_topic = "sensor/voltagemonitors";
char* backdoor_topic = "sensor/doorsensor1";
char* frontdoor_topic = "sensor/doorsensor2";
char* sheddoor_topic = "sensor/doorsensor3";
char* shedlock_topic = "sensor/locksensor3";
char* bikesheddoor_topic = "sensor/doorsensor4";
char* bikeshedlock_topic = "sensor/locksensor4";
char* alarmstate_topic = "alarm/main/state";

boolean doors, prev_doors, voltagemonitors, frontdoor, prev_frontdoor, backdoor, prev_backdoor;
boolean sheddoor, prev_sheddoor, shedlock, bikesheddoor, prev_bikesheddoor, bikeshedlock;
boolean alarmstate;
boolean updatescreen = false, debounce = false;//, debouncewait = false;
#define DEBOUNCETIME_S 10

void startCameraServer();
const GFXfont* f;

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  Serial.println("init");
  pinMode(33, OUTPUT);
  digitalWrite(33, HIGH);
  digitalWrite(4, LOW);
  display.init();
  display.setRotation(3);

  // camera init
  init_camera();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  setup_wifi();
  if (!client.connected()) {
    reconnect();
  }
  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
  //updatescreen = true;
}

void loop() {
  if (!client.connected())
    reconnect();

  client.loop();

  if (debounce)
  {
    if (millis() > debouncestart + DEBOUNCETIME_S * 1000)
    {
      Serial.println("debounce time ended");
      debounce = false;

      if (prev_doors != doors || prev_backdoor != backdoor || prev_frontdoor != frontdoor || prev_sheddoor != sheddoor || prev_bikesheddoor != bikesheddoor)
      {
        updatescreen = true;
        Serial.println("prev_doorX != doorX dus update screen");
      }
      else
        Serial.println("prev_doorX = doorX dus geen update screen");
    }
  }
  if (updatescreen)
    displayvalues();
}

void displayvalues(void)
{
  Serial.println("update screen routine started");
  display.fillScreen(GxEPD_WHITE);
  f = &FreeSansBold12pt7b;  display.setFont(f);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(FIRSTTAB, FIRSTLINE);
  display.print("Deuren & Sloten:");

  f = &WHSymbolMono18pt7b;   display.setFont(f);
  if (doors)
  {
    display.drawChar(FIFTHTAB, FIRSTHALFLINE, MY_CHECKMARK, GxEPD_BLACK, 0, 2);
  }
  else
  {
    display.drawChar(FIFTHTAB, FIRSTHALFLINE, MY_CROSS, GxEPD_RED, 0, 2);
  }

  f = &FreeSansBold9pt7b;  display.setFont(f);

  display.setCursor(FIRSTTAB, FIFTHLINE);
  display.print("Spanning sensoren:");

  //display software version
  f = &FreeSans9pt7b;  display.setFont(f);
  display.setCursor(SEVENTHTAB, FIFTHLINE);
  display.print("v10");

  f = &WHSymbolMono18pt7b;   display.setFont(f);
  if (!voltagemonitors)
  {
    display.drawChar(FIFTHTAB, FIFTHLINE, MY_CHECKMARK, GxEPD_BLACK, 0, 1);
  }
  else
  {
    display.drawChar(FIFTHTAB, FIFTHLINE, MY_CROSS, GxEPD_RED, 0, 1);
  }

  //display separate doors and locks
  f = &FreeSansBold9pt7b;  display.setFont(f);
  display.setCursor(FIRSTTAB, SECONDLINE);
  display.print("V-deur:");
  display.setCursor(THIRDTAB, SECONDLINE);
  display.print("A-deur:");
  display.setCursor(FIRSTTAB, THIRDLINE);
  display.print("S-deur:");
  display.setCursor(THIRDTAB, THIRDLINE);
  display.print("S-slot:");
  display.setCursor(FIRSTTAB, FOURTHLINE);
  display.print("F-deur:");
  display.setCursor(THIRDTAB, FOURTHLINE);
  display.print("F-slot:");
  display.setCursor(THIRDTAB, FOURTHLINE);
  display.print("F-slot:");
  display.setCursor(FIFTHTAB + 10, THIRDLINE - 10);
  display.print("Alarm");


  f = &WHSymbolMono18pt7b;   display.setFont(f);
  display.drawRect(0, FIRSTLINE + 8, FIFTHTAB - 4, FOURTHLINE - FIRSTLINE, GxEPD_BLACK);
  if (frontdoor)
  {
    display.fillCircle(SECONDTAB, SECONDLINE - OFFSET, CIRCLESIZE, GxEPD_BLACK);
  }
  else
  {
    drawThickCircle(SECONDTAB, SECONDLINE - OFFSET, CIRCLESIZE, GxEPD_RED);
  }
  if (backdoor)
  {
    display.fillCircle(FOURTHTAB, SECONDLINE - OFFSET, CIRCLESIZE, GxEPD_BLACK);
  }
  else
  {
    drawThickCircle(FOURTHTAB, SECONDLINE - OFFSET, CIRCLESIZE, GxEPD_RED);
  }
  if (sheddoor)
  {
    display.fillCircle(SECONDTAB, THIRDLINE - OFFSET, CIRCLESIZE, GxEPD_BLACK);
  }
  else
  {
    drawThickCircle(SECONDTAB, THIRDLINE - OFFSET, CIRCLESIZE, GxEPD_RED);
  }
  if (shedlock)
  {
    display.fillCircle(FOURTHTAB, THIRDLINE - OFFSET, CIRCLESIZE, GxEPD_BLACK);
  }
  else
  {
    drawThickCircle(FOURTHTAB, THIRDLINE - OFFSET, CIRCLESIZE, GxEPD_RED);
  }
  if (bikesheddoor)
  {
    display.fillCircle(SECONDTAB, FOURTHLINE - OFFSET, CIRCLESIZE, GxEPD_BLACK);
  }
  else
  {
    drawThickCircle(SECONDTAB, FOURTHLINE - OFFSET, CIRCLESIZE, GxEPD_RED);
  }
  if (bikeshedlock)
  {
    display.fillCircle(FOURTHTAB, FOURTHLINE - OFFSET, CIRCLESIZE, GxEPD_BLACK);
  }
  else
  {
    drawThickCircle(FOURTHTAB, FOURTHLINE - OFFSET, CIRCLESIZE, GxEPD_RED);
  }

  display.drawRect(FIFTHTAB - OFFSET, SECONDLINE - 3, XMAX - FIFTHTAB, FIFTHLINE - THIRDLINE, GxEPD_BLACK);
  if (alarmstate)
  {
    display.fillCircle(SIXTHTAB, THIRDLINE + OFFSET, CIRCLESIZE, GxEPD_RED);
  }
  else
  {
    drawThickCircle(SIXTHTAB, THIRDLINE + OFFSET, CIRCLESIZE, GxEPD_BLACK);
  }
  display.update();
  updatescreen = false;
  digitalWrite(4, LOW);
}

void setup_wifi()
{
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(mySSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(mySSID, myPASSWORD);
  time1 = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(50);
    time2 = millis();
    if ((time2 - time1) > 1000 * WIFI_CONNECT_TIMEOUT_S) // wifi connection lasts too long
    {
      break;
      ESP.restart();
    }
    if (time1 > time2)
      time1 = 0;
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

boolean reconnect()
{
  if (WiFi.status() != WL_CONNECTED) {    // check if WiFi connection is present
    setup_wifi();
  }
  Serial.println("Attempting MQTT connection...");
  if (client.connect(mqtt_id)) {
    Serial.println("connected");
    // ... and resubscribe
    client.subscribe(voltagemonitors_topic);
    client.subscribe(doors_topic);

    client.subscribe(backdoor_topic);
    client.subscribe(frontdoor_topic);
    client.subscribe(sheddoor_topic);
    client.subscribe(shedlock_topic);
    client.subscribe(bikesheddoor_topic);
    client.subscribe(bikeshedlock_topic);
    client.subscribe(alarmstate_topic);
  }
  Serial.println(client.connected());
  return client.connected();
}
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // set the value of door and lock sensor
  if ((char)topic[7] == 'd')      // check for the "d" sensor/doorsensor#
  {
    if (!debounce)  // no recent change, start the debouncetimer for the doors
    {
      prev_doors = doors;
      prev_frontdoor = frontdoor;
      prev_backdoor = backdoor;
      prev_sheddoor = sheddoor;
      prev_bikesheddoor = bikesheddoor;
      debouncestart = millis();
      debounce = true;
      Serial.println("debounce started, debounce = true in callback");
    }
    if ((char)topic[12] == 'g')      // check for the "g" of sensor/doorsgroup
    {
      // set the value to true if CLOSED
      if ((char)payload[0] == 'C')
      {
        doors = true;
      }
      // set the value to false if OPEN
      else if ((char)payload[0] == 'O')
      {
        doors = false;
      }
    }
    if ((char)topic[17] == '1')      // check for the "1" of sensor/doorsensor1 (backdoor)
    {
      // set the value to true if CLOSED
      if ((char)payload[0] == 'C')
      {
        backdoor = true;
      }
      // set the value to false if OPEN
      else if ((char)payload[0] == 'O' || (char)payload[0] == 'N') // value can be NULL, when the door position is not determined
      {
        backdoor = false;
      }
    }
    if ((char)topic[17] == '2')      // check for the "1" of sensor/doorsensor2 (frontdoor)
    {
      // set the value to true if CLOSED
      if ((char)payload[0] == 'C')
      {
        frontdoor = true;
      }
      // set the value to false if OPEN
      else if ((char)payload[0] == 'O')
      {
        frontdoor = false;
      }
    }
    if ((char)topic[17] == '3')      // check for the "1" of sensor/doorsensor3 (sheddoor)
    {
      // set the value to true if CLOSED
      if ((char)payload[0] == 'C')
      {
        sheddoor = true;
      }
      // set the value to false if OPEN
      else if ((char)payload[0] == 'O')
      {
        sheddoor = false;
      }
    }
    if ((char)topic[17] == '4')      // check for the "1" of sensor/doorsensor4 (bikesheddoor)
    {
      // set the value to true if CLOSED
      if ((char)payload[0] == 'C')
      {
        bikesheddoor = true;
      }
      // set the value to false if OPEN
      else if ((char)payload[0] == 'O')
      {
        bikesheddoor = false;
      }
    }
  }

  if ((char)topic[7] == 'l')      // check for the "d" sensor/locksensor#
  {

    if ((char)topic[17] == '3')      // check for the "1" of sensor/locksensor3 (shedlock)
    {
      // set the value to true if CLOSED
      if ((char)payload[0] == 'C')
      {
        shedlock = true;
      }
      // set the value to false if OPEN
      else if ((char)payload[0] == 'O')
      {
        shedlock = false;
      }
    }
    if ((char)topic[17] == '4')      // check for the "1" of sensor/locksensor4 (bikeshedlock)
    {
      // set the value to true if CLOSED
      if ((char)payload[0] == 'C')
      {
        bikeshedlock = true;
      }
      // set the value to false if OPEN
      else if ((char)payload[0] == 'O')
      {
        bikeshedlock = false;
      }
    }
    updatescreen = true;
  }
  // set the value of voltagemonitors
  if ((char)topic[7] == 'v')      // check for the "v" sensor/voltagemonitors
  {
    // set the value to true if ON
    if ((char)payload[1] == 'N')
    {
      voltagemonitors = true;
    }
    // set the value to false if OFF
    if ((char)payload[1] == 'F')
    {
      voltagemonitors = false;
    }
    updatescreen = true;
  }
  // set the value of alarmstate
  if ((char)topic[0] == 'a')      // check for the "a" alarm/main/state
  {
    // set the value to true if ON
    if ((char)payload[1] == 'N')
    {
      alarmstate = true;
    }
    // set the value to false if OFF
    if ((char)payload[1] == 'F')
    {
      alarmstate = false;
    }
    updatescreen = true;
  }
  Serial.print("voltagemonitors: ");
  Serial.print(voltagemonitors);
  Serial.print("\tdoors: ");
  Serial.println(doors);
}

void drawThickCircle(int x, int y, int size, int color)
{
  for (int i = size; i > size - THICKNESS ; i--)
  {
    display.drawCircle(x, y, i, color);
  }
}

void init_camera(void) {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  //init with high specs to pre-allocate larger buffers
  if (psramFound()) {
    //Serial.println("PSRAM found");
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  /*
    //initial sensors are flipped vertically and colors are a bit saturated
    if (s->id.PID == OV3660_PID) {
     s->set_vflip(s, 1);//flip it back
     s->set_brightness(s, 1);//up the blightness just a bit
     s->set_saturation(s, -2);//lower the saturation
    }*/
  //drop down frame size for higher initial frame rate
  //s->set_framesize(s, FRAMESIZE_SVGA);
  s->set_framesize(s, FRAMESIZE_SXGA);
}
