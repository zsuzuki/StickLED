#include <Arduino.h>
#include <FastLED.h>
#include <vector>
#include <M5GFX.h>
#include <I2C_BM8563.h>
#include <WiFi.h>

namespace
{
  constexpr int BM8563_I2C_SDA = 21;
  constexpr int BM8563_I2C_SCL = 22;
  constexpr int DIN = 26;
  constexpr int NB_LED = 12;

  using DateType = I2C_BM8563_DateTypeDef;
  using TimeType = I2C_BM8563_TimeTypeDef;

  M5GFX display;
  I2C_BM8563 rtc(I2C_BM8563_DEFAULT_ADDRESS, Wire1);
  CRGB leds[NB_LED];

  class Btn
  {
    int pin;
    bool on;

  public:
    Btn(int p) : pin(p) {}
    void init()
    {
      pinMode(pin, INPUT_PULLUP);
    }
    void update()
    {
      on = !digitalRead(pin);
    }
    bool get() const { return on; }
  };

  struct ColorInfo
  {
    const char *caption;
    CRGB ledColor;
    int color;
    int textColor;
  };

  std::vector<ColorInfo> colorList = {
      {"赤", CRGB::Red, TFT_RED, TFT_WHITE},
      {"青", CRGB::Blue, TFT_BLUE, TFT_WHITE},
      {"緑", CRGB::Green, TFT_GREEN, TFT_BLACK},
      {"オレンジ", CRGB::Orange, TFT_ORANGE, TFT_WHITE},
      {"紫", CRGB::Purple, TFT_PURPLE, TFT_WHITE},
      {"黄", CRGB::Yellow, TFT_YELLOW, TFT_BLACK},
      {"白", CRGB::White, TFT_WHITE, TFT_BLACK},
      {"オリーブ", CRGB::Olive, TFT_OLIVE, TFT_WHITE},
      {"新緑", CRGB::ForestGreen, TFT_GREENYELLOW, TFT_BLACK},
  };

  int colorIndex;
  TimeType nowTime;
  Btn buttonA(37);
  Btn buttonB(39);

  struct ModeInfo
  {
    using function = void (*)();
    const char *caption;
    function func;
  };

  void setupDateTime();
  std::vector<ModeInfo> modeList = {
      {"点灯", []()
       {
         colorIndex = (colorIndex + 1) % colorList.size();
       }},
      {"時刻", []()
       { setupDateTime(); }},
  };
  int mode;

  //
  void setupDateTime()
  {
    const char *ssid = "********";
    const char *password = "********";
    const char *ntpServer = "ntp.jst.mfeed.ad.jp";

    WiFi.begin(ssid, password);
    Serial.printf("Wifi[%s] setup:", ssid);
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
    }
    Serial.println("done.");

    // get time from NTP server(UTC+9)
    configTime(9 * 3600, 0, ntpServer);

    // Get local time
    struct tm timeInfo;
    if (getLocalTime(&timeInfo))
    {
      // Set RTC time
      TimeType TimeStruct;
      TimeStruct.hours = timeInfo.tm_hour;
      TimeStruct.minutes = timeInfo.tm_min;
      TimeStruct.seconds = timeInfo.tm_sec;
      rtc.setTime(&TimeStruct);

      DateType DateStruct;
      DateStruct.weekDay = timeInfo.tm_wday;
      DateStruct.month = timeInfo.tm_mon + 1;
      DateStruct.date = timeInfo.tm_mday;
      DateStruct.year = timeInfo.tm_year + 1900;
      rtc.setDate(&DateStruct);
      Serial.println("time setting success");
    }

    //disconnect WiFi
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("time setting done.");
  }

}

void setup()
{
  display.begin();
  display.setRotation(1);
  display.setFont(&fonts::lgfxJapanGothic_20);

  Wire1.begin(BM8563_I2C_SDA, BM8563_I2C_SCL);
  rtc.begin();
  rtc.getTime(&nowTime);

  FastLED.addLeds<WS2812B, DIN, GRB>(leds, NB_LED).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(10);

  colorIndex = 0;
  for (auto &led : leds)
  {
    led = colorList[colorIndex].ledColor;
  }
  mode = 0;

  buttonA.init();
  buttonB.init();

  display.startWrite();
  display.fillScreen(TFT_BLACK);
  display.endWrite();
}

void loop()
{
  static bool needUpdate = true;

  buttonA.update();
  buttonB.update();

  if (buttonA.get())
  {
    modeList[mode].func();
    needUpdate = true;
  }
  if (buttonB.get())
  {
    mode = (mode + 1) % modeList.size();
    needUpdate = true;
  }

  const auto &info = colorList[colorIndex];
  leds[0] = info.ledColor;
  leds[0] /= 2;
  for (int i = NB_LED - 1; i > 0; i--)
  {
    leds[i] = leds[i - 1];
  }

  display.startWrite();
  if (needUpdate)
  {
    display.fillScreen(TFT_BLACK);
    display.fillRoundRect(20, 30, 120, 40, 8, info.color);
    display.setTextColor(info.textColor, info.color);
    display.drawCenterString(info.caption, 80, 40);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.drawString(modeList[mode].caption, 100, 0);
    needUpdate = false;
  }
  {
    char buff[32];
    rtc.getTime(&nowTime);
    snprintf(buff, sizeof(buff), "%02d:%02d.%02d", nowTime.hours, nowTime.minutes, nowTime.seconds);
    display.setTextColor(TFT_WHITE, TFT_DARKGRAY);
    display.drawString(buff, 5, 0);
  }

  display.endWrite();

  FastLED.show();
  FastLED.delay(200);
}
