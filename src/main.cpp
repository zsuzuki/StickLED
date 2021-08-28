#include <Arduino.h>
#include <FastLED.h>
#include <vector>
#include <M5GFX.h>
#include <I2C_BM8563.h>
#include <I2C_AXP192.h>
#include <WiFi.h>
#include <array>

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
  I2C_AXP192 axp(I2C_AXP192_DEFAULT_ADDRESS, Wire1);
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
  int lightIndex;
  uint8_t lightLevel;
  TimeType nowTime;
  Btn buttonA(37);
  Btn buttonB(39);
  std::array<uint8_t, 7> lvList = {255, 128, 64, 32, 16, 8, 0};

  struct ModeInfo
  {
    using function = void (*)();
    using renderFunc = void (*)(const ColorInfo &);
    const char *caption;
    function func;
    renderFunc render;
  };

  void changeLightLevel();
  void setupDateTime();
  void selectTimer();
  void renderMain(const ColorInfo &);
  void renderTimer(const ColorInfo &);
  std::vector<ModeInfo> modeList = {
      {"点灯", []()
       {
         colorIndex = (colorIndex + 1) % colorList.size();
       },
       renderMain},
      {"光量", changeLightLevel, renderMain},
      {"消灯", selectTimer, renderTimer},
      {"時刻", setupDateTime, renderMain},
  };
  int mode;

  int timerIndex;
  bool timerOn;
  constexpr int operator"" _MIN(unsigned long long n) { return n * 60; }
  constexpr int operator"" _HOUR(unsigned long long n) { return n * 60_MIN; }
  std::array<int, 6> timerList = {0, 5, 30_MIN, 1_HOUR, 1_HOUR + 30_MIN, 2_HOUR};
  TimeType timerTarget;

  //
  void selectTimer()
  {
    timerIndex = (timerIndex + 1) % timerList.size();
    int remainSec = timerList[timerIndex];
    if (remainSec == 0)
    {
      // timer off
      timerOn = false;
    }
    else
    {
      // timer on
      int sec = nowTime.seconds + remainSec;
      timerTarget.seconds = sec % 60;
      int min = nowTime.minutes + sec / 60;
      timerTarget.minutes = min % 60;
      int hour = nowTime.hours + min / 60;
      timerTarget.hours = hour % 24;
      timerOn = true;
      // Serial.printf("target: %02d:%02d.%02d\n", timerTarget.hours, timerTarget.minutes, timerTarget.seconds);
    }
  }

  //
  void renderTimer([[maybe_unused]] const ColorInfo &info)
  {
    for (size_t i = 0; i < timerList.size(); i++)
    {
      int color = i == timerIndex ? TFT_LIGHTGRAY : TFT_WHITE;
      display.fillRoundRect(10 + i * 20, 25, 18, 20, 2, color);
    }
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    if (timerOn)
    {
      char buff[64];
      snprintf(buff, sizeof(buff), "Tgt:%02d:%02d.%02d\n", timerTarget.hours, timerTarget.minutes, timerTarget.seconds);
      display.drawString(buff, 10, 50);
    }
    else
    {
      display.drawString("タイマー無し", 10, 50);
    }
  }

  //
  void checkTimer()
  {
    if (timerOn == false)
    {
      return;
    }

    bool off = false;
    if (timerTarget.hours < nowTime.hours)
      off = true;
    else if (timerTarget.hours == nowTime.hours)
    {
      if (timerTarget.minutes < nowTime.minutes)
        off = true;
      else if (timerTarget.minutes == nowTime.minutes)
      {
        if (timerTarget.seconds <= nowTime.seconds)
          off = true;
      }
    }
    if (off)
    {
      axp.powerOff();
    }
  }

  //
  void changeLightLevel()
  {
    lightIndex = (lightIndex + 1) % lvList.size();
    lightLevel = lvList[lightIndex];
    // Serial.printf("light level: %hhu\n", lightLevel);
  }

  //
  void renderMain(const ColorInfo &info)
  {
    display.fillRoundRect(20, 30, 120, 40, 8, info.color);
    display.fillRect(120, 30, 10, 40, TFT_BLACK);
    display.setTextColor(info.textColor, info.color);
    for (int i = lvList.size() - lightIndex - 1; i > 0; i--)
    {
      display.fillRect(121, 72 - i * 7, 8, 5, info.color);
    }
    // float length = (float)lightIndex / (float)(lvList.size() - 1) * 40.0f;
    // display.fillRect(121, 30, 8, length, TFT_BLACK);
    if (lightLevel > 0)
    {
      display.drawCenterString(info.caption, 80, 40);
    }
    else
    {
      display.drawCenterString("消灯", 80, 40);
    }
  }

  //
  //
  void dispBattery()
  {
    float bv = axp.getBatteryVoltage();
    constexpr float vLow = 3000.0f;
    constexpr float vHigh = 4000.0f;
    constexpr float vDan = (vHigh - vLow) * 0.15f + vLow;
    bool onCharge = axp.getBatteryDischargeCurrent() == 0.0f;
    int vCol = TFT_WHITE;
    if (onCharge)
      vCol = bv > vHigh ? TFT_GREEN : TFT_YELLOW;
    else if (bv < vDan)
      vCol = TFT_RED;
    bv = std::max(bv - vLow, 0.0f);
    bv = std::min(bv / (vHigh - vLow), 1.0f);
    auto vHeight = bv * 45;
    display.fillRoundRect(145, 75 - vHeight, 12, vHeight, 2, vCol);
  }

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
  setCpuFrequencyMhz(160);
  Serial.begin(115200);
  display.begin();
  display.setRotation(1);
  display.setFont(&fonts::lgfxJapanGothic_20);

  Wire1.begin(BM8563_I2C_SDA, BM8563_I2C_SCL);
  rtc.begin();
  rtc.getTime(&nowTime);

  I2C_AXP192_InitDef axpInitDef{};
  axpInitDef.EXTEN = true;
  axpInitDef.BACKUP = true;
  axpInitDef.DCDC1 = 3300;
  axpInitDef.DCDC2 = 0;
  axpInitDef.DCDC3 = 0;
  axpInitDef.LDO2 = 3000;
  axpInitDef.LDO3 = 3000;
  axpInitDef.GPIO0 = 2800;
  axpInitDef.GPIO1 = -1;
  axpInitDef.GPIO2 = -1;
  axpInitDef.GPIO3 = -1;
  axpInitDef.GPIO4 = -1;
  axp.begin(axpInitDef);

  FastLED.addLeds<WS2812B, DIN, GRB>(leds, NB_LED).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(100);

  colorIndex = 0;
  for (auto &led : leds)
  {
    led = colorList[colorIndex].ledColor;
  }
  mode = 0;
  lightLevel = 255;
  timerIndex = 0;
  timerOn = false;

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
  if (lightLevel < 255)
  {
    leds[0].nscale8(lightLevel);
  }
  for (int i = NB_LED - 1; i > 0; i--)
  {
    leds[i] = leds[i - 1];
  }

  display.startWrite();
  if (needUpdate)
  {
    display.fillScreen(TFT_BLACK);
    modeList[mode].render(info);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.drawString(modeList[mode].caption, 98, 0);
    needUpdate = false;
  }
  dispBattery();
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
  checkTimer();
}
