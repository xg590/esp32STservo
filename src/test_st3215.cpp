#include <Arduino.h>
#include <vector>
#include "test_st3215.h"

// 测试用的舵机ID定义（与主测试保持一致）
const uint8_t TEST_SERVO_ID_1 = 3;
const uint8_t TEST_SERVO_ID_2 = 4;

void runAllExtTests() {
    if (servo == nullptr) {
        Serial.printf("❌ Servo object not initialized - showing extended test structure only\n");
    }
    
    Serial.printf("=====================================\n");
    Serial.printf("🚀 Starting Extended Function Tests\n");
    Serial.printf("=====================================\n\n"); delay(2000); 
    testTorqueModeFunction()        ; Serial.printf("-------------------------------------\n\n"); delay(2000); 
    testAccelerationFunction()      ; Serial.printf("-------------------------------------\n\n"); delay(2000); 
    testPositionFunction()          ; Serial.printf("-------------------------------------\n\n"); delay(2000); 
    testStatusAndIdFunction()       ; Serial.printf("-------------------------------------\n\n"); delay(2000); 
    testPositionCorrectionFunction(); Serial.printf("-------------------------------------\n\n"); delay(2000);
    
    Serial.printf("🏁 All extended tests completed!\n");
}

void testTorqueModeFunction() {
    Serial.printf("🔧 [Test] setTorqueMode() function\n"); 
    
    // 测试使能力矩 
    bool result1 = servo->setTorqueMode(TEST_SERVO_ID_1, TORQUE_FREE);
    if (result1) {
        Serial.printf("TorqueMode:✅ dev_id:%d mode:FREE\n", TEST_SERVO_ID_1);
    } else {
        Serial.printf("TorqueMode:❌ dev_id:%d mode:FREE\n", TEST_SERVO_ID_1);
    }
    
    delay(1000);
    
    // 测试释放力矩 
    bool result2 = servo->setTorqueMode(TEST_SERVO_ID_2, TORQUE_ENABLE);
    if (result2) {
        Serial.printf("TorqueMode:✅ dev_id:%d mode:ENABLE\n", TEST_SERVO_ID_2);
    } else {
        Serial.printf("TorqueMode:❌ dev_id:%d mode:ENABLE\n", TEST_SERVO_ID_2);
    }
}

void testAccelerationFunction() {
    Serial.printf("⚡ [Test] Acceleration Control (set & get)\n"); 
    
    // 步骤1：设置加速度 - 使用保守的值进行测试
    std::vector<uint8_t> servoIds = {TEST_SERVO_ID_1, TEST_SERVO_ID_2};
    std::vector<uint8_t> targetAccelerations = {30, 50};  // 使用较低的安全值进行测试
    
    Serial.printf("Step 1: Setting accelerations\n");
    Serial.printf("Target:\n");
    Serial.printf("  dev_id:%d -> acc:%d\n", TEST_SERVO_ID_1, targetAccelerations[0]);
    Serial.printf("  dev_id:%d -> acc:%d\n", TEST_SERVO_ID_2, targetAccelerations[1]);
    
    bool setResult = servo->setAcceleration(servoIds, targetAccelerations);
    if (setResult) {
        Serial.printf("SetAcceleration:✅\n");
    } else {
        Serial.printf("SetAcceleration:❌\n");
        return;
    }
    
    delay(2000);  // 等待设置完成
    
    // 步骤2：读取加速度验证
    std::vector<uint8_t> readAccelerations;
    Serial.printf("Step 2: Reading back accelerations...\n");
    
    bool getResult = servo->getAcceleration(servoIds, readAccelerations);
    if (getResult) {
        Serial.printf("Real Acc:\n");
        for (size_t i = 0; i < servoIds.size(); i++) {
            uint8_t dev_id = servoIds[i];
            uint8_t readAcc = readAccelerations[i];
            uint8_t targetAcc = targetAccelerations[i];
            
            if (readAcc == targetAcc) {
                Serial.printf("  dev_id:%d acc:%d target:%d ✅ MATCH\n", dev_id, readAcc, targetAcc);
            } else {
                Serial.printf("  dev_id:%d acc:%d target:%d ❌ MISMATCH (diff:%d)\n", 
                             dev_id, readAcc, targetAcc, abs((int)readAcc - (int)targetAcc));
            }
        }
        
        // 步骤3：尝试更大范围的值来确定限制
        Serial.printf("Step 3: Testing acceleration limits...\n");
        std::vector<uint8_t> testValues = {10, 25, 35, 45, 49, 50, 51, 75, 100, 150, 200};
        
        for (uint8_t testVal : testValues) {
            std::vector<uint8_t> singleId = {TEST_SERVO_ID_1};
            std::vector<uint8_t> singleAcc = {testVal};
            
            bool setTestResult = servo->setAcceleration(singleId, singleAcc);
            if (setTestResult) {
                delay(500);
                std::vector<uint8_t> readTestAcc;
                bool getTestResult = servo->getAcceleration(singleId, readTestAcc);
                if (getTestResult && !readTestAcc.empty()) {
                    Serial.printf("  Test: set=%d -> read=%d %s\n", 
                                 testVal, readTestAcc[0], 
                                 (testVal == readTestAcc[0]) ? "✅" : "❌");
                    
                    // 找到限制值
                    if (readTestAcc[0] != testVal && readTestAcc[0] < testVal) {
                        Serial.printf("⚠️  Possible hardware limit detected at: %d\n", readTestAcc[0]);
                        break;  // 找到限制后停止测试
                    }
                }
            }
        }
    } else {
        Serial.printf("GetAcceleration:❌ Failed to read acceleration\n");
    }
}

