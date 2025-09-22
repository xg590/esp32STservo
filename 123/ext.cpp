#include "ST3215.h"

////////////////////////////////////////////////////
//                                                //
//   约定小写函数名供内部使用，大写函数名供外部调用   //
//                                                //
////////////////////////////////////////////////////

/*
// 静态波特率表
static const BaudRateMap BAUD_RATES_TABLE[] = {
    {"1M", 0},
    {"500K", 1},
    {"250K", 2},
    {"128K", 3},
    {"115200", 4},
    {"76800", 5},
    {"57600", 6},
    {"38400", 7}
};
*/

// 设置工作模式
bool ST3215::setMode(uint8_t id, ServoMode mode) {
    if (_model == SCS_MODEL) {
        if (_debugEnabled) {
            Serial.println("SCS servos only support position mode");
        }
        return false;
    }
    
    return write_int(id, MEM_ADDR_MODE, static_cast<int>(mode));
}

// 获取工作模式
ServoMode ST3215::getMode(uint8_t id) {
    if (_model == SCS_MODEL) {
        return MODE_POSITION;
    }
    
    std::vector<uint8_t> data;
    if (read(id, MEM_ADDR_MODE, 1, data) && !data.empty()) {
        return static_cast<ServoMode>(data[0]);
    }
    
    return MODE_POSITION;
}

// 设置力矩模式
bool ST3215::setTorqueMode(uint8_t id, TorqueMode mode) {
    return write_int(id, MEM_ADDR_TORQUE_ENABLE, static_cast<int>(mode));
} 

// 移动到指定位置（多个舵机）
bool ST3215::moveToPosition(const std::vector<uint8_t>& id_arr, const std::vector<uint16_t>& positions, 
                            uint16_t velocity, uint8_t acceleration) {
    if (id_arr.size() != positions.size()) {
        if (_debugEnabled) {
            Serial.println("ID count and position count mismatch");
        }
        return false;
    }
    
    std::vector<std::vector<uint8_t>> dataArray;
    
    for (uint16_t position : positions) {
        if (position > 0x0FFF) {
            if (_debugEnabled) {
                Serial.printf("Position value %d too large\n", position);
            }
            return false;
        }
        
        std::vector<uint8_t> data = {
            acceleration,
            static_cast<uint8_t>(position & 0xFF),
            static_cast<uint8_t>((position >> 8) & 0xFF),
            0x00, 0x00,  // 目标时间
            static_cast<uint8_t>(velocity & 0xFF),
            static_cast<uint8_t>((velocity >> 8) & 0xFF)
        };
        
        dataArray.push_back(data);
    }
    
    return sync_write(id_arr, MEM_ADDR_ACC, dataArray);
}

// 读取位置信息（单个舵机）
bool ST3215::readPosition(uint8_t id, PositionData& data) {
    std::vector<uint8_t> response;
    if (read(id, MEM_ADDR_PRESENT_POSITION, 6, response) && response.size() >= 6) {
        data.position = response[0] | (response[1] << 8);
        data.velocity = response[2] | (response[3] << 8);
        data.load     = response[4] | (response[5] << 8);
        return true;
    }
    
    return false;
}

// 读取位置信息（多个舵机）
bool ST3215::readPosition(const std::vector<uint8_t>& id_arr, std::map<uint8_t, PositionData>& results) {
    results.clear();
    
    std::map<uint8_t, std::vector<uint8_t>> rawResults;
    if (sync_read(id_arr, MEM_ADDR_PRESENT_POSITION, 6, rawResults)) {
        for (const auto& pair : rawResults) {
            uint8_t id = pair.first;
            const std::vector<uint8_t>& response = pair.second;
            
            if (response.size() >= 6) {
                PositionData data;
                data.position = response[0] | (response[1] << 8);
                data.velocity = response[2] | (response[3] << 8);
                data.load     = response[4] | (response[5] << 8);
                results[id] = data;
            }
        }
        
        return !results.empty();
    }
    
    return false;
}

// 读取舵机完整状态
bool ST3215::readServoStatus(uint8_t id, ServoStatus& status) {
    // 读取位置、速度、负载
    PositionData posData;
    if (!readPosition(id, posData)) {
        return false;
    }
    
    status.position = posData.position;
    status.speed = posData.velocity;
    status.load = posData.load;
    
    // 读取电压
    std::vector<uint8_t> voltageData;
    if (read(id, MEM_ADDR_PRESENT_VOLTAGE, 1, voltageData) && !voltageData.empty()) {
        status.voltage = voltageData[0];
    }
    
    // 读取温度
    std::vector<uint8_t> tempData;
    if (read(id, MEM_ADDR_PRESENT_TEMPERATURE, 1, tempData) && !tempData.empty()) {
        status.temperature = tempData[0];
    }
    
    // 读取电流
    std::vector<uint8_t> currentData;
    if (read(id, MEM_ADDR_PRESENT_CURRENT, 2, currentData) && currentData.size() >= 2) {
        status.current = currentData[0] | (currentData[1] << 8);
    }
    
    // 读取运动状态
    std::vector<uint8_t> movingData;
    if (read(id, MEM_ADDR_MOVING, 1, movingData) && !movingData.empty()) {
        status.moving = (movingData[0] != 0);
    }
    
    return true;
}

