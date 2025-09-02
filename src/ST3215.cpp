#include "ST3215.h"

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

// 静态方法实现
const BaudRateMap* ST3215::getBaudRates() {
    return BAUD_RATES_TABLE;
}

int ST3215::getBaudRateCount() {
    return sizeof(BAUD_RATES_TABLE) / sizeof(BaudRateMap);
}

// 构造函数
ST3215::ST3215(HardwareSerial& serial, uint32_t baudrate) 
    : _serial(&serial), _model(STS_MODEL), _debugEnabled(false), _timeout(3000) {
    // GPIO引脚定义（在构造函数中定义，避免头文件依赖）
    const int SERVO_RX_PIN = 18;
    const int SERVO_TX_PIN = 19;
    _serial->begin(baudrate, SERIAL_8N1, SERVO_RX_PIN, SERVO_TX_PIN);
    updateMemoryMap();
}

// 析构函数
ST3215::~ST3215() {
    end();
}

// 初始化方法
bool ST3215::begin() {
    _serial->setTimeout(_timeout);
    return true;
}

void ST3215::end() {
    if (_serial) {
        _serial->end();
    }
}

// 设置舵机型号
bool ST3215::setModel(ServoModel model) {
    _model = model;
    updateMemoryMap();
    return true;
}

// 获取舵机型号
ServoModel ST3215::getModel() const {
    return _model;
}

// 更新内存地址映射
void ST3215::updateMemoryMap() {
    if (_model == STS_MODEL) {
        MEM_ADDR_ID = STSMemoryMap::ID;
        MEM_ADDR_BAUD_RATE = STSMemoryMap::BAUD_RATE;
        MEM_ADDR_STEP_CORR = STSMemoryMap::STEP_CORR;
        MEM_ADDR_MODE = STSMemoryMap::MODE;
        MEM_ADDR_TORQUE_ENABLE = STSMemoryMap::TORQUE_ENABLE;
        MEM_ADDR_ACC = STSMemoryMap::ACC;
        MEM_ADDR_GOAL_POSITION = STSMemoryMap::GOAL_POSITION;
        MEM_ADDR_GOAL_TIME = STSMemoryMap::GOAL_TIME;
        MEM_ADDR_GOAL_SPEED = STSMemoryMap::GOAL_SPEED;
        MEM_ADDR_EPROM_LOCK = STSMemoryMap::EPROM_LOCK;
        MEM_ADDR_PRESENT_POSITION = STSMemoryMap::PRESENT_POSITION;
        MEM_ADDR_PRESENT_SPEED = STSMemoryMap::PRESENT_SPEED;
        MEM_ADDR_PRESENT_LOAD = STSMemoryMap::PRESENT_LOAD;
        MEM_ADDR_PRESENT_VOLTAGE = STSMemoryMap::PRESENT_VOLTAGE;
        MEM_ADDR_PRESENT_TEMPERATURE = STSMemoryMap::PRESENT_TEMPERATURE;
        MEM_ADDR_MOVING = STSMemoryMap::MOVING;
        MEM_ADDR_PRESENT_CURRENT = STSMemoryMap::PRESENT_CURRENT;
    } else if (_model == SCS_MODEL) {
        MEM_ADDR_EPROM_LOCK = SCSMemoryMap::EPROM_LOCK;
    }
}

// 生成数据包
std::vector<uint8_t> ST3215::makePacket(uint8_t id, uint8_t instruction, const std::vector<uint8_t>& params) {
    std::vector<uint8_t> packet;
    
    // 包头
    packet.push_back(0xFF);
    packet.push_back(0xFF);
    
    // ID
    packet.push_back(id);
    
    // 长度
    uint8_t length = params.size() + 2;
    packet.push_back(length);
    
    // 指令
    packet.push_back(instruction);
    
    // 参数
    for (uint8_t param : params) {
        packet.push_back(param);
    }
    
    // 校验和
    uint8_t checksum = calculateChecksum(packet);
    packet.push_back(checksum);
    
    return packet;
}

