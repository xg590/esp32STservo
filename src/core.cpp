
#include <Arduino.h>
#include <vector>
#include "core.h"

// 更新内存地址映射
void STServo::update_memory_map() {
    if (_model == STServo::STS_MODEL) {
        MEM_ADDR_SMS_STS_MODEL       = STSMemoryMap::SMS_STS_MODEL;

        MEM_ADDR_ID                  = STSMemoryMap::ID;
        MEM_ADDR_BAUD_RATE           = STSMemoryMap::BAUD_RATE;
        MEM_ADDR_STEP_CORR           = STSMemoryMap::STEP_CORR;
        MEM_ADDR_MODE                = STSMemoryMap::MODE;

        MEM_ADDR_TORQUE_SWITCH       = STSMemoryMap::TORQUE_SWITCH;
        MEM_ADDR_ACC                 = STSMemoryMap::ACC;
        MEM_ADDR_GOAL_POSITION       = STSMemoryMap::GOAL_POSITION;
        MEM_ADDR_GOAL_TIME           = STSMemoryMap::GOAL_TIME;
        MEM_ADDR_GOAL_SPEED          = STSMemoryMap::GOAL_SPEED;
        MEM_ADDR_EPROM_LOCK          = STSMemoryMap::EPROM_LOCK;

        MEM_ADDR_PRESENT_POSITION    = STSMemoryMap::PRESENT_POSITION;
        MEM_ADDR_PRESENT_SPEED       = STSMemoryMap::PRESENT_SPEED;
        MEM_ADDR_PRESENT_LOAD        = STSMemoryMap::PRESENT_LOAD;
        MEM_ADDR_PRESENT_VOLTAGE     = STSMemoryMap::PRESENT_VOLTAGE;
        MEM_ADDR_PRESENT_TEMPERATURE = STSMemoryMap::PRESENT_TEMPERATURE;
        MEM_ADDR_SERVO_STATUS        = STSMemoryMap::SERVO_STATUS;
        MEM_ADDR_ASYNC_ACTION        = STSMemoryMap::ASYNC_ACTION;
        MEM_ADDR_MOVING              = STSMemoryMap::MOVING;
        MEM_ADDR_PRESENT_CURRENT     = STSMemoryMap::PRESENT_CURRENT;
    } else if (_model == STServo::SCS_MODEL) {
        MEM_ADDR_EPROM_LOCK          = SCSMemoryMap::EPROM_LOCK;
    }
}

// 构造函数
STServo::STServo(HardwareSerial& serial, uint32_t baudrate, bool debugEnabled) 
    : _serial(&serial), _model(STServo::STS_MODEL), _debugEnabled(debugEnabled), _timeout(3000) {
    // GPIO引脚定义（在构造函数中定义，避免头文件依赖）
    const int SERVO_RX_PIN = 18;
    const int SERVO_TX_PIN = 19;
    _serial->begin(baudrate, SERIAL_8N1, SERVO_RX_PIN, SERVO_TX_PIN);
    
    // 初始化内存地址映射
    update_memory_map();
}

// 初始化方法
bool STServo::begin() {
    _serial->setTimeout(_timeout); 
    return true;
}

void STServo::end() {
    if (_serial) {
        _serial->end();
    }
}

// 析构函数
STServo::~STServo() {
    end();
}

// 计算校验和
uint8_t STServo::calculate_checksum(const std::vector<uint8_t>& data) {
    int sum = 0;
    for (size_t i = 2; i < data.size(); i++) {  // 跳过包头的两个0xFF
        sum += data[i];
    }
    return 255 - (sum & 0xFF);
}

// 打印数据包（调试用）
void STServo::printPacket(const std::vector<uint8_t>& packet) { 
    if (packet.empty()) {
        Serial.printf("[Func printPacket()] Packet is empty\n");
    } else {
        Serial.printf("[Func printPacket()] Packet (%d bytes): ", packet.size());
        for (const uint8_t& byte : packet) Serial.printf("0x%02X ", byte); 
        Serial.println();
    }
    return ;
}

// 将int转换为两个uint8_t（小端序）
void STServo::intToBytes(int value, uint8_t& lowByte, uint8_t& highByte) {
    lowByte = value & 0xFF;         // 低字节：取最低8位
    highByte = (value >> 8) & 0xFF; // 高字节：取高8位
}

// 将两个uint8_t转换为int（小端序）
int STServo::bytesToInt(uint8_t lowByte, uint8_t highByte) {
    return lowByte | (highByte << 8); // 小端序：低字节在前，高字节在后
}

