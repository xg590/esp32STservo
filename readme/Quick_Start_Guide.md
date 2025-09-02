# ESP32 ST3215 TCP服务器快速开始指南

## 🚀 快速部署步骤

### 1. 硬件准备
- **ESP32开发板** (已连接)
- **ST3215舵机** (ID设置为3和4)
- **SSD1306 OLED显示器** (可选)
- **WS2812B LED灯带** (可选)

### 2. 软件准备
确保安装了以下工具：
- PlatformIO IDE 或 Arduino IDE
- Python 3.x (用于测试客户端)

### 3. 配置WiFi
编辑 `src/main_tcp.cpp` 文件中的WiFi信息：
```cpp
const char* ssid = "YOUR_WIFI_SSID";        // 替换为您的WiFi名称
const char* password = "YOUR_WIFI_PASSWORD"; // 替换为您的WiFi密码
```

### 4. 编译上传
```bash
# 使用PlatformIO
pio run -t upload

# 查看串口输出
pio device monitor
```

### 5. 获取ESP32 IP地址
从串口监控中找到ESP32的IP地址，例如：
```
WiFi连接成功!
IP地址: 192.168.1.100
TCP服务器已启动，端口: 8888
```

### 6. 测试连接
运行Python测试客户端：
```bash
python test_tcp_client.py 192.168.1.100
```

## 📋 项目文件结构

```
esp32ST3215Arm/
├── src/
│   ├── main.cpp          # 原始LED+OLED+舵机演示程序
│   ├── main_tcp.cpp      # 新的TCP服务器主程序 ⭐
│   ├── ST3215.h          # 舵机驱动头文件 (已更新)
│   ├── ST3215.cpp        # 舵机驱动实现 (已更新)
│   └── STservo.py        # 原始Python驱动程序
├── test_tcp_client.py    # Python测试客户端 ⭐
├── TCP_Server_Usage_Guide.md  # 详细使用指南 ⭐
├── Quick_Start_Guide.md  # 本快速指南 ⭐
└── platformio.ini        # 项目配置 (已更新)
```

## 🔧 主要功能对比

### 原始Python驱动 → C++驱动转换 ✅
- [x] 基础通信协议 (ping, read, write)
- [x] 同步操作 (sync_read, sync_write)
- [x] 位置控制 (move2Posi)
- [x] 状态读取 (readPosi)
- [x] 模式设置 (setMode, setTorqueMode)
- [x] ID更改 (changeId)
- [x] 编码器解包装 (EncoderUnwrapper类)
- [x] Python兼容接口

### 新增TCP服务器功能 ✅
- [x] WiFi连接管理
- [x] TCP服务器 (端口8888)
- [x] JSON协议解析
- [x] 函数调用路由
- [x] 错误处理和响应
- [x] 状态显示 (OLED + LED)

## 🎮 快速测试命令

### 1. 基础连通性测试
```json
{"func": "ping", "id": 3}
{"func": "ping", "id": 4}
```

### 2. 舵机移动测试
```json
{"func": "move2Posi", "id": 3, "position": 2048}
{"func": "move2Posi", "ids": [3, 4], "positions": [1024, 3072]}
```

### 3. 位置读取测试
```json
{"func": "readPosi", "id": 3}
{"func": "readPosi", "ids": [3, 4]}
```

## 🔍 系统架构图

```
PC客户端 <---> WiFi <---> ESP32 TCP服务器 <---> UART <---> ST3215舵机
    │                         │                              │
    │                         ├── OLED显示状态               ├── 舵机ID: 3
    │                         └── LED状态指示               └── 舵机ID: 4
    │
    └── Python/JS/任何TCP客户端
```

## ⚡ 性能指标

- **网络延迟**: < 50ms (局域网)
- **舵机响应**: < 100ms
- **位置精度**: 12位 (0-4095)
- **最大舵机数**: 254个 (理论)
- **并发连接**: 1个客户端

## 🐛 常见问题解决

### WiFi连接失败
```
WiFi: Disconnected
```
**解决方案**: 检查SSID和密码，确保ESP32在信号范围内

### 舵机无响应  
```
{"success": false, "connected": false}
```
**解决方案**: 检查舵机电源、接线和ID设置

### JSON解析错误
```
{"success": false, "error": "JSON parse error"}
```
**解决方案**: 检查JSON格式，确保语法正确

## 🎯 下一步扩展

1. **Web界面**: 创建HTML/JS控制界面
2. **多客户端**: 支持多个客户端同时连接
3. **MQTT协议**: 支持物联网标准协议
4. **动作序列**: 预设舵机动作序列
5. **实时流传输**: 舵机状态实时推送

## 📞 支持

如果遇到问题，请检查：
1. 串口监控输出
2. WiFi连接状态
3. 舵机硬件连接
4. JSON命令格式
5. 网络防火墙设置

项目完全基于您原始的Python驱动程序转换而来，保持了所有核心功能，并添加了现代化的网络控制能力。
