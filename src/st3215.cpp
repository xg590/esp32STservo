#include "st3215.h"

// ST3215构造函数
ST3215::ST3215(HardwareSerial& serial, uint32_t baudrate, bool debugEnabled) 
    : STServo(serial, baudrate, debugEnabled) {
    // 设置为STS模型
    setModel(STServo::STS_MODEL);
}

// ST3215析构函数
ST3215::~ST3215() {
    // 基类析构函数会自动调用
}

// 设置力矩模式
bool ST3215::setTorqueMode(uint8_t dev_id, TorqueMode mode) {
    uint8_t error = 0;
    std::vector<uint8_t> params_rx;
    return write_int(dev_id, MEM_ADDR_TORQUE_SWITCH, static_cast<int>(mode), error, params_rx);
}

// 设置加速度（多个舵机）
bool ST3215::setAcceleration(const std::vector<uint8_t>& dev_id_vec, 
                             const std::vector<uint8_t>& acc_vec) {
    if (dev_id_vec.size() != acc_vec.size()) {
        if (_debugEnabled) {
            Serial.println("ID count and acceleration count mismatch");
        }
        return false;
    }
    
    std::vector<std::vector<uint8_t>> dataArray;
    
    for (const uint8_t& acc : acc_vec) {
        std::vector<uint8_t> data = {acc};
        dataArray.push_back(data);
    }
    
    return sync_write(dev_id_vec, MEM_ADDR_ACC, dataArray);
}

// 读取加速度（多个舵机）
bool ST3215::getAcceleration(const std::vector<uint8_t>& dev_id_vec, 
                                   std::vector<uint8_t>& acc_vec) {
    acc_vec.clear();
    std::vector<std::vector<uint8_t>> rawData;
    if (sync_read(dev_id_vec, MEM_ADDR_ACC, 1, rawData)) {
        acc_vec.resize(rawData.size());
        for (size_t i = 0; i < rawData.size(); i++) {
            const std::vector<uint8_t>& params_rx = rawData[i];
            if (params_rx.size() >= 1) {
                acc_vec[i] = params_rx[0];
            }
        }
        return !acc_vec.empty();
    }
    return false;
}

// 设置位置（多个舵机）
bool ST3215::setPosition(const std::vector<uint8_t >& dev_id_vec, 
                         const std::vector<uint16_t>& posi_vec, 
                         const std::vector<uint16_t>& velo_vec) {
    if (dev_id_vec.size() != posi_vec.size() || dev_id_vec.size() != velo_vec.size()) {
        if (_debugEnabled) {
            Serial.println("ID count, position count and velocity count mismatch");
        }
        return false;
    }
    
    std::vector<std::vector<uint8_t>> dataArray;
    
    for (size_t i = 0; i < posi_vec.size(); i++) {
        uint16_t posi = posi_vec[i];
        uint16_t velo = velo_vec[i];
        
        if (posi > 0x0FFF) {
            if (_debugEnabled) {
                Serial.printf("Position value %d too large\n", posi);
            }
            return false;
        }
        
        uint8_t posL, posH;
        uint8_t velL, velH;
        intToBytes(posi, posL, posH);
        intToBytes(velo, velL, velH);
        
        std::vector<uint8_t> data = {
            posL, posH,      // 目标位置
            0x00, 0x00,      // 目标时间
            velL, velH       // 目标速度
        };
        
        dataArray.push_back(data);
    }
    
    return sync_write(dev_id_vec, MEM_ADDR_GOAL_POSITION, dataArray);
}

// 读取位置信息（单个舵机）
bool ST3215::getPosition(uint8_t dev_id, uint16_t& posi) {
    uint8_t error = 0;
    std::vector<uint8_t> params_rx;
    if (read(dev_id, MEM_ADDR_PRESENT_POSITION, 2, error, params_rx) && params_rx.size() == 2) {
        posi = bytesToInt(params_rx[0], params_rx[1]); 
        return true;
    }
    
    return false;
}