// 生成数据包
std::vector<uint8_t> STServo::make_a_packet(uint8_t dev_id, uint8_t instruction, const std::vector<uint8_t>& params_tx) { 
    std::vector<uint8_t> packet;
    
    // 包头
    packet.push_back(0xFF);
    packet.push_back(0xFF);
    
    // ID
    packet.push_back(dev_id);
    
    // 长度
    uint8_t length = params_tx.size() + 2;
    packet.push_back(length);
    
    // 指令
    packet.push_back(instruction);
    
    // 参数
    for (uint8_t param : params_tx) {
        packet.push_back(param);
    }
    
    // 校验和
    uint8_t checksum = calculate_checksum(packet);
    packet.push_back(checksum);
    
    if (_debugEnabled) {
        printPacket(packet);
    }

    return packet;
}

// 串口读取函数 - 封装完整的等待+超时+读取逻辑
uint8_t STServo::serial_read_a_byte(const std::string& errMsg) { 
    unsigned long startTime = millis();
    int byte = -1;
    while (millis() - startTime < _timeout) {
        byte = _serial->read();
        if (byte != -1) {
            return byte;
        }
        delay(1); // 短暂等待
    }
    throw SerialTimeoutException(errMsg.c_str());
}

// 接收数据包 - 按照标准舵机应答包格式: header(2) + ID(1) + length(1) + error(1) + params(length-2) + checksum(1)
bool STServo::receive_packet(uint8_t dev_id, uint8_t& error, std::vector<uint8_t>& params_rx) {
    params_rx.clear();

    // 读取包头 (2字节，应为0xFF 0xFF)
    uint8_t header1 = serial_read_a_byte("[Timeout] Reading packet header byte 1");
    uint8_t header2 = serial_read_a_byte("[Timeout] Reading packet header byte 2");
    if (header1 != 0xFF || header2 != 0xFF) { 
        Serial.printf("[Error] [STServo::receive_packet()] Header mismatch: got 0x%02X 0x%02X, expected 0xFF 0xFF\n", header1, header2); 
        return false;
    }

    // 读取ID (1字节)
    uint8_t receivedId = serial_read_a_byte("[Timeout] Reading packet ID");
    if (receivedId != dev_id) {
        Serial.printf("[Error] [STServo::receive_packet()] ID mismatch: expected %d, got %d\n", dev_id, receivedId); 
        return false;
    }

    // 读取长度 (1字节)
    uint8_t length = serial_read_a_byte("[Timeout] Reading packet length");
    int paramsLength = length - 2;  // 减去error(1字节)和checksum(1字节)

    // 读取状态 (1字节)
    error = serial_read_a_byte("[Timeout] Reading packet error");

    // 读取参数数据 (length-2字节，因为length包含error和checksum)
    if (_debugEnabled) {
        Serial.printf("[Debug] [STServo::receive_packet()] Reading param byte: "); 
    }
    for (int i = 0; i < paramsLength; i++) { 
        uint8_t byte = serial_read_a_byte("[Timeout] Reading param byte"); 
        params_rx.push_back(byte);
    }

    // 读取校验和 (1字节)
    uint8_t receivedChecksum = serial_read_a_byte("[Timeout] Reading packet checksum");

    // 验证校验和
    std::vector<uint8_t> checksumData = {0xFF, 0xFF, receivedId, length, error};
    checksumData.insert(checksumData.end(), params_rx.begin(), params_rx.end());
    uint8_t expectedChecksum = calculate_checksum(checksumData);
    if (expectedChecksum != receivedChecksum) {
        Serial.printf("[Error] [STServo::receive_packet()] Checksum error: expected 0x%02X, got 0x%02X\n", expectedChecksum, receivedChecksum); 
        return false;
    }
    
    if (_debugEnabled) {
        Serial.printf("[Debug] [STServo::receive_packet()] Packet received: ID=%d, length=%d, error=0x%02X, paramsLength=%d\n",
                     receivedId, length, error, paramsLength);
    }
    
    return true;
}

// Ping指令
bool STServo::ping(uint8_t dev_id, uint8_t& error, std::vector<uint8_t>& params_rx) {
    std::vector<uint8_t> params_tx = {};
    std::vector<uint8_t> packet = make_a_packet(dev_id, STServo::INST_PING, params_tx);
    _serial->write(packet.data(), packet.size());
    
    return receive_packet(dev_id, error, params_rx);
}

// 读取指令
bool STServo::read(uint8_t dev_id, uint8_t mem_addr, uint8_t length, uint8_t& error, std::vector<uint8_t>& params_rx) {
    std::vector<uint8_t> params_tx = {mem_addr, length};
    std::vector<uint8_t> packet = make_a_packet(dev_id, STServo::INST_READ, params_tx);
    _serial->write(packet.data(), packet.size());
    
    return receive_packet(dev_id, error, params_rx);
}

