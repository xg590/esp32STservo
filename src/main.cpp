#include <Arduino.h>
#include <HardwareSerial.h>
#include "core_func.h"
#include "test_core_func.h"

// 全局ST3215对象指针
ST3215* servo = nullptr;

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("\n\n");
    Serial.println("[SETUP] === ESP32 ST3215 Core Functions Test ===");
    
    // 创建ST3215对象（使用Serial1，波特率1000000）
    try {
        servo = new ST3215(Serial1, 1000000, true);
        if (servo && servo->begin()) {
            Serial.println("[SETUP] ST3215 object created and initialized successfully");
            servo->setDebug(false);  // 启用调试输出
            servo->setTimeout(1000); // 设置3秒超时
        } else {
            Serial.println("[SETUP] Failed to initialize ST3215 object");
            delete servo;
            servo = nullptr;
        }
    } catch (const std::exception& e) {
        Serial.printf("[SETUP] Exception creating ST3215: %s\n", e.what());
        servo = nullptr;
        while (true) {};
    } 
    
    // 运行测试
}

void loop() {
    runAllTests();
    Serial.println("[LOOP] Run tests again...");
    Serial.println("\n\n");
    delay(5000);
}