// 接收数据包
bool ST3215::receivePacket(uint8_t id, std::vector<uint8_t>& params, bool paramsInBytes) {
    params.clear();
    
    // 等待包头
    unsigned long startTime = millis();
    while (millis() - startTime < _timeout) {
        if (_serial->available() >= 2) {
            uint8_t header1 = _serial->read();
            uint8_t header2 = _serial->read();
            if (header1 == 0xFF && header2 == 0xFF) {
                break;
            }
        }
        delay(1);
    }
    
    // 检查超时
    if (millis() - startTime >= _timeout) {
        if (_debugEnabled) {
            Serial.println("Timeout waiting for packet header");
        }
        return false;
    }
    
    // 读取ID
    if (_serial->available() < 1) {
        delay(10);
    }
    uint8_t receivedId = _serial->read();
    
    // 读取长度
    if (_serial->available() < 1) {
        delay(10);
    }
    uint8_t length = _serial->read();
    
    // 读取数据
    std::vector<uint8_t> data;
    for (int i = 0; i < length; i++) {
        while (_serial->available() < 1) {
            delay(1);
            if (millis() - startTime >= _timeout) {
                if (_debugEnabled) {
                    Serial.println("Timeout reading packet data");
                }
                return false;
            }
        }
        data.push_back(_serial->read());
    }
    
    // 验证ID
    if (receivedId != id) {
        if (_debugEnabled) {
            Serial.printf("ID mismatch: expected %d, got %d\n", id, receivedId);
        }
        return false;
    }
    
    // 验证校验和
    std::vector<uint8_t> checksumData = {receivedId, length};
    checksumData.insert(checksumData.end(), data.begin(), data.end() - 1);
    uint8_t expectedChecksum = calculateChecksum(checksumData);
    uint8_t receivedChecksum = data.back();
    
    if (expectedChecksum != receivedChecksum) {
        if (_debugEnabled) {
            Serial.printf("Checksum error: expected 0x%02X, got 0x%02X\n", expectedChecksum, receivedChecksum);
        }
        return false;
    }
    
    // 提取参数
    if (paramsInBytes) {
        params.assign(data.begin() + 1, data.end() - 1);
    } else {
        params.assign(data.begin() + 1, data.end() - 1);
    }
    
    return true;
}

// 计算校验和
uint8_t ST3215::calculateChecksum(const std::vector<uint8_t>& data) {
    int sum = 0;
    for (size_t i = 2; i < data.size(); i++) {  // 跳过包头的两个0xFF
        sum += data[i];
    }
    return 255 - (sum & 0xFF);
}

// 整数转字节数组
std::vector<uint8_t> ST3215::intToByteArray(int value) {
    std::vector<uint8_t> bytes;
    
    if (value < 256) {
        bytes.push_back(value & 0xFF);
    } else if (value < 65536) {
        bytes.push_back(value & 0xFF);
        bytes.push_back((value >> 8) & 0xFF);
    } else {
        // 对于更大的值，可以扩展
        bytes.push_back(value & 0xFF);
        bytes.push_back((value >> 8) & 0xFF);
        bytes.push_back((value >> 16) & 0xFF);
        bytes.push_back((value >> 24) & 0xFF);
    }
    
    return bytes;
}

// 字节数组转整数
int ST3215::byteArrayToInt(const std::vector<uint8_t>& bytes) {
    int value = 0;
    for (size_t i = 0; i < bytes.size() && i < 4; i++) {
        value |= (bytes[i] << (i * 8));
    }
    return value;
}

// Ping指令
bool ST3215::ping(uint8_t id) {
    std::vector<uint8_t> params;
    std::vector<uint8_t> packet = makePacket(id, INST_PING, params);
    
    if (_debugEnabled) {
        printPacket(packet);
    }
    
    _serial->write(packet.data(), packet.size());
    
    std::vector<uint8_t> response;
    return receivePacket(id, response);
}

// 读取指令
bool ST3215::read(uint8_t id, uint8_t address, uint8_t length, std::vector<uint8_t>& data) {
    std::vector<uint8_t> params = {address, length};
    std::vector<uint8_t> packet = makePacket(id, INST_READ, params);
    
    if (_debugEnabled) {
        printPacket(packet);
    }
    
    _serial->write(packet.data(), packet.size());
    
    return receivePacket(id, data, true);
}