// 读取位置和速度信息（多个舵机）
bool ST3215::getPosition(const std::vector<uint8_t >& dev_id_vec, 
                               std::vector<uint16_t>& posi_vec, 
                               std::vector<uint16_t>& velo_vec) {
    posi_vec.clear();
    velo_vec.clear();

    std::vector<std::vector<uint8_t>> rawData;
    if (sync_read(dev_id_vec, MEM_ADDR_PRESENT_POSITION, 4, rawData)) {
        // posi_vec.clear()清空了posi_vec，所以需要重新设置大小
        posi_vec.resize(rawData.size()); 
        // velo_vec.clear()清空了velo_vec，所以需要重新设置大小
        velo_vec.resize(rawData.size()); 
        
        for (size_t i = 0; i < rawData.size(); i++) {
            const std::vector<uint8_t>& params_rx = rawData[i];
            if (params_rx.size() == 4) {   
                uint16_t posi = bytesToInt(params_rx[0], params_rx[1]);
                uint16_t velo = bytesToInt(params_rx[2], params_rx[3]);
                posi_vec[i] = posi;
                velo_vec[i] = velo;
            }else{
                if (_debugEnabled) {
                    Serial.printf("GetPosition:❌ dev_id:%d params_rx.size() != 4\n", dev_id_vec[i]);
                }
                return false;
            }
        }
        return !posi_vec.empty();
    }
    
    return false;
}

// 读取舵机完整状态
bool ST3215::getStatus(uint8_t dev_id, ServoStatus& status) {
    uint8_t error = 0;
    std::vector<uint8_t> params_rx;
    if (read(dev_id, MEM_ADDR_PRESENT_POSITION, 15, error, params_rx) && !params_rx.empty()) {
        status.posi = bytesToInt(params_rx[0], params_rx[1]);
        status.velo = bytesToInt(params_rx[2], params_rx[3]);
        status.load = bytesToInt(params_rx[4], params_rx[5]);
        status.volt = params_rx[6];
        status.temp = params_rx[7];
        status.asyn = params_rx[8];
        status.stat = params_rx[9];
        status.mvng = (params_rx[10] != 0);
        status.curr = bytesToInt(params_rx[13], params_rx[14]);
        return true;
    }
    return false;
}

// 更改舵机ID
bool ST3215::changeId(uint8_t old_dev_id, uint8_t new_dev_id) {
    uint8_t error = 0;
    std::vector<uint8_t> params_rx;
    
    // 解锁EPROM
    if (!write_int(old_dev_id, MEM_ADDR_EPROM_LOCK, 0, error, params_rx)) {
        if (_debugEnabled) {
            Serial.printf("ChangeId:❌ dev_id:%d write_int(MEM_ADDR_EPROM_LOCK, 0) failed\n", old_dev_id);
        }
        return false;
    }
    
    // 设置新ID
    error = 0;
    params_rx.clear();
    if (!write_int(old_dev_id, MEM_ADDR_ID, new_dev_id, error, params_rx)) {
        if (_debugEnabled) {
            Serial.printf("ChangeId:❌ dev_id:%d write_int(MEM_ADDR_ID, %d) failed\n", old_dev_id, new_dev_id);
        }
        return false;
    }
    
    // 锁定EPROM
    error = 0;
    params_rx.clear();
    return write_int(new_dev_id, MEM_ADDR_EPROM_LOCK, 1, error, params_rx);
}

// 设置位置校正
bool ST3215::setPositionCorrection(uint8_t dev_id, int16_t correction, bool save) {
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
    
    uint8_t corrL, corrH;
    intToBytes(correctionValue, corrL, corrH);
    std::vector<uint8_t> data = {corrL, corrH};
    
    uint8_t error = 0;
    std::vector<uint8_t> params_rx;
    
    if (save) {
        // 解锁EPROM
        if (!write_int(dev_id, MEM_ADDR_EPROM_LOCK, 0, error, params_rx)) { 
            return false;
        }
    }
    
    error = 0;
    params_rx.clear();
    bool result = write_data(dev_id, MEM_ADDR_STEP_CORR, data, error, params_rx);
    
    error = 0;
    params_rx.clear();
    if (save) {
        // 锁定EPROM
        write_int(dev_id, MEM_ADDR_EPROM_LOCK, 1, error, params_rx);
    }
    
    return result;
}

// 获取位置校正
bool ST3215::getPositionCorrection(uint8_t dev_id, int16_t& correction) {
    if (_model == SCS_MODEL) {
        if (_debugEnabled) {
            Serial.println("Position correction not available for SCS servos");
        }
        return false;
    }
    
    uint8_t error = 0;
    std::vector<uint8_t> params_rx;
    if (read(dev_id, MEM_ADDR_STEP_CORR, 2, error, params_rx) && params_rx.size() >= 2) {
        uint16_t correctionValue = bytesToInt(params_rx[0], params_rx[1]);
        
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
    setDebug(enable);
}
