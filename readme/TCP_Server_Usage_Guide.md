# ESP32 ST3215舵机TCP服务器使用指南

## 概述

本项目将ESP32转换为一个TCP服务器，通过WiFi接收JSON格式的命令来控制ST3215总线型舵机。这样您就可以在PC上通过网络控制舵机，ESP32充当翻译器的角色。

## 硬件连接

### ESP32引脚连接
- **舵机串口**: GPIO18(RX), GPIO19(TX) → ST3215舵机总线
- **OLED显示器**: GPIO21(SDA), GPIO22(SCL) → SSD1306 I2C
- **状态LED**: GPIO23 → WS2812B LED带

### 舵机连接
- ST3215舵机通过总线连接到ESP32的UART1端口
- 支持多个舵机，默认配置为舵机ID 3和4

## 软件配置

### 1. WiFi配置
在 `src/main_tcp.cpp` 中修改WiFi信息：
```cpp
const char* ssid = "YOUR_WIFI_SSID";        // 您的WiFi名称
const char* password = "YOUR_WIFI_PASSWORD"; // 您的WiFi密码
```

### 2. 编译和上传
```bash
# 使用PlatformIO
pio run -t upload

# 或者使用Arduino IDE
# 将src/main_tcp.cpp重命名为main_tcp.ino并导入Arduino IDE
```

### 3. 串口监控
```bash
pio device monitor
```
查看ESP32的IP地址和连接状态。

## TCP通信协议

### 服务器信息
- **端口**: 8888
- **协议**: TCP
- **数据格式**: JSON

### JSON命令格式
所有命令都使用JSON格式，必须包含`func`字段指定要调用的函数：

```json
{
    "func": "function_name",
    "param1": "value1",
    "param2": "value2"
}
```

### 响应格式
服务器响应也是JSON格式：

```json
{
    "success": true,
    "data": {...}
}
```

或错误响应：
```json
{
    "success": false,
    "error": "error_message"
}
```

## 支持的函数

### 1. ping - 测试舵机连接
**请求:**
```json
{
    "func": "ping",
    "id": 3
}
```

**响应:**
```json
{
    "success": true,
    "id": 3,
    "connected": true
}
```

### 2. move2Posi - 移动舵机到指定位置

**单舵机:**
```json
{
    "func": "move2Posi",
    "id": 3,
    "position": 2048,
    "velocity": 800,
    "acceleration": 100
}
```

**多舵机同步:**
```json
{
    "func": "move2Posi",
    "ids": [3, 4],
    "positions": [1024, 3072],
    "velocity": 800,
    "acceleration": 100
}
```

### 3. readPosi - 读取舵机位置

**单舵机:**
```json
{
    "func": "readPosi",
    "id": 3
}
```

**响应:**
```json
{
    "success": true,
    "id": 3,
    "position": 2048,
    "velocity": 0,
    "load": 0
}
```

**多舵机:**
```json
{
    "func": "readPosi",
    "ids": [3, 4]
}
```

**响应:**
```json
{
    "success": true,
    "data": {
        "3": {"position": 2048, "velocity": 0, "load": 0},
        "4": {"position": 1024, "velocity": 0, "load": 0}
    }
}
```

### 4. setMode - 设置工作模式
```json
{
    "func": "setMode",
    "id": 3,
    "mode": "posi"
}
```
支持的模式: "posi", "wheel", "pwm", "step"

### 5. setTorqueMode - 设置力矩模式
```json
{
    "func": "setTorqueMode",
    "id": 3,
    "mode": "torque"
}
```
支持的模式: "free", "torque", "damped"

### 6. changeId - 更改舵机ID
```json
{
    "func": "changeId",
    "old_id": 1,
    "new_id": 3
}
```

## Python测试客户端

项目包含一个Python测试客户端 `test_tcp_client.py`：

```bash
# 运行测试客户端
python test_tcp_client.py 192.168.1.100

# 替换为您的ESP32实际IP地址
```

### 测试客户端功能
1. 自动连接测试
2. 舵机Ping测试
3. 单舵机移动测试
4. 多舵机同步移动测试
5. 位置读取测试
6. 交互模式

## 状态指示

### OLED显示器
- 第1行: 标题 "ST3215 TCP Server"
- 第2行: WiFi状态和IP地址
- 第3行: 客户端连接状态

### LED状态指示
- **LED 1 (红/绿)**: WiFi连接状态
  - 红色: WiFi未连接
  - 绿色: WiFi已连接
- **LED 2 (蓝/黑)**: 客户端连接状态
  - 蓝色: 有客户端连接
  - 黑色: 无客户端连接

## 使用示例

### Python客户端示例
```python
import socket
import json

# 连接到ESP32
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('192.168.1.100', 8888))

# 移动舵机3到位置2048
command = {
    "func": "move2Posi",
    "id": 3,
    "position": 2048,
    "velocity": 800
}

sock.send((json.dumps(command) + '\n').encode())
response = sock.recv(1024).decode()
print(json.loads(response))

sock.close()
```

### JavaScript (Node.js) 示例
```javascript
const net = require('net');

const client = new net.Socket();
client.connect(8888, '192.168.1.100', () => {
    console.log('Connected');
    
    const command = {
        func: 'readPosi',
        ids: [3, 4]
    };
    
    client.write(JSON.stringify(command) + '\n');
});

client.on('data', (data) => {
    console.log('Response:', JSON.parse(data.toString()));
    client.destroy();
});
```

## 故障排除

### 1. WiFi连接失败
- 检查SSID和密码是否正确
- 确认ESP32在WiFi信号覆盖范围内
- 检查路由器是否支持2.4GHz WiFi

### 2. 舵机无响应
- 检查舵机电源和接线
- 确认舵机ID设置正确
- 检查串口波特率(默认1M)

### 3. TCP连接失败
- 确认ESP32已连接WiFi
- 检查防火墙设置
- 确认IP地址和端口正确

### 4. JSON解析错误
- 确保JSON格式正确
- 检查字段名称拼写
- 确认数据类型正确

## 性能说明

- **并发连接**: 支持单客户端连接
- **响应时间**: 通常 < 100ms
- **舵机控制**: 支持多达254个舵机(理论值)
- **位置精度**: 12位 (0-4095)
- **速度范围**: 0-65535

## 安全注意事项

1. 确保舵机有足够的活动空间
2. 设置合理的速度和加速度值
3. 在网络不稳定时注意舵机状态
4. 建议在局域网内使用

## 扩展功能

本框架可以轻松扩展支持：
- 更多舵机控制功能
- WebSocket支持
- MQTT协议
- Web界面控制
- 多客户端支持
- 舵机序列动作

如需添加新功能，在 `callServoFunction()` 函数中添加新的命令处理即可。
