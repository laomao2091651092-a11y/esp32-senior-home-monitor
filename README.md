# ESP32 银发智慧多设备健康检测系统

> 基于 ESP32-C3 和 ESP32-S3 的物联网智慧养老监测系统

## 功能特性

- 🌡️ **环境监测**：温度、湿度、光照强度、有害气体检测
- 👤 **人体存在检测**：LD2450 毫米波雷达非接触式监测
- 😴 **睡眠质量监测**：基于雷达微动分析的睡眠状态识别
- 💡 **智能照明**：人体感应 + 光照自动控制
- 📺 **本地显示**：ESP32-S3 LCD 屏实时数据展示
- 📱 **远程监控**：Web 可视化界面 + MQTT 远程控制

## 硬件架构

```
┌───────────────────────────────────────┐
│         ESP32-C3 主网关               │
│  ┌─────────┐  ┌─────────┐  ┌───────┐ │
│  │  DHT11  │  │  GY-30  │  │MICS5524│ │
│  │ 温湿度   │  │ 光照    │  │气体检测│ │
│  └─────────┘  └─────────┘  └───────┘ │
│  ┌─────────┐  ┌─────────┐  ┌───────┐ │
│  │ LD2450  │  │  RGB    │  │ BOOT  │ │
│  │ 雷达    │  │  LED    │  │ 按键   │ │
│  └─────────┘  └─────────┘  └───────┘ │
└──────────────┬────────────────────────┘
               │ WiFi + MQTT
┌──────────────▼────────────────────────┐
│         ESP32-S3 显示终端             │
│  ┌─────────┐  ┌─────────┐  ┌───────┐ │
│  │  LCD    │  │  BEEP   │  │ KEY1/2│ │
│  │ 240x240 │  │ 蜂鸣器   │  │ 按键   │ │
│  └─────────┘  └─────────┘  └───────┘ │
└───────────────────────────────────────┘
```

## 技术栈

- **主控芯片**：ESP32-C3 / ESP32-S3
- **开发框架**：ESP-IDF v5.1.2
- **通信协议**：WiFi 802.11 b/g/n + MQTT 3.1.1
- **Web可视化**：HTML5 + Chart.js + MQTT.js
- **操作系统**：FreeRTOS

## 快速开始

### 环境要求

- ESP-IDF v5.1.2 或更高版本
- Python 3.8+
- Git

### 编译烧录

```bash
# 编译 ESP32-C3 主网关
cd esp32-senior-home-monitor
idf.py set-target esp32c3
idf.py build
idf.py -p COMxx flash monitor

# 编译 ESP32-S3 显示终端
cd s3_link_device
idf.py set-target esp32s3
idf.py build
idf.py -p COMxx flash monitor
```

### 配置 WiFi

修改 `sdkconfig` 文件或使用 `idf.py menuconfig` 设置 WiFi SSID 和密码。

### Web 可视化

打开 `dashboard.html` 在浏览器中查看实时数据。

## MQTT 主题

| 主题 | 说明 |
|------|------|
| `2023108380254_linzijie_evt` | 传感器数据上报 |
| `2023108380254_linzijie_cmd` | 控制命令 |
| `2023108380254_linzijie_alarm` | 告警消息 |

## 项目结构

```
esp32-senior-home-monitor/
├── main/                    # ESP32-C3 主程序
│   ├── app_main.c           # 主入口
│   ├── bsp.c                # 板级支持包
│   ├── sensor_data_mgr.c    # 传感器数据管理
│   ├── task_app_sensor.c    # 传感器采集任务
│   ├── task_app_mqtt.c      # MQTT 通信任务
│   ├── task_app_radar.c     # 雷达处理任务
│   └── ...
├── s3_link_device/          # ESP32-S3 显示终端
│   ├── main/                # 主程序
│   └── components/          # 组件（LCD/BEEP/KEY/LED/SPI）
├── dashboard.html           # Web 可视化页面
├── CMakeLists.txt           # 项目配置
└── sdkconfig               # SDK 配置
```

## 接线说明

### ESP32-C3

| 模块 | SDA | SCL | GPIO |
|------|-----|-----|------|
| DHT11 | - | - | GPIO3 |
| GY-30 | GPIO1 | GPIO2 | - |
| MICS5524 | - | - | GPIO10 |
| LD2450 | TX→GPIO5 | RX→GPIO4 | - |

## License

MIT License