void testPositionFunction() {
    Serial.printf("🎯 [Test] Position Control (set & get)\n"); 
    
    // 步骤0：位置归零
    std::vector<uint8_t>  servoIds = {TEST_SERVO_ID_1, TEST_SERVO_ID_2};
    std::vector<uint16_t> targetPositions  = {   0,    0};    // 不同的目标位置
    std::vector<uint16_t> targetVelocities = {1200, 1200};     // 不同的速度
    
    Serial.printf("Step 0: Move to zero:\n");
    Serial.printf("  dev_id:%d -> pos:%d, vel:%d\n", TEST_SERVO_ID_1, targetPositions[0], targetVelocities[0]);
    Serial.printf("  dev_id:%d -> pos:%d, vel:%d\n", TEST_SERVO_ID_2, targetPositions[1], targetVelocities[1]);
    
    bool setResult = servo->setPosition(servoIds, targetPositions, targetVelocities);
    if (setResult) {
        Serial.printf("SetPosition:✅\n");
    } else {
        Serial.printf("SetPosition:❌\n");
        return;
    }
    
    delay(5000);

    // 步骤1：读取当前位置
    std::vector<uint16_t> currentPositions, currentVelocities;
    
    Serial.printf("Step 1: Reading current positions...\n");
    bool getCurrentResult = servo->getPosition(servoIds, currentPositions, currentVelocities);
    if (getCurrentResult) {
        for (size_t i = 0; i < currentPositions.size() && i < servoIds.size(); i++) {
            Serial.printf("  dev_id:%d current pos:%d vel:%d\n", 
                         servoIds[i], currentPositions[i], currentVelocities[i]);
        }
    }
    
    delay(500);
    
    // 步骤2：设置新位置和速度
    targetPositions = {2000, 2500};    // 不同的目标位置
    targetVelocities = {400, 600};     // 不同的速度
    
    Serial.printf("Step 2: Setting new positions and velocities:\n");
    Serial.printf("  dev_id:%d -> pos:%d, vel:%d\n", TEST_SERVO_ID_1, targetPositions[0], targetVelocities[0]);
    Serial.printf("  dev_id:%d -> pos:%d, vel:%d\n", TEST_SERVO_ID_2, targetPositions[1], targetVelocities[1]);
    
    setResult = servo->setPosition(servoIds, targetPositions, targetVelocities);
    if (setResult) {
        Serial.printf("SetPosition:✅\n");
    } else {
        Serial.printf("SetPosition:❌\n");
        return;
    }
    
    delay(6000);  // 等待运动完成 
    
    // 步骤4：测试单个舵机位置读取
    uint16_t singlePosition;
    Serial.printf("Step 4: Testing single servo position read...\n");
    bool singleResult = servo->getPosition(TEST_SERVO_ID_2, singlePosition);
    if (singleResult) {
        Serial.printf("SingleGetPosition:✅ dev_id:%d pos:%d\n", TEST_SERVO_ID_2, singlePosition);
    } else {
        Serial.printf("SingleGetPosition:❌ dev_id:%d\n", TEST_SERVO_ID_2);
    }
}

