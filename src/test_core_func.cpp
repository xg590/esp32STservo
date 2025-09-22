#include <Arduino.h>
#include <HardwareSerial.h>
#include <vector>
#include <map>
#include "test_core_func.h"

// 测试用的舵机ID定义
const uint8_t TEST_SERVO_ID_1 = 3;
const uint8_t TEST_SERVO_ID_2 = 4;

void runAllTests() {
    if (servo == nullptr) {
        Serial.printf("❌ Servo object not initialized - showing test structure only\n");
    }
    
    Serial.printf("=====================================\n"); delay(5000); testPingFunction();
    Serial.printf("=====================================\n"); delay(5000); testReadFunction();
    Serial.printf("=====================================\n"); delay(5000); testWriteFunctions();
    Serial.printf("=====================================\n"); delay(5000); testSyncReadFunction();
    Serial.printf("=====================================\n"); delay(5000); testRegWriteFunction(); testActionFunction();
    Serial.printf("=====================================\n"); delay(5000); testSyncReadFunction();
    Serial.printf("=====================================\n"); delay(5000); testSyncWriteFunction();
    Serial.printf("=====================================\n"); delay(5000); testSyncReadFunction();

    Serial.printf("🏁 All tests completed!\n");
}

void testPingFunction() {
    uint8_t error = 0;
    std::vector<uint8_t> params_rx;

    // 实际测试代码（当servo对象可用时）
    bool result1 = servo->ping(TEST_SERVO_ID_1, error, params_rx);
    if (result1) {
        Serial.printf("Ping:✅ dev_id:%d\n", TEST_SERVO_ID_1); 
    } else {
        Serial.printf("Ping:❌ dev_id:%d\n", TEST_SERVO_ID_1);
    }

    error = 0;
    params_rx.clear();
    try {
        bool result2 = servo->ping(99, error, params_rx);
        if (!result2) {
            Serial.printf("Ping:✅ dev_id:%d\n", 99);
        } else {
            Serial.printf("Ping:❌ dev_id:%d\n", 99);
        }
    } catch (const SerialTimeoutException& e) {
        Serial.printf("Ping:❌ dev_id:%d \nException -> %s\n", 99, e.what());
    }
}

void testReadFunction() {
    uint8_t error = 0;
    std::vector<uint8_t> params_rx;

    bool result2 = servo->read(TEST_SERVO_ID_1, servo->MEM_ADDR_PRESENT_POSITION, 2, error, params_rx);
    if (result2 && params_rx.size() >= 2) {
        int position = servo->bytesToInt(params_rx[0], params_rx[1]);
        Serial.printf("Read:✅ dev_id:%d position:%d\n", TEST_SERVO_ID_1, position);
        if (servo) servo->printPacket(params_rx);
    } else {
        Serial.printf("Read:❌ dev_id:%d\n", TEST_SERVO_ID_1);
    }
}

void testWriteFunctions() {
    uint8_t error = 0;
    std::vector<uint8_t> params_rx = {};

    // 测试 write_data - 设置完整运动参数
    uint8_t acc = 160; 
    int    posi = 0;
    int    velo = 800;  
    uint8_t posiL, posiH;
    uint8_t veloL, veloH;
    servo->intToBytes(posi, posiL, posiH);
    servo->intToBytes(velo, veloL, veloH);
    std::vector<uint8_t> motionData = { acc, posiL, posiH, 0x00, 0x00, veloL, veloH}; 
    Serial.printf("Moving to position %d...\n", posi);
    bool result1 = servo->write_data(TEST_SERVO_ID_1, servo->MEM_ADDR_ACC, motionData, error, params_rx);
    if (result1) {
        Serial.printf("Write:✅ dev_id:%d\n", TEST_SERVO_ID_1);
    } else {
        Serial.printf("Write:❌ dev_id:%d\n", TEST_SERVO_ID_1);
    }
    error = 0;
    params_rx.clear();
    bool result2 = servo->write_data(TEST_SERVO_ID_2, servo->MEM_ADDR_ACC, motionData, error, params_rx);
    if (result2) {
        Serial.printf("Write:✅ dev_id:%d\n", TEST_SERVO_ID_2);
    } else {
        Serial.printf("Write:❌ dev_id:%d\n", TEST_SERVO_ID_2);
    }
}

