#include <HomeSpan.h>                  // 引入 HomeSpan 主库，用于 HomeKit 接入
#include <Adafruit_SHT4x.h>            // 引入 Adafruit SHT4x 温湿度传感器库
#include <nvs_flash.h>                 // 原始 NVS API，用于擦除整个 NVS 分区
#include <Preferences.h>               // 简易 NVS 操作类，存储是否需要擦除标志

#define STATUS_LED 2                   // 指定板载 LED 的 GPIO（可根据开发板调整）
#define BOOT_BUTTON 0                  // ESP32-S3 的 BOOT 键对应 GPIO 0
#define PRESS_DURATION 5000            // 按键长按持续时间设定为 5 秒（单位毫秒）
#define NVS_FLAG_NAMESPACE "bootflag"  // 用于记录“是否需要擦除”的 NVS 命名空间
#define NVS_FLAG_KEY       "clear"     

// --------- SHT4x 传感器变量初始化 ---------
Adafruit_SHT4x sht4 = Adafruit_SHT4x();  // 创建 SHT4x 传感器实例
float currentTemp = NAN;                 // 当前温度变量（初始为非数字）
float currentHum = NAN;                  // 当前湿度变量（初始为非数字）
unsigned long lastRead = 0;              // 上一次读取传感器的时间戳

// 每隔 2 秒读取一次传感器
void readSHT4x() {
  if (millis() - lastRead > 2000) {
    sensors_event_t humidity, temp;     // 创建事件对象
    sht4.getEvent(&humidity, &temp);    // 读取传感器数据
    if (!isnan(temp.temperature)) currentTemp = temp.temperature;  // 如果有效则更新温度
    if (!isnan(humidity.relative_humidity)) currentHum = humidity.relative_humidity;  // 更新湿度
    lastRead = millis();                // 记录读取时间
  }
}

// --------- 温度传感器服务类 ---------
struct TempSensor : Service::TemperatureSensor {
  SpanCharacteristic *tempChar;

  TempSensor() {
    tempChar = new Characteristic::CurrentTemperature();  // 创建温度特征值
    tempChar->setRange(-40, 125);                         // 设置温度测量范围
  }

  void loop() override {
    readSHT4x();                                          // 读取温湿度
    if (!isnan(currentTemp)) {
      tempChar->setVal(currentTemp);                      // 更新 HomeKit 温度值
    }
  }
};

// --------- 湿度传感器服务类 ---------
struct HumSensor : Service::HumiditySensor {
  SpanCharacteristic *humChar;

  HumSensor() {
    humChar = new Characteristic::CurrentRelativeHumidity();  // 创建湿度特征值
  }

  void loop() override {
    readSHT4x();                                            // 读取温湿度
    if (!isnan(currentHum)) {
      humChar->setVal(currentHum);                          // 更新 HomeKit 湿度值
    }
  }
};

// --------- HomeKit 附件信息 ---------
struct MyInfo : Service::AccessoryInformation {
  MyInfo() {
    new Characteristic::Name("TH Sensor");                // 附件名称
    new Characteristic::Manufacturer("xx9780");          // 制造商
    new Characteristic::Model("ESP32-SHT40");             // 型号
    new Characteristic::SerialNumber("SN-00001");         // 序列号
    new Characteristic::FirmwareRevision("1.0");          // 固件版本
    new Characteristic::Identify();                       
  }
};

// --------- 检查是否 BOOT 长按，触发清除 NVS ---------
void checkLongPressToEnterWebPortal() {
  pinMode(BOOT_BUTTON, INPUT_PULLUP);                      // 设置 BOOT 按键为上拉输入
  unsigned long pressedTime = 0;

  while (digitalRead(BOOT_BUTTON) == LOW) {                // 如果按下
    if (pressedTime == 0) {
      pressedTime = millis();                              // 记录第一次按下时间
      digitalWrite(STATUS_LED, HIGH);                      // 点亮 LED 指示长按中
    }
    if (millis() - pressedTime >= PRESS_DURATION) {
      Serial.println("⏱️ 检测到 BOOT 长按 5 秒，清除所有配置并重启进入 Web 配网模式");

      Preferences prefs;
      prefs.begin(NVS_FLAG_NAMESPACE, false);              // 打开 NVS 命名空间
      prefs.putBool(NVS_FLAG_KEY, true);                   // 写入“需要擦除”标志
      prefs.end();

      delay(1000);
      ESP.restart();                                       // 重启设备
    }
    delay(10);                                              // 防抖与节省 CPU
  }

  digitalWrite(STATUS_LED, LOW);                           // 松开按键，熄灭 LED
}

// --------- 执行擦除所有 NVS 数据的操作 ---------
void clearAllNVS() {
  Serial.println("🧹 正在擦除所有 NVS 存储...");
  nvs_flash_erase();                                       // 擦除整个 NVS 区域
  nvs_flash_init();                                        // 重新初始化 NVS

  Preferences prefs;
  prefs.begin(NVS_FLAG_NAMESPACE, false);                  // 打开命名空间
  prefs.putBool(NVS_FLAG_KEY, false);                      // 清除“需要擦除”标志
  prefs.end();

  delay(1000);
  Serial.println("✅ 擦除完成，继续启动...");
}

void setup() {
  Serial.begin(115200);                                    // 初始化串口
  delay(500);                                              // 稳定时间

  pinMode(STATUS_LED, OUTPUT);                             // 设置 LED 为输出
  digitalWrite(STATUS_LED, LOW);                           // 初始熄灭

  // 检查 NVS 标志位，看是否需要擦除配置
  Preferences prefs;
  prefs.begin(NVS_FLAG_NAMESPACE, true);                   // 只读方式读取
  if (prefs.getBool(NVS_FLAG_KEY, false)) {
    clearAllNVS();                                        
  }
  prefs.end();

  checkLongPressToEnterWebPortal();                        // 检查是否按键触发擦除

  // 初始化 SHT4x 传感器
  if (!sht4.begin()) {
    Serial.println("❌ 未找到 SHT4x 传感器！");
  } else {
    sht4.setPrecision(SHT4X_HIGH_PRECISION);               // 设置高精度模式
    sht4.setHeater(SHT4X_NO_HEATER);                       // 禁用加热器
    Serial.println("✅ SHT4x 初始化成功");
  }

  // 初始化 HomeSpan
  String accessoryName = "TH Sensor-" + String((uint32_t)ESP.getEfuseMac(), HEX);  // 根据芯片唯一地址生成名称
  homeSpan.setPairingCode("46637726");                      // 设置配对码根据HomeSpan官方文档设置为4663-7726，启用 Web Portal
  homeSpan.enableAutoStartAP();                             // 启用自动启动热点
  homeSpan.begin(Category::Sensors, accessoryName.c_str()); // 设置类别与名称

  new SpanAccessory();                                      // 创建 HomeKit 附件
  new MyInfo();                                             // 添加基本信息服务
  new TempSensor();                                         // 添加温度服务
  new HumSensor();                                          // 添加湿度服务
}

void loop() {
  homeSpan.poll();                                          // HomeKit 主轮询
}
