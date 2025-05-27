# ESP32_SHT4x
使用ESP32-S3与SHT4x制作可以接入HomeKit的温湿度传感器

由于现在市面上的温湿度传感器都必须配备一个蓝牙网关，出于能省则省的原则，制作了可以直接连接HomeKit的温湿度传感器。
## 功能特性

- 原生 HomeKit 支持
- BOOT 长按 5 秒进入 Web 配网
- 温度 + 湿度传感
- 自动生成热点 Web 配网
- 基于 Adafruit SHT4x 库

## 接线说明（默认 I2C）

|  SHT40 引脚    | ESP32S3 引脚 |
|----------------|-------------|
| VCC            | 3.3V        |
| GND            | GND         |
| SDA            | GPIO 8      |
| SCL            | GPIO 9      |

## 使用说明

1. 使用Arduino IDE,HomeSpan v2.1.2
2. 首次启动或长按 BOOT 5 秒进入热点模式
3. 使用手机连接 Wi-Fi 热点并访问 192.168.4.1 配置 Wi-Fi

## 许可证

MIT License