// 写入指令（字节数组版本）
bool ST3215::write(uint8_t id, uint8_t address, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> params = {address};
    params.insert(params.end(), data.begin(), data.end());
    std::vector<uint8_t> packet = makePacket(id, INST_WRITE, params);
    
    if (_debugEnabled) {
        printPacket(packet);
    }
    
    _serial->write(packet.data(), packet.size());
    
    std::vector<uint8_t> response;
    return receivePacket(id, response);
}

// 写入指令（整数版本）
bool ST3215::write(uint8_t id, uint8_t address, int value) {
    std::vector<uint8_t> data = intToByteArray(value);
    return write(id, address, data);
}

// 寄存器写入指令
bool ST3215::regWrite(uint8_t id, uint8_t address, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> params = {address};
    params.insert(params.end(), data.begin(), data.end());
    std::vector<uint8_t> packet = makePacket(id, INST_REG_WRITE, params);
    
    if (_debugEnabled) {
        printPacket(packet);
    }
    
    _serial->write(packet.data(), packet.size());
    
    std::vector<uint8_t> response;
    return receivePacket(id, response);
}

// 动作指令
bool ST3215::action() {
    // 广播动作指令，无返回
    uint8_t actionPacket[] = {0xFF, 0xFF, 0xFE, 0x02, 0x05, 0xFA};
    _serial->write(actionPacket, sizeof(actionPacket));
    
    if (_debugEnabled) {
        Serial.println("Action broadcast sent");
    }
    
    return true;
}

// 同步写入
bool ST3215::syncWrite(const std::vector<uint8_t>& ids, uint8_t address, 
                       const std::vector<std::vector<uint8_t>>& dataArray) {
    if (ids.size() != dataArray.size()) {
        if (_debugEnabled) {
            Serial.println("ID count and data array count mismatch");
        }
        return false;
    }
    
    std::vector<uint8_t> params;
    params.push_back(address);
    
    if (!dataArray.empty()) {
        params.push_back(dataArray[0].size());
        
        for (size_t i = 0; i < ids.size(); i++) {
            params.push_back(ids[i]);
            params.insert(params.end(), dataArray[i].begin(), dataArray[i].end());
        }
    }
    
    std::vector<uint8_t> packet = makePacket(0xFE, INST_SYNC_WRITE, params);
    
    if (_debugEnabled) {
        printPacket(packet);
    }
    
    _serial->write(packet.data(), packet.size());
    
    return true;
}

// 同步读取
bool ST3215::syncRead(const std::vector<uint8_t>& ids, uint8_t address, uint8_t length,
                      std::map<uint8_t, std::vector<uint8_t>>& results) {
    results.clear();
    
    std::vector<uint8_t> params = {address, length};
    params.insert(params.end(), ids.begin(), ids.end());
    std::vector<uint8_t> packet = makePacket(0xFE, INST_SYNC_READ, params);
    
    if (_debugEnabled) {
        printPacket(packet);
    }
    
    _serial->write(packet.data(), packet.size());
    
    // 接收每个舵机的响应
    for (uint8_t id : ids) {
        std::vector<uint8_t> response;
        if (receivePacket(id, response, true)) {
            results[id] = response;
        } else {
            if (_debugEnabled) {
                Serial.printf("Failed to receive response from servo %d\n", id);
            }
        }
    }
    
    return !results.empty();
}

// 设置工作模式
bool ST3215::setMode(uint8_t id, ServoMode mode) {
    if (_model == SCS_MODEL) {
        if (_debugEnabled) {
            Serial.println("SCS servos only support position mode");
        }
        return false;
    }
    
    return write(id, MEM_ADDR_MODE, static_cast<int>(mode));
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
    return write(id, MEM_ADDR_TORQUE_ENABLE, static_cast<int>(mode));
}

