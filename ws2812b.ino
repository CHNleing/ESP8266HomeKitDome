/*
   WS2812B.ino

   This accessory is based on a colored LED bulb made from ESP8266-01S and WS2812B light strip.
   Setup code: 336-85-553
   WS2812B (DATA) Connect to GPIO3 of ESP-01S.
   Button to connect GPIO0 of ESP-01S
    Created on: 2020-03-22
        Author: Leing
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <arduino_homekit_server.h>
#include <Adafruit_NeoPixel.h>



#include "ButtonDebounce.h"
#include "ButtonHandler.h"

#define PL(s) Serial.println(s)
#define P(s) Serial.print(s)



#define PIN_RGB 3 //WS2812B控制引脚 / WS2812B control GPIO
#define NUMPIXELS 10  //WS2812B的LED数量 / WS2812B LED quantity
#define PIN_ButtonDebounce 0  //按钮控制引脚 / Button control GPIO
#define PIN_LED 2 //系统指示灯(板载勿动) /System indicator(Please do not modify)

const char *ssid = "you ssid";  //输入你的无线名称 / you wifi ssid
const char *password = "you password";  //wifi密码 / you wifi password


int led_r = 0;
int led_g = 0;
int led_b = 0;

Adafruit_NeoPixel pixels(NUMPIXELS, PIN_RGB, NEO_GRB);
void blink_led(int interval, int count) {
  for (int i = 0; i < count; i++) {
    builtinledSetStatus(true);
    delay(interval);
    builtinledSetStatus(false);
    delay(interval);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setRxBufferSize(32);
  Serial.setDebugOutput(false);
  pixels.begin();
  pixels.clear();
  pixels.show();
  pinMode(PIN_LED, OUTPUT);
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.disconnect(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    blink_led(500, 1);
  }
  printf("\n");
  printf("SketchSize: %d B\n", ESP.getSketchSize());
  printf("FreeSketchSpace: %d B\n", ESP.getFreeSketchSpace());
  printf("FlashChipSize: %d B\n", ESP.getFlashChipSize());
  printf("FlashChipRealSize: %d B\n", ESP.getFlashChipRealSize());
  printf("FlashChipSpeed: %d\n", ESP.getFlashChipSpeed());
  printf("SdkVersion: %s\n", ESP.getSdkVersion());
  printf("FullVersion: %s\n", ESP.getFullVersion().c_str());
  printf("CpuFreq: %dMHz\n", ESP.getCpuFreqMHz());
  printf("FreeHeap: %d B\n", ESP.getFreeHeap());
  printf("ResetInfo: %s\n", ESP.getResetInfo().c_str());
  printf("ResetReason: %s\n", ESP.getResetReason().c_str());
  DEBUG_HEAP();
  homekit_setup();
  DEBUG_HEAP();
  blink_led(200, 6);

}

void loop() {
  homekit_loop();
  while (WiFi.status() != WL_CONNECTED) {
    blink_led(500, 1);
  }
}

void builtinledSetStatus(bool on) {
  digitalWrite(PIN_LED, on ? LOW : HIGH);
}

//==============================
// Homekit setup and loop
//==============================

extern "C" homekit_server_config_t config;
extern "C" homekit_characteristic_t name;
//extern "C" void occupancy_toggle();
extern "C" void led_toggle();
extern "C" void accessory_init();
extern "C" void led_update();

extern "C" int led_bri;
extern "C" bool led_power;
extern "C" bool is_led_updata;
extern "C" float led_hue;
extern "C" float led_saturation;


ButtonDebounce btn(PIN_ButtonDebounce, INPUT_PULLUP, LOW);
ButtonHandler btnHandler;

void IRAM_ATTR btnInterrupt() {
  btn.update();
}

uint32_t next_heap_millis = 0;

void homekit_setup() {
  //accessory_init();
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.macAddress(mac);
  int name_len = snprintf(NULL, 0, "%s_%02X%02X%02X",
                          name.value.string_value, mac[3], mac[4], mac[5]);
  char *name_value = (char*)malloc(name_len + 1);
  snprintf(name_value, name_len + 1, "%s_%02X%02X%02X",
           name.value.string_value, mac[3], mac[4], mac[5]);
  name.value = HOMEKIT_STRING_CPP(name_value);

  arduino_homekit_setup(&config);

  btn.setCallback([](const bool down) {
    btnHandler.handleChange(down);
  });
  btn.setInterrupt(btnInterrupt);

  btnHandler.setIsDownFunction([](void) {
    return btn.checkIsDown();
  });
  btnHandler.setCallback([](const button_event e) {
    P(F("Button Event: "));
    switch (e) {
      case BUTTON_EVENT_SINGLECLICK:
        PL(F("SINGLECLICK"));
        led_toggle();
        break;
      case BUTTON_EVENT_DOUBLECLICK:
        PL(F("DOUBLECLICK"));
        //occupancy_toggle();
        break;
      case BUTTON_EVENT_LONGCLICK:
        PL(F("LONGCLICK"));
        homekit_storage_reset();
        system_restart();
        break;
    }
  } );
}
void hsi2rgb(float h, float s, float i) {
  int r, g, b;

  while (h < 0) {
    h += 360.0F;
  };     // cycle h around to 0-360 degrees
  while (h >= 360) {
    h -= 360.0F;
  };
  h = 3.14159F * h / 180.0F;          // convert to radians.
  s /= 100.0F;                        // from percentage to ratio
  i /= 100.0F;                        // from percentage to ratio
  s = s > 0 ? (s < 1 ? s : 1) : 0;    // clamp s and i to interval [0,1]
  i = i > 0 ? (i < 1 ? i : 1) : 0;    // clamp s and i to interval [0,1]
  i = i * sqrt(i);                    // shape intensity to have finer granularity near 0

  if (h < 2.09439) {
    r = 255 * i / 3 * (1 + s * cos(h) / cos(1.047196667 - h));
    g = 255 * i / 3 * (1 + s * (1 - cos(h) / cos(1.047196667 - h)));
    b = 255 * i / 3 * (1 - s);
  }
  else if (h < 4.188787) {
    h = h - 2.09439;
    g = 255 * i / 3 * (1 + s * cos(h) / cos(1.047196667 - h));
    b = 255 * i / 3 * (1 + s * (1 - cos(h) / cos(1.047196667 - h)));
    r = 255 * i / 3 * (1 - s);
  }
  else {
    h = h - 4.188787;
    b = 255 * i / 3 * (1 + s * cos(h) / cos(1.047196667 - h));
    r = 255 * i / 3 * (1 + s * (1 - cos(h) / cos(1.047196667 - h)));
    g = 255 * i / 3 * (1 - s);
  }

  led_r =  r;
  led_g =  g;
  led_b =  b;    //(uint8_t) b;
  //rgb->white = (uint8_t) 0;           // white channel is not used
}

void homekit_loop() {

  if (is_led_updata) {
    P("颜色 = ");
    PL(led_hue);
    P("亮度 = ");
    PL(led_bri);
    P("饱和度 = ");
    PL(led_saturation);

    if (led_power) {
      hsi2rgb(led_hue, led_saturation, led_bri);
      pixels.fill(pixels.Color(led_r, led_g, led_b));
      //pixels.setBrightness(int(led_bri*2.55));
    } else {
      pixels.clear();
    }
    pixels.show();
    is_led_updata = false;
  }


  btnHandler.loop();
  arduino_homekit_loop();
  uint32_t time = millis();
  if (time > next_heap_millis) {
    INFO("heap: %d, sockets: %d",
         ESP.getFreeHeap(), arduino_homekit_connected_clients_count());
    next_heap_millis = time + 5000;
  }
  led_update();
}