// 更改舵机ID
bool ST3215::changeId(uint8_t oldId, uint8_t newId) {
    // 解锁EPROM
    if (!write_int(oldId, MEM_ADDR_EPROM_LOCK, 0)) {
        return false;
    }
    
    // 设置新ID
    if (!write_int(oldId, MEM_ADDR_ID, newId)) {
        return false;
    }
    
    // 锁定EPROM
    return write_int(newId, MEM_ADDR_EPROM_LOCK, 1);
}

// 设置位置校正
bool ST3215::setPositionCorrection(uint8_t id, int16_t correction, bool save) {
    if (_model == SCS_MODEL) {
        if (_debugEnabled) {
            Serial.println("Position correction not available for SCS servos");
        }
        return false;
    }
    
    if (correction > 2047 || correction < -2047) {
        if (_debugEnabled) {
            Serial.println("Correction value out of range");
        }
        return false;
    }
    
    uint16_t correctionValue;
    if (correction >= 0) {
        correctionValue = correction;
    } else {
        correctionValue = (-correction) + 0x800;
    }
    
    std::vector<uint8_t> data = {
        static_cast<uint8_t>(correctionValue & 0xFF),
        static_cast<uint8_t>((correctionValue >> 8) & 0xFF)
    };
    
    if (save) {
        // 解锁EPROM
        if (!write_int(id, MEM_ADDR_EPROM_LOCK, 0)) {
            return false;
        }
    }
    
    bool result = write_bytes(id, MEM_ADDR_STEP_CORR, data);
    
    if (save) {
        // 锁定EPROM
        write_int(id, MEM_ADDR_EPROM_LOCK, 1);
    }
    
    return result;
}

// 获取位置校正
bool ST3215::getPositionCorrection(uint8_t id, int16_t& correction) {
    if (_model == SCS_MODEL) {
        if (_debugEnabled) {
            Serial.println("Position correction not available for SCS servos");
        }
        return false;
    }
    
    std::vector<uint8_t> data;
    if (read(id, MEM_ADDR_STEP_CORR, 2, data) && data.size() >= 2) {
        uint16_t correctionValue = data[0] | (data[1] << 8);
        
        if (correctionValue > 0x800) {
            correction = -(correctionValue - 0x800);
        } else {
            correction = correctionValue;
        }
        
        return true;
    }
    
    return false;
}

// 启用/禁用调试
void ST3215::enableDebug(bool enable) {
    _debugEnabled = enable;
}


// ==== EncoderUnwrapper 类实现 ====
EncoderUnwrapper::EncoderUnwrapper(int32_t currentPosition, uint16_t maxPosition) 
    : lastRaw(-1), position(currentPosition), totalSteps(maxPosition + 1), initialized(false) {
}

void EncoderUnwrapper::update(uint16_t rawValue) {
    if (!initialized) {
        lastRaw = rawValue;
        initialized = true;
        return;
    }
    
    // 计算相对变化量
    int32_t delta = rawValue - lastRaw;
    
    // 处理跳变：如果跨过了0点
    if (delta > totalSteps / 2) {  // 逆向跳变
        delta -= totalSteps;
    } else if (delta < -totalSteps / 2) {  // 顺向跳变
        delta += totalSteps;
    }
    
    // 累加
    position += delta;
    lastRaw = rawValue;
}

int32_t EncoderUnwrapper::getPosition() const {
    return position;
}

float EncoderUnwrapper::getDegrees() const {
    return position * (360.0f / totalSteps);
}

void EncoderUnwrapper::reset(int32_t newPosition) {
    position = newPosition;
    initialized = false;
}

// ==== Python兼容接口实现 ==== 

bool ST3215::move2Posi(const std::vector<uint8_t>& id_arr, const std::vector<uint16_t>& positions, 
                       uint16_t velocity, uint8_t acceleration) {
    return moveToPosition(id_arr, positions, velocity, acceleration);
}

std::map<uint8_t, PositionData> ST3215::readPosi(const std::vector<uint8_t>& id_arr) {
    std::map<uint8_t, PositionData> results;
    readPosition(id_arr, results);
    return results;
} 