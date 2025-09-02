// 简化的舵机测试程序
#include <Arduino.h>
#include "ST3215.h"

// 舵机对象
ST3215 servo(Serial1, 1000000);

void setup() {
    Serial.begin(115200);
    Serial.println("ST3215 舵机测试开始...");
    
    // 初始化舵机
    servo.begin();
    servo.enableDebug(true);
    
    // 测试舵机3, 4
    std::vector<uint8_t> testIds = {3, 4};
    
    for (uint8_t id : testIds) {
        Serial.printf("测试舵机 %d...\n", id);
        
        if (servo.ping(id)) {
            Serial.printf("舵机 %d 连接成功!\n", id);
            
            // 设置位置模式
            servo.setMode(id, MODE_POSITION);
            servo.setTorqueMode(id, TORQUE_ENABLE);
            
            Serial.printf("舵机 %d 已配置完成\n", id);
        } else {
            Serial.printf("舵机 %d 连接失败\n", id);
        }
        
        delay(100);
    }
    
    Serial.println("初始化完成，开始循环测试...");
}

void loop() {
    static unsigned long lastMove = 0;
    static int positionIndex = 0;
    
    // 每2秒移动一次
    if (millis() - lastMove >= 2000) {
        lastMove = millis();
        
        // 测试位置数组
        uint16_t positions[] = {512, 1024, 2048, 3072, 3584};
        int numPositions = sizeof(positions) / sizeof(positions[0]);
        
        uint16_t targetPos = positions[positionIndex % numPositions];
        
        Serial.printf("同步移动舵机到位置 %d...\n", targetPos);
        
        // 同步移动多个舵机
        std::vector<uint8_t> ids = {3, 4};
        std::vector<uint16_t> targetPositions = {targetPos, targetPos + 200};
        
        if (servo.moveToPosition(ids, targetPositions, 1000, 50)) {
            Serial.println("移动指令发送成功");
            
            // 等待一下然后读取位置
            delay(800);
            
            // 读取实际位置
            std::map<uint8_t, PositionData> results;
            if (servo.readPosition(ids, results)) {
                for (const auto& pair : results) {
                    Serial.printf("舵机%d: 位置=%d, 负载=%d\n", 
                                 pair.first, pair.second.position, pair.second.load);
                }
            }
        } else {
            Serial.println("移动指令失败");
        }
        
        positionIndex++;
    }
    
    delay(10);
}
