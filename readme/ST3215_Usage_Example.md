# ST3215舵机驱动使用说明

## 概述
ST3215.h/ST3215.cpp是从Python版本的STservo.py翻译而来的C++舵机驱动库，专为ESP32设计，使用硬件UART(Serial1)与ST3215舵机进行通信。

## 硬件连接
- **RX引脚**: GPIO18 (连接到舵机的TXD)
- **TX引脚**: GPIO19 (连接到舵机的RXD)
- **波特率**: 1,000,000 bps (默认)

## 基本使用方法

### 1. 包含头文件
```cpp
#include "ST3215.h"
```

### 2. 创建舵机对象
```cpp
ST3215 servo(Serial1, 1000000);  // 使用Serial1，波特率1M
```

### 3. 初始化
```cpp
void setup() {
    servo.begin();                    // 初始化舵机通信
    servo.enableDebug(true);          // 可选：启用调试输出
}
```

## 主要功能

### 基础通信
```cpp
// 测试舵机连接
bool connected = servo.ping(1);       // ping ID为1的舵机

// 读取数据
std::vector<uint8_t> data;
bool success = servo.read(1, 0x38, 2, data);  // 读取位置信息

// 写入数据
servo.write(1, 0x28, 1);             // 启用力矩
```

### 位置控制
```cpp
// 单个舵机移动
servo.moveToPosition(1, 2048, 1000, 50);  // ID=1, 位置=2048, 速度=1000, 加速度=50

// 多个舵机同步移动
std::vector<uint8_t> ids = {1, 2, 3};
std::vector<uint16_t> positions = {1024, 2048, 3072};
servo.moveToPosition(ids, positions, 800, 100);
```

### 状态读取
```cpp
// 读取位置、速度、负载
PositionData posData;
if (servo.readPosition(1, posData)) {
    Serial.printf("位置: %d, 速度: %d, 负载: %d\n", 
                 posData.position, posData.velocity, posData.load);
}

// 读取完整状态
ServoStatus status;
if (servo.readServoStatus(1, status)) {
    Serial.printf("位置: %d, 电压: %d, 温度: %d\n", 
                 status.position, status.voltage, status.temperature);
}
```

### 配置设置
```cpp
// 设置工作模式
servo.setMode(1, MODE_POSITION);      // 位置模式
servo.setMode(1, MODE_WHEEL);         // 轮式模式

// 设置力矩模式
servo.setTorqueMode(1, TORQUE_ENABLE);  // 启用力矩
servo.setTorqueMode(1, TORQUE_FREE);    // 自由模式

// 更改舵机ID
servo.changeId(1, 5);                 // 将ID从1改为5

// 设置位置校正
servo.setPositionCorrection(1, 100, true);  // 设置+100的校正值并保存
```

## 高级功能

### 同步操作
```cpp
// 同步写入
std::vector<uint8_t> ids = {1, 2, 3};
std::vector<std::vector<uint8_t>> dataArray = {
    {50, 0x00, 0x04},  // 舵机1的数据
    {50, 0x00, 0x08},  // 舵机2的数据
    {50, 0x00, 0x0C}   // 舵机3的数据
};
servo.syncWrite(ids, 0x29, dataArray);

// 同步读取多个舵机位置
std::map<uint8_t, PositionData> results;
servo.readPosition(ids, results);
for (auto& pair : results) {
    Serial.printf("舵机%d: 位置=%d\n", pair.first, pair.second.position);
}
```

### 调试功能
```cpp
servo.enableDebug(true);              // 启用调试输出
// 调试模式下会显示发送/接收的数据包内容
```

## 错误处理
所有方法都返回boolean值指示操作是否成功：
```cpp
if (!servo.ping(1)) {
    Serial.println("舵机1连接失败，请检查接线");
}

if (!servo.moveToPosition(1, 2048)) {
    Serial.println("舵机移动指令失败");
}
```

## 注意事项
1. **波特率匹配**: 确保舵机的波特率设置与代码中一致
2. **电源供应**: ST3215舵机需要7.4V电源，ESP32需要3.3V/5V
3. **信号电平**: 舵机通信使用TTL电平(3.3V/5V)
4. **ID设置**: 每个舵机必须有唯一的ID
5. **接线检查**: 确认RX/TX接线正确（交叉连接）

## 完整示例
参考main.cpp中的实现，包含了：
- 舵机初始化
- 连接测试
- 定期位置移动演示
- 状态读取和显示
- 与LED和OLED的集成使用
