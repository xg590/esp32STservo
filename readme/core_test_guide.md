# ESP32 ST3215 Core Functions Test Guide

## 概述

这个测试代码用于验证 `core.cpp` 中的基础舵机通信函数，包括：
- `ping()` - 舵机连接测试
- `read()` - 读取舵机数据
- `write_bytes()` / `write_int()` - 写入舵机数据
- `reg_write()` - 寄存器写入（延迟执行）
- `action()` - 执行寄存器写入命令
- `sync_read()` - 同步读取多个舵机
- `sync_write()` - 同步写入多个舵机

## 文件说明

### test_core_functions.cpp
完整的测试实现，直接包含 core.cpp（可能需要修改以适应您的构建环境）

### test_core_basic.cpp  
更加规范的测试框架，展示测试结构但需要您自己初始化 ST3215 对象

## 使用方法

### 方案一：直接使用 test_core_functions.cpp

1. 确保您的项目中包含了所有必要的头文件
2. 将 `test_core_functions.cpp` 添加到您的项目中
3. 在 PlatformIO 中编译和上传到 ESP32
4. 通过串口监视器查看测试结果

### 方案二：使用 test_core_basic.cpp（推荐）

1. 复制 `test_core_basic.cpp` 到您的项目
2. 修改代码中的 servo 对象初始化部分：

```cpp
// 在 setup() 函数中添加：
#include "你的头文件.h"  // 包含 ST3215 类定义

servo = new ST3215(Serial2, 1000000);  // 使用合适的串口和波特率
if (!servo->begin()) {
    Serial.println("❌ Failed to initialize servo");
    return;
}
servo->enableDebug(true);  // 启用调试输出
```

## 测试内容详解

### 1. Ping测试
- 测试与指定ID舵机的连接
- 测试不存在舵机的响应处理

### 2. Read测试
- 读取单字节数据（舵机ID）
- 读取双字节数据（位置信息）
- 读取多字节数据（位置+速度+负载）

### 3. Write测试
- `write_int()`: 写入单个整数值
- `write_bytes()`: 写入字节数组
- 测试不同数据长度的写入

### 4. Reg Write测试
- 使用寄存器写入准备多个命令
- 验证命令暂存机制

### 5. Action测试
- 执行所有预准备的寄存器写入命令
- 测试广播命令机制

### 6. Sync Read测试
- 同时从多个舵机读取数据
- 处理部分舵机无响应的情况
- 解析返回的数据格式

### 7. Sync Write测试
- 同时向多个舵机发送不同数据
- 测试参数验证（ID数量与数据数量匹配）
- 验证错误处理机制

## 硬件连接

确保您的硬件连接正确：

```
ESP32        ST3215舵机
GPIO18  -->  RX (舵机接收)
GPIO19  -->  TX (舵机发送)
GND     -->  GND
5V      -->  VCC
```

## 测试参数

测试中使用的默认参数：
- 测试舵机ID: 1 和 2
- 串口: Serial2
- 波特率: 1,000,000 bps
- 超时时间: 3000ms

您可以根据实际硬件配置修改这些参数。

## 内存地址

测试中使用的关键内存地址：
- `0x05`: 舵机ID
- `0x21`: 工作模式
- `0x28`: 力矩使能
- `0x29`: 加速度
- `0x2A`: 目标位置
- `0x2C`: 目标时间
- `0x2E`: 目标速度
- `0x38`: 当前位置
- `0x3A`: 当前速度
- `0x3C`: 当前负载

## 预期输出

成功运行时，您应该看到类似输出：

```
=== ESP32 ST3215 Core Functions Test ===
Testing only core.cpp functions

✅ Servo initialized successfully

🔍 Starting Core Functions Tests...
=====================================
📡 Testing ping() function:
----------------------------
Pinging servo ID 1...
Result: ✅ SUCCESS

📖 Testing read() function:
----------------------------
Reading servo ID from address 0x05...
✅ SUCCESS: Servo ID = 1
...
```

## 故障排除

### 常见问题

1. **编译错误**
   - 确保包含了正确的头文件
   - 检查 ST3215 类是否正确定义

2. **串口通信问题**
   - 检查硬件连接
   - 验证波特率设置
   - 确认舵机电源正常

3. **舵机无响应**
   - 检查舵机ID是否正确
   - 确认舵机工作模式
   - 验证通信协议版本

### 调试技巧

1. 启用调试输出：
```cpp
servo->enableDebug(true);
```

2. 检查串口数据：
```cpp
Serial.println("Available bytes: " + String(Serial2.available()));
```

3. 添加延迟：
```cpp
delay(100);  // 在命令之间添加延迟
```

## 扩展测试

您可以基于这个框架添加更多测试：

1. **性能测试**: 测试命令执行时间
2. **压力测试**: 连续发送大量命令
3. **错误恢复测试**: 测试通信中断后的恢复
4. **边界测试**: 测试参数的边界值

## 注意事项

1. 测试前确保舵机处于安全位置
2. 某些测试会移动舵机，注意安全
3. 如果舵机无响应，检查硬件连接和电源
4. 调试输出会影响性能，生产环境中应关闭

## 代码结构

测试代码采用模块化设计：
- 每个函数有独立的测试函数
- 统一的结果输出格式
- 清晰的错误处理机制
- 易于扩展和修改