// 写入指令（字节数组版本）
bool STServo::write_data(uint8_t dev_id, uint8_t mem_addr, const std::vector<uint8_t>& data, uint8_t& error, std::vector<uint8_t>& params_rx) {
    std::vector<uint8_t> params_tx = {mem_addr};
    params_tx.insert(params_tx.end(), data.begin(), data.end());
    std::vector<uint8_t> packet = make_a_packet(dev_id, STServo::INST_WRITE, params_tx);
    _serial->write(packet.data(), packet.size());

    return receive_packet(dev_id, error, params_rx);
}

// 写入指令（整数版本）
bool STServo::write_int(uint8_t dev_id, uint8_t mem_addr, int value, uint8_t& error, std::vector<uint8_t>& params_rx) {
    std::vector<uint8_t> data;
    data.push_back(value & 0xFF);
    if (value > 255) {
        data.push_back((value >> 8) & 0xFF);
    }

    return write_data(dev_id, mem_addr, data, error, params_rx);
}

// 寄存器写入指令
bool STServo::reg_write(uint8_t dev_id, uint8_t mem_addr, const std::vector<uint8_t>& data, uint8_t& error, std::vector<uint8_t>& params_rx) {
    std::vector<uint8_t> params_tx = {mem_addr};
    params_tx.insert(params_tx.end(), data.begin(), data.end());
    std::vector<uint8_t> packet = make_a_packet(dev_id, STServo::INST_REG_WRITE, params_tx);
    _serial->write(packet.data(), packet.size());
    
    return receive_packet(dev_id, error, params_rx);
}

// 动作指令
bool STServo::action() {
    // 广播动作指令，无返回 
    std::vector<uint8_t> params_tx = {};
    std::vector<uint8_t> packet = make_a_packet(0xFE, STServo::INST_ACTION, params_tx);
    _serial->write(packet.data(), packet.size()); 

    return true;
}

// 同步写入
bool STServo::sync_write(const std::vector<uint8_t>& dev_id_vec, uint8_t mem_addr, 
                         const std::vector<std::vector<uint8_t>>& params_tx_vec) {
    // 广播动作指令，无返回
    // 传入dev_id_vec和params_tx_vec，params_tx_vec的每个vector元素都是要写入的舵机的数据。
    if (dev_id_vec.empty() || params_tx_vec.empty()) {
        return false;
    }
    if (dev_id_vec.size() != params_tx_vec.size()) {
        return false;
    }
    
    std::vector<uint8_t> params_tx;
    params_tx.push_back(mem_addr); 
    params_tx.push_back(params_tx_vec[0].size());
    for (size_t i = 0; i < dev_id_vec.size(); i++) {
        params_tx.push_back(dev_id_vec[i]);
        params_tx.insert(params_tx.end(), params_tx_vec[i].begin(), params_tx_vec[i].end());
    }
    std::vector<uint8_t> packet = make_a_packet(0xFE, STServo::INST_SYNC_WRITE, params_tx);
    _serial->write(packet.data(), packet.size());
    return true;
}

// 同步读取
bool STServo::sync_read(const std::vector<uint8_t>& dev_id_vec, uint8_t mem_addr, uint8_t length,
                              std::vector<std::vector<uint8_t>>& params_rx_vec) {
    // 每个舵机要读的地址mem_addr和从每个舵机读取的数据的长度length，这里的length不是广播数据（0xFE）的长度，广播长度由make_a_packet计算。

    params_rx_vec.clear();
    
    std::vector<uint8_t> params_tx = {mem_addr, length};
    params_tx.insert(params_tx.end(), dev_id_vec.begin(), dev_id_vec.end());
    std::vector<uint8_t> packet = make_a_packet(0xFE, STServo::INST_SYNC_READ, params_tx); //制作一个广播packet
    _serial->write(packet.data(), packet.size());
    
    // 接收每个舵机的响应
    for (const uint8_t& dev_id : dev_id_vec) {
        std::vector<uint8_t> params_rx = {};
        uint8_t error;
        if (!receive_packet(dev_id, error, params_rx)) {
            Serial.printf("[Func] [STServo::sync_read()] Failed to receive params_rx from servo %d\n", dev_id);
        }
        params_rx_vec.push_back(params_rx);
    }
    
    return !params_rx_vec.empty();
}

// 配置方法实现
void STServo::setDebug(bool enabled) {
    _debugEnabled = enabled;
}

void STServo::setTimeout(uint32_t timeout) {
    _timeout = timeout;
    if (_serial) {
        _serial->setTimeout(_timeout);
    }
}

void STServo::setModel(uint8_t model) {
    _model = model;
    update_memory_map();
}