void testStatusAndIdFunction() {
    Serial.printf("📊 [Test] Status Reading & ID Management (getStatus & changeId)\n"); 
    
    Serial.printf("⚠️ WARNING: This will temporarily change servo ID for testing\n");
    
    // 步骤1：详细读取原始状态 (dev_id = 3)
    ServoStatus originalStatus;
    Serial.printf("Step 1: Reading comprehensive status from dev_id:%d...\n", TEST_SERVO_ID_1);
    bool result1 = servo->getStatus(TEST_SERVO_ID_1, originalStatus);
    if (result1) {
        Serial.printf("OriginalStatus:✅ dev_id:%d complete data:\n", TEST_SERVO_ID_1);
        Serial.printf("  Position: %d\n", originalStatus.posi);
        Serial.printf("  Velocity: %d\n", originalStatus.velo);
        Serial.printf("  Load: %d\n", originalStatus.load);
        Serial.printf("  Voltage: %d\n", originalStatus.volt);
        Serial.printf("  Temperature: %d°C\n", originalStatus.temp);
        Serial.printf("  Async: %d\n", originalStatus.asyn);
        Serial.printf("  Status: %d\n", originalStatus.stat);
        Serial.printf("  Moving: %s\n", originalStatus.mvng ? "Yes" : "No");
        Serial.printf("  Current: %d\n", originalStatus.curr);
    } else {
        Serial.printf("OriginalStatus:❌ dev_id:%d\n", TEST_SERVO_ID_1);
        return;  // 如果原始状态读取失败，停止测试
    }
    
    delay(1000);
    
    // 步骤2：改变ID (3 → 5)
    uint8_t newId = 5;
    Serial.printf("Step 2: Changing servo ID from %d to %d...\n", TEST_SERVO_ID_1, newId);
    bool result2 = servo->changeId(TEST_SERVO_ID_1, newId);
    if (result2) {
        Serial.printf("ChangeId:✅ %d → %d\n", TEST_SERVO_ID_1, newId);
    } else {
        Serial.printf("ChangeId:❌ %d → %d\n", TEST_SERVO_ID_1, newId);
        return;  // 如果ID修改失败，停止测试
    }
    
    delay(1000);
    
    // 步骤3：用新ID读取状态验证 (dev_id = 5)
    ServoStatus newIdStatus;
    Serial.printf("Step 3: Reading status with new dev_id:%d...\n", newId);
    bool result3 = servo->getStatus(newId, newIdStatus);
    if (result3) {
        Serial.printf("NewIdStatus:✅ dev_id:%d verified:\n", newId);
        Serial.printf("  Position: %d (diff: %d)\n", newIdStatus.posi, 
                      abs((int)newIdStatus.posi - (int)originalStatus.posi));
        Serial.printf("  Temperature: %d°C (diff: %d°C)\n", newIdStatus.temp,
                      abs((int)newIdStatus.temp - (int)originalStatus.temp));
        Serial.printf("  Voltage: %d (diff: %d)\n", newIdStatus.volt,
                      abs((int)newIdStatus.volt - (int)originalStatus.volt));
    } else {
        Serial.printf("NewIdStatus:❌ dev_id:%d\n", newId);
    }
    
    delay(1000);
    
    // 步骤4：改回原始ID (5 → 3)
    Serial.printf("Step 4: Changing servo ID back from %d to %d...\n", newId, TEST_SERVO_ID_1);
    bool result4 = servo->changeId(newId, TEST_SERVO_ID_1);
    if (result4) {
        Serial.printf("ChangeIdBack:✅ %d → %d\n", newId, TEST_SERVO_ID_1);
    } else {
        Serial.printf("ChangeIdBack:❌ %d → %d\n", newId, TEST_SERVO_ID_1);
        Serial.printf("⚠️ WARNING: Servo ID may still be %d!\n", newId);
        return;  // 如果ID恢复失败，发出警告
    }
    
    delay(1000);
    
    // 步骤5：用原始ID验证状态完整性 (dev_id = 3)
    ServoStatus finalStatus;
    Serial.printf("Step 5: Verifying status integrity with restored dev_id:%d...\n", TEST_SERVO_ID_1);
    bool result5 = servo->getStatus(TEST_SERVO_ID_1, finalStatus);
    if (result5) {
        Serial.printf("FinalStatus:✅ dev_id:%d integrity check:\n", TEST_SERVO_ID_1);
        
        // 温度检查（最可靠的验证指标）
        int tempDiff = abs((int)finalStatus.temp - (int)originalStatus.temp);
        Serial.printf("  Temperature: %d°C (original: %d°C, diff: %d°C) %s\n", 
                      finalStatus.temp, originalStatus.temp, tempDiff,
                      (tempDiff <= 2) ? "✅" : "⚠️");
        
        // 电压检查
        int voltDiff = abs((int)finalStatus.volt - (int)originalStatus.volt);
        Serial.printf("  Voltage: %d (original: %d, diff: %d) %s\n", 
                      finalStatus.volt, originalStatus.volt, voltDiff,
                      (voltDiff <= 3) ? "✅" : "⚠️");
        
        // 整体结果判断
        if (tempDiff <= 2 && voltDiff <= 3) {
            Serial.printf("✅ ID change test completed successfully\n");
        } else {
            Serial.printf("⚠️ Some status values differ significantly from original\n");
        }
    } else {
        Serial.printf("FinalStatus:❌ dev_id:%d\n", TEST_SERVO_ID_1);
        Serial.printf("❌ ID restoration may have failed!\n");
    }
    
    Serial.printf("🏁 Status and ID management test completed\n");
}
 
