# ESP32-SeniorHome

[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.1.2-green)](https://github.com/espressif/esp-idf)
[![Platform](https://img.shields.io/badge/Platform-ESP32--C3%20%7C%20ESP32--S3-orange)](https://www.espressif.com/)

> 基于 ESP32-C3 和 ESP32-S3 的物联网智慧养老健康监测系统，实现多设备联动、实时环境监测与远程智能控制。

## 🌟 项目简介

ESP32-SeniorHome 是一款专为老年居家健康监测设计的物联网系统，采用 ESP32-C3 作为主网关，ESP32-S3 作为显示终端，通过 MQTT 协议实现多设备协同工作。系统集成环境监测、人体存在检测、睡眠质量分析、智能照明等功能，为老年人提供安全、舒适的居家环境。

## ✨ 功能特性

### 🏠 环境监测
- **温湿度监测**：基于 DHT11 传感器实时采集室内温度和湿度数据
- **光照强度检测**：采用 GY-30 数字光强传感器，自动判断室内亮度
- **有害气体检测**：MICS-5524 气体传感器监测空气质量

### 👤 人体感知
- **毫米波雷达检测**：LD2450 24GHz 雷达实现非接触式人体存在检测
- **微动分析**：基于雷达数据的微动特征提取，实现精准的存在识别
- **睡眠状态监测**：通过雷达微动频率分析，判断睡眠深浅状态

### 💡 智能控制
- **自动照明**：人体存在 + 光照强度双重条件触发智能开关灯
- **远程控制**：支持 MQTT 协议远程控制设备状态
- **本地交互**：ESP32-S3 LCD 屏实时显示数据，按键快捷操作

### 📊 数据可视化
- **Web Dashboard**：基于 HTML5 + Chart.js 的实时数据可视化界面
- **数据存储**：JSON 格式数据序列化，便于云端存储与分析
- **历史趋势**：支持查看温湿度、光照等数据的变化趋势

### 🔧 系统特性
- **模块化设计**：基于 FreeRTOS 的任务调度，各功能模块解耦
- **异常处理**：完善的传感器异常检测与自动恢复机制
- **低功耗设计**：支持 ESP32 深度睡眠模式，延长电池寿命

## 📐 系统架构

![系统架构图](https://github.com/user-attachments/assets/b21a2ea5-0859-4f2b-bc8b-a6b4ba5b37b3)

## 🛠️ 技术栈

| 分类 | 技术 | 版本 |
|------|------|------|
| 主控芯片 | ESP32-C3 / ESP32-S3 | - |
| 开发框架 | ESP-IDF | v5.1.2 |
| 操作系统 | FreeRTOS | - |
| 通信协议 | WiFi / MQTT | 802.11 b/g/n / 3.1.1 |
| Web 前端 | HTML5 / CSS3 / JavaScript | - |
| 图表库 | Chart.js | v3.x |
| MQTT 客户端 | MQTT.js | v4.x |

## 🚀 快速开始

### 环境要求

- ESP-IDF v5.1.2 或更高版本
- Python 3.8+
- Git
- 串口驱动（CH340 / CP210x）

### 克隆项目

```bash
git clone https://github.com/laomao2091651092-a11y/esp32-senior-home-monitor.git
cd esp32-senior-home-monitor
```

### 编译烧录 ESP32-C3 主网关

```bash
# 设置目标芯片
idf.py set-target esp32c3

# 配置项目（设置 WiFi 等参数）
idf.py menuconfig

# 编译
idf.py build

# 烧录到设备（替换 COMxx 为你的串口）
idf.py -p COMxx flash

# 启动串口监视器
idf.py -p COMxx monitor
```

### 编译烧录 ESP32-S3 显示终端

```bash
cd s3_link_device

# 设置目标芯片
idf.py set-target esp32s3

# 编译
idf.py build

# 烧录到设备
idf.py -p COMxx flash monitor
```

### 运行 Web 可视化

```bash
# 使用浏览器打开 dashboard.html
# 或使用本地服务器（推荐）
python -m http.server 8080
# 然后访问 http://localhost:8080/dashboard.html
```

## 📡 MQTT 协议

### 主题定义

| 主题名称 | 方向 | 说明 |
|----------|------|------|
| `2023108380254_linzijie_evt` | 上行 | 传感器数据上报（JSON 格式） |
| `2023108380254_linzijie_cmd` | 下行 | 控制命令（1=开灯，0=关灯） |
| `2023108380254_linzijie_alarm` | 上行 | 告警消息 |

### 数据格式

**传感器数据 (evt)**：
```json
{
  "timestamp": 1699999999,
  "device_id": "ESP32_SeniorHome_001",
  "sensors": {
    "temperature": 27.5,
    "humidity": 59.0,
    "light": 238.3,
    "gas": 1
  },
  "radar": {
    "presence": true,
    "moving_target_distance": 150,
    "static_target_distance": 145,
    "sleep_state": "deep"
  }
}
```

## 📁 项目结构

```
esp32-senior-home-monitor/
├── main/                    # ESP32-C3 主网关代码
│   ├── app_main.c           # 应用入口
│   ├── bsp.c / bsp.h        # 板级支持包
│   ├── sensor_data_mgr.c/h  # 传感器数据管理与 JSON 序列化
│   ├── task_app_wifi_manage.c/h  # WiFi 管理任务
│   ├── task_app_mqtt.c/h    # MQTT 通信任务
│   ├── task_app_sensor.c/h  # 传感器采集任务
│   ├── task_app_radar.c/h   # 雷达数据处理任务
│   ├── task_app_cmd.c/h     # 命令处理任务
│   ├── task_app_btn.c/h     # 按键处理任务
│   ├── task_monitor.c/h     # 任务监控与状态报告
│   ├── 10_ld2450.c/h        # LD2450 雷达驱动
│   └── CMakeLists.txt       # 主程序 CMake 配置
├── s3_link_device/          # ESP32-S3 显示终端
│   ├── main/                # 主程序
│   │   ├── app_main.c       # 应用入口
│   │   ├── s3_wifi.c/h      # WiFi 管理
│   │   ├── s3_mqtt.c/h      # MQTT 通信
│   │   ├── s3_display.c/h   # LCD 显示
│   │   └── s3_buttons.c/h   # 按键处理
│   └── components/          # 硬件组件
│       ├── LCD/             # LCD 驱动
│       ├── LED/             # LED 驱动
│       ├── BEEP/            # 蜂鸣器驱动
│       ├── KEY/             # 按键驱动
│       └── SPI/             # SPI 驱动
├── dashboard.html           # Web 可视化界面
├── CMakeLists.txt           # 项目根 CMake 配置
├── sdkconfig                # ESP-IDF 配置文件
├── sdkconfig.defaults       # 默认配置
├── .gitignore               # Git 忽略文件
└── README.md                # 项目说明文档
```

## 🔌 硬件接线

### ESP32-C3 主网关

| 模块 | 引脚 | 说明 |
|------|------|------|
| DHT11 | GPIO3 | 数字信号 |
| GY-30 | SDA=GPIO1, SCL=GPIO2 | I2C 通信 |
| MICS5524 | GPIO10 | 模拟信号输入 |
| LD2450 | TX=GPIO5, RX=GPIO4 | UART 通信 (9600bps) |
| RGB LED | GPIO8 | PWM 输出 |
| BOOT 按键 | GPIO9 | 输入 |

### ESP32-S3 显示终端

| 模块 | 引脚 | 说明 |
|------|------|------|
| LCD (ST7789) | SPI 接口 | 240x240 分辨率 |
| BEEP | GPIOxx | 蜂鸣器控制 |
| KEY1 | GPIOxx | 按键1 |
| KEY2 | GPIOxx | 按键2 |

## 🤝 贡献指南

欢迎贡献代码！请遵循以下步骤：

1. Fork 本仓库
2. 创建特性分支：`git checkout -b feature/your-feature`
3. 提交更改：`git commit -m 'Add some feature'`
4. 推送到分支：`git push origin feature/your-feature`
5. 创建 Pull Request

## 📄 许可证

本项目采用 MIT 许可证 - 查看 [LICENSE](LICENSE) 文件了解详情。

## 🙋‍♂️ 作者

**Lin Zijie** - 电子信息工程专业学生

- 📧 Email: linzijie@example.com
- 🏫 学校: 某大学

---

⭐ 如果这个项目对你有帮助，请给个 Star！