void testRegWriteFunction() {
    int     posi = 2000;
    int     velo =  800; 
    uint8_t posiL, posiH;
    uint8_t veloL, veloH;
    servo->intToBytes(posi, posiL, posiH);
    servo->intToBytes(velo, veloL, veloH);
    std::vector<uint8_t> motionData = {posiL, posiH, 0x00, 0x00, veloL, veloH};  
    Serial.printf("Moving to position %d...\n", posi);

    uint8_t error = 0;
    std::vector<uint8_t> params_rx;
    bool result1 = servo->reg_write(TEST_SERVO_ID_1, servo->MEM_ADDR_GOAL_POSITION, motionData, error, params_rx);
    if (result1) {
        Serial.printf("RegWrite:✅ dev_id:%d\n", TEST_SERVO_ID_1); 
    } else {
        Serial.printf("RegWrite:❌ dev_id:%d\n", TEST_SERVO_ID_1);
    }
    bool result2 = servo->reg_write(TEST_SERVO_ID_2, servo->MEM_ADDR_GOAL_POSITION, motionData, error, params_rx);
    if (result2) {
        Serial.printf("RegWrite:✅ dev_id:%d\n", TEST_SERVO_ID_2); 
    } else {
        Serial.printf("RegWrite:❌ dev_id:%d\n", TEST_SERVO_ID_2);
    }
    Serial.printf("Note: Commands are prepared but not executed until action() is called\n");
}

void testActionFunction() {
    bool result = servo->action();
    if (result) {
        Serial.printf("Action:✅\n");
    } else {
        Serial.printf("Action:❌\n");
    }
}

void testSyncReadFunction() {
    std::vector<uint8_t> servoIds = {TEST_SERVO_ID_1, TEST_SERVO_ID_2};
    std::map<uint8_t, std::vector<uint8_t>> params_rx_arr;
    params_rx_arr.clear();
    bool result1 = servo->sync_read(servoIds, servo->MEM_ADDR_PRESENT_POSITION, 6, params_rx_arr);
    if (result1) {
        for (const auto& pair : params_rx_arr) {
            uint8_t id = pair.first;
            const std::vector<uint8_t>& data = pair.second;
            if (data.size() >= 6) {
                int position = servo->bytesToInt(data[0], data[1]);
                int speed    = servo->bytesToInt(data[2], data[3]);
                int load     = servo->bytesToInt(data[4], data[5]);
                Serial.printf("SyncRead:✅ dev_id=%d, Pos=%d, Speed=%d, Load=%d\n", id, position, speed, load);
            }
        }
    } else {
        Serial.printf("SyncRead:❌\n");
    }
}

void testSyncWriteFunction() {
    std::vector<uint8_t> servoIds = {TEST_SERVO_ID_1, TEST_SERVO_ID_2};

    // 测试3: 同步设置完整运动参数
    uint8_t  acc =  160;  
    int     posi_1 = 4000;
    int     posi_2 = 3800;
    int     velo =  800; 
    uint8_t posiL_1, posiH_1;
    uint8_t posiL_2, posiH_2;
    uint8_t veloL, veloH;
    servo->intToBytes(posi_1, posiL_1, posiH_1);
    servo->intToBytes(posi_2, posiL_2, posiH_2);
    servo->intToBytes(velo, veloL, veloH);  
    std::vector<std::vector<uint8_t>> motionData = {
        {acc, posiL_1, posiH_1, 0x00, 0x00, veloL, veloH},  // 舵机1参数
        {acc, posiL_2, posiH_2, 0x00, 0x00, veloL, veloH}   // 舵机2参数
    };
    bool result1 = servo->sync_write(servoIds, servo->MEM_ADDR_ACC, motionData);
    if (result1) {
        Serial.printf("SyncWrite:✅\n");
    } else {
        Serial.printf("SyncWrite:❌\n");
    }
}
