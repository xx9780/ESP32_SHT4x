#include <HomeSpan.h>
#include <Adafruit_SHT4x.h>
#include <Preferences.h>
#include "nvs_flash.h"

#define STATUS_LED 2
#define BOOT_BUTTON 0
#define PRESS_DURATION 5000

Adafruit_SHT4x sht4 = Adafruit_SHT4x();
Preferences preferences;

float currentTemp = NAN;
float currentHum = NAN;
unsigned long lastRead = 0;

void readSHT4x() {
  if (millis() - lastRead > 2000) {
    sensors_event_t humidity, temp;
    sht4.getEvent(&humidity, &temp);
    if (!isnan(temp.temperature)) currentTemp = temp.temperature;
    if (!isnan(humidity.relative_humidity)) currentHum = humidity.relative_humidity;
    lastRead = millis();
  }
}

struct TempSensor : Service::TemperatureSensor {
  SpanCharacteristic *tempChar;

  TempSensor() {
    tempChar = new Characteristic::CurrentTemperature();
    tempChar->setRange(-40, 125);
  }

  void loop() override {
    readSHT4x();
    if (!isnan(currentTemp)) {
      tempChar->setVal(currentTemp);
    }
  }
};

struct HumSensor : Service::HumiditySensor {
  SpanCharacteristic *humChar;

  HumSensor() {
    humChar = new Characteristic::CurrentRelativeHumidity();
  }

  void loop() override {
    readSHT4x();
    if (!isnan(currentHum)) {
      humChar->setVal(currentHum);
    }
  }
};

struct MyInfo : Service::AccessoryInformation {
  MyInfo() {
    new Characteristic::Name("TempHumi Sensor");
    new Characteristic::Manufacturer("Xie BEI");
    new Characteristic::Model("HS-SHT4x");
    new Characteristic::SerialNumber("SN-00001");
    new Characteristic::FirmwareRevision("1.0");
    new Characteristic::Identify();
  }
};

void checkLongPressToResetConfig() {
  Serial.println("检测 BOOT 按键是否长按...");
  unsigned long pressedTime = 0;

  while (digitalRead(BOOT_BUTTON) == LOW) {
    if (pressedTime == 0) {
      pressedTime = millis();
      digitalWrite(STATUS_LED, HIGH);
    }
    if (millis() - pressedTime >= PRESS_DURATION) {
      Serial.println("⏱️ 检测到 BOOT 长按 5 秒，清除所有配置并重启进入 Web 配网模式");
      nvs_flash_erase();
      nvs_flash_init();
      delay(1000);
      ESP.restart();
    }
    delay(10);
  }

  digitalWrite(STATUS_LED, LOW);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(STATUS_LED, OUTPUT);
  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  digitalWrite(STATUS_LED, LOW);

  checkLongPressToResetConfig();

  Serial.println("初始化 SHT4x...");
  if (!sht4.begin()) {
    Serial.println("❌ 未找到 SHT4x 传感器！");
  } else {
    sht4.setPrecision(SHT4X_HIGH_PRECISION);
    sht4.setHeater(SHT4X_NO_HEATER);
    Serial.println("✅ SHT4x 初始化成功");
  }

  String accessoryName = "TH Sensor-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  homeSpan.enableAutoStartAP();
  homeSpan.begin(Category::Sensors, accessoryName.c_str());

  new SpanAccessory();
  new MyInfo();
  new TempSensor();
  new HumSensor();
}

void loop() {
  homeSpan.poll();
}