// 移动到指定位置（单个舵机）
bool ST3215::moveToPosition(uint8_t id, uint16_t position, uint16_t velocity, uint8_t acceleration) {
    if (acceleration > 254) {
        if (_debugEnabled) {
            Serial.println("Acceleration value too large");
        }
        return false;
    }
    
    if (velocity > 0xFFFF) {
        if (_debugEnabled) {
            Serial.println("Velocity value too large");
        }
        return false;
    }
    
    if (position > 0x0FFF) {
        if (_debugEnabled) {
            Serial.println("Position value too large");
        }
        return false;
    }
    
    // 构建数据包
    std::vector<uint8_t> data = {
        acceleration,
        static_cast<uint8_t>(position & 0xFF),
        static_cast<uint8_t>((position >> 8) & 0xFF),
        0x00, 0x00,  // 目标时间（暂时不使用）
        static_cast<uint8_t>(velocity & 0xFF),
        static_cast<uint8_t>((velocity >> 8) & 0xFF)
    };
    
    return write(id, MEM_ADDR_ACC, data);
}

// 移动到指定位置（多个舵机）
bool ST3215::moveToPosition(const std::vector<uint8_t>& ids, const std::vector<uint16_t>& positions, 
                            uint16_t velocity, uint8_t acceleration) {
    if (ids.size() != positions.size()) {
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
    
    return syncWrite(ids, MEM_ADDR_ACC, dataArray);
}

// 读取位置信息（单个舵机）
bool ST3215::readPosition(uint8_t id, PositionData& data) {
    std::vector<uint8_t> response;
    if (read(id, MEM_ADDR_PRESENT_POSITION, 6, response) && response.size() >= 6) {
        data.position = response[0] | (response[1] << 8);
        data.velocity = response[2] | (response[3] << 8);
        data.load = response[4] | (response[5] << 8);
        return true;
    }
    
    return false;
}

// 读取位置信息（多个舵机）
bool ST3215::readPosition(const std::vector<uint8_t>& ids, std::map<uint8_t, PositionData>& results) {
    results.clear();
    
    std::map<uint8_t, std::vector<uint8_t>> rawResults;
    if (syncRead(ids, MEM_ADDR_PRESENT_POSITION, 6, rawResults)) {
        for (const auto& pair : rawResults) {
            uint8_t id = pair.first;
            const std::vector<uint8_t>& response = pair.second;
            
            if (response.size() >= 6) {
                PositionData data;
                data.position = response[0] | (response[1] << 8);
                data.velocity = response[2] | (response[3] << 8);
                data.load = response[4] | (response[5] << 8);
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
    if (!write(oldId, MEM_ADDR_EPROM_LOCK, 0)) {
        return false;
    }
    
    // 设置新ID
    if (!write(oldId, MEM_ADDR_ID, newId)) {
        return false;
    }
    
    // 锁定EPROM
    return write(newId, MEM_ADDR_EPROM_LOCK, 1);
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
        if (!write(id, MEM_ADDR_EPROM_LOCK, 0)) {
            return false;
        }
    }
    
    bool result = write(id, MEM_ADDR_STEP_CORR, data);
    
    if (save) {
        // 锁定EPROM
        write(id, MEM_ADDR_EPROM_LOCK, 1);
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

// 打印数据包（调试用）
void ST3215::printPacket(const std::vector<uint8_t>& packet) {
    Serial.print("Packet: ");
    for (uint8_t byte : packet) {
        Serial.printf("0x%02X ", byte);
    }
    Serial.println();
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
bool ST3215::move2Posi(uint8_t id, uint16_t position, uint16_t velocity, uint8_t acceleration) {
    return moveToPosition(id, position, velocity, acceleration);
}

bool ST3215::move2Posi(const std::vector<uint8_t>& ids, const std::vector<uint16_t>& positions, 
                       uint16_t velocity, uint8_t acceleration) {
    return moveToPosition(ids, positions, velocity, acceleration);
}

std::map<uint8_t, PositionData> ST3215::readPosi(const std::vector<uint8_t>& ids) {
    std::map<uint8_t, PositionData> results;
    readPosition(ids, results);
    return results;
}

std::map<uint8_t, PositionData> ST3215::readPosi(uint8_t id) {
    std::vector<uint8_t> ids = {id};
    return readPosi(ids);
}