void testPositionCorrectionFunction() {
    Serial.printf("⚙️ [Test] Position Correction functions\n"); 
    
    // 校正值归零（不保存到EPROM）
    int16_t newCorrection = 0;  // 设置一个小的校正值
    Serial.printf("Setting temporary position correction to %d...\n", newCorrection);
    bool result0 = servo->setPositionCorrection(TEST_SERVO_ID_1, newCorrection, false);  // 不保存
    if (result0) {
        Serial.printf("SetCorrection:✅ dev_id:%d value:%d (temporary)\n", TEST_SERVO_ID_1, newCorrection);
    } else {
        Serial.printf("SetCorrection:❌ dev_id:%d value:%d\n", TEST_SERVO_ID_1, newCorrection);
    }
    
    delay(500);

    // 测试读取当前校正值
    int16_t currentCorrection = 0;
    Serial.printf("Reading current position correction for servo %d...\n", TEST_SERVO_ID_1);
    bool result1 = servo->getPositionCorrection(TEST_SERVO_ID_1, currentCorrection);
    if (result1) {
        Serial.printf("GetCorrection:✅ dev_id:%d current:%d\n", TEST_SERVO_ID_1, currentCorrection);
    } else {
        Serial.printf("GetCorrection:❌ dev_id:%d\n", TEST_SERVO_ID_1);
    }
    
    // 测试设置新的校正值（不保存到EPROM）
    newCorrection = 50;  // 设置一个小的校正值
    Serial.printf("Setting temporary position correction to %d...\n", newCorrection);
    bool result2 = servo->setPositionCorrection(TEST_SERVO_ID_1, newCorrection, false);  // 不保存
    if (result2) {
        Serial.printf("SetCorrection:✅ dev_id:%d value:%d (temporary)\n", TEST_SERVO_ID_1, newCorrection);
    } else {
        Serial.printf("SetCorrection:❌ dev_id:%d value:%d\n", TEST_SERVO_ID_1, newCorrection);
    }
    
    delay(500);
    
    // 验证校正值是否设置成功
    int16_t verifyCorrection = 0;
    Serial.printf("Verifying position correction...\n");
    bool result3 = servo->getPositionCorrection(TEST_SERVO_ID_1, verifyCorrection);
    if (result3) {
        Serial.printf("VerifyCorrection:✅ dev_id:%d value:%d\n", TEST_SERVO_ID_1, verifyCorrection);
        if (verifyCorrection == newCorrection) {
            Serial.printf("✅ Correction value matches expected value\n");
        } else {
            Serial.printf("⚠️ Correction value differs: expected %d, got %d\n", newCorrection, verifyCorrection);
        }
    } else {
        Serial.printf("VerifyCorrection:❌ dev_id:%d\n", TEST_SERVO_ID_1);
    }
}

void testGetPositionFunction() {
    // 测试读取当前位置
    uint16_t currentPosition = 0;
    bool result0 = servo->getPosition(TEST_SERVO_ID_1, currentPosition);
    if (result0) {
        Serial.printf("GetPosition:✅ dev_id:%d current:%d\n", TEST_SERVO_ID_1, currentPosition);
    } else {
        Serial.printf("GetPosition:❌ dev_id:%d\n", TEST_SERVO_ID_1);
    }
}