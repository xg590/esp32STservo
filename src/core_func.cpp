
#include <Arduino.h>
#include <vector>
#include <map>
#include "core_func.h"

// 更新内存地址映射
void ST3215::update_memory_map() {
    if (_model == ST3215::STS_MODEL) {
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
    } else if (_model == ST3215::SCS_MODEL) {
        MEM_ADDR_EPROM_LOCK          = SCSMemoryMap::EPROM_LOCK;
    }
}

// 构造函数
ST3215::ST3215(HardwareSerial& serial, uint32_t baudrate, bool debugEnabled) 
    : _serial(&serial), _model(ST3215::STS_MODEL), _debugEnabled(debugEnabled), _timeout(3000) {
    // GPIO引脚定义（在构造函数中定义，避免头文件依赖）
    const int SERVO_RX_PIN = 18;
    const int SERVO_TX_PIN = 19;
    _serial->begin(baudrate, SERIAL_8N1, SERVO_RX_PIN, SERVO_TX_PIN);
    
    // 初始化内存地址映射
    update_memory_map();
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

// 析构函数
ST3215::~ST3215() {
    end();
}

// 计算校验和
uint8_t ST3215::calculate_checksum(const std::vector<uint8_t>& data) {
    int sum = 0;
    for (size_t i = 2; i < data.size(); i++) {  // 跳过包头的两个0xFF
        sum += data[i];
    }
    return 255 - (sum & 0xFF);
}

// 打印数据包（调试用）
void ST3215::printPacket(const std::vector<uint8_t>& packet) { 
    if (packet.empty()) {
        Serial.printf("[Func printPacket()] Packet is empty\n");
    } else {
        Serial.printf("[Func printPacket()] Packet (%d bytes): ", packet.size());
        for (uint8_t byte : packet) Serial.printf("0x%02X ", byte); 
        Serial.println();
    }
    return ;
}

// 将int转换为两个uint8_t（小端序）
void ST3215::intToBytes(int value, uint8_t& lowByte, uint8_t& highByte) {
    lowByte = value & 0xFF;         // 低字节：取最低8位
    highByte = (value >> 8) & 0xFF; // 高字节：取高8位
}

// 将两个uint8_t转换为int（小端序）
int ST3215::bytesToInt(uint8_t lowByte, uint8_t highByte) {
    return lowByte | (highByte << 8); // 小端序：低字节在前，高字节在后
}

// 生成数据包
std::vector<uint8_t> ST3215::make_a_packet(uint8_t dev_id, uint8_t instruction, const std::vector<uint8_t>& params_tx) { 
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
uint8_t ST3215::serial_read_a_byte(const char* errMsg) { 
    unsigned long startTime = millis();
    int byte = -1;
    while (millis() - startTime < _timeout) {
        byte = _serial->read();
        if (byte != -1) {
            return byte;
        }
        delay(1); // 短暂等待
    }
    throw SerialTimeoutException(errMsg);
}

// 接收数据包 - 按照标准舵机应答包格式: header(2) + ID(1) + length(1) + error(1) + params(length-2) + checksum(1)
bool ST3215::receive_packet(uint8_t dev_id, uint8_t& error, std::vector<uint8_t>& params_rx) {
    params_rx.clear();

    // 读取包头 (2字节，应为0xFF 0xFF)
    uint8_t header1 = serial_read_a_byte("[Timeout] Reading packet header byte 1");
    uint8_t header2 = serial_read_a_byte("[Timeout] Reading packet header byte 2");
    if (header1 != 0xFF || header2 != 0xFF) { 
        Serial.printf("[Error] [ST3215::receive_packet()] Header mismatch: got 0x%02X 0x%02X, expected 0xFF 0xFF\n", header1, header2); 
        return false;
    }

    // 读取ID (1字节)
    uint8_t receivedId = serial_read_a_byte("[Timeout] Reading packet ID");
    if (receivedId != dev_id) {
        Serial.printf("[Error] [ST3215::receive_packet()] ID mismatch: expected %d, got %d\n", dev_id, receivedId); 
        return false;
    }

    // 读取长度 (1字节)
    uint8_t length = serial_read_a_byte("[Timeout] Reading packet length");
    int paramsLength = length - 2;  // 减去error(1字节)和checksum(1字节)

    // 读取状态 (1字节)
    error = serial_read_a_byte("[Timeout] Reading packet error");

    // 读取参数数据 (length-2字节，因为length包含error和checksum)
    if (_debugEnabled) {
        Serial.printf("[Debug] [ST3215::receive_packet()] Reading param byte: "); 
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
        Serial.printf("[Error] [ST3215::receive_packet()] Checksum error: expected 0x%02X, got 0x%02X\n", expectedChecksum, receivedChecksum); 
        return false;
    }
    
    if (_debugEnabled) {
        Serial.printf("[Debug] [ST3215::receive_packet()] Packet received: ID=%d, length=%d, error=0x%02X, paramsLength=%d\n",
                     receivedId, length, error, paramsLength);
    }
    
    return true;
}

// Ping指令
bool ST3215::ping(uint8_t dev_id, uint8_t& error, std::vector<uint8_t>& params_rx) {
    std::vector<uint8_t> params_tx = {};
    std::vector<uint8_t> packet = make_a_packet(dev_id, ST3215::INST_PING, params_tx);
    _serial->write(packet.data(), packet.size());
    
    return receive_packet(dev_id, error, params_rx);
}

// 读取指令
bool ST3215::read(uint8_t dev_id, uint8_t mem_addr, uint8_t length, uint8_t& error, std::vector<uint8_t>& params_rx) {
    std::vector<uint8_t> params_tx = {mem_addr, length};
    std::vector<uint8_t> packet = make_a_packet(dev_id, ST3215::INST_READ, params_tx);
    _serial->write(packet.data(), packet.size());
    
    return receive_packet(dev_id, error, params_rx);
}

// 写入指令（字节数组版本）
bool ST3215::write_data(uint8_t dev_id, uint8_t mem_addr, const std::vector<uint8_t>& data, uint8_t& error, std::vector<uint8_t>& params_rx) {
    std::vector<uint8_t> params_tx = {mem_addr};
    params_tx.insert(params_tx.end(), data.begin(), data.end());
    std::vector<uint8_t> packet = make_a_packet(dev_id, ST3215::INST_WRITE, params_tx);
    _serial->write(packet.data(), packet.size());

    return receive_packet(dev_id, error, params_rx);
}

// 写入指令（整数版本）
bool ST3215::write_int(uint8_t dev_id, uint8_t mem_addr, int value, uint8_t& error, std::vector<uint8_t>& params_rx) {
    std::vector<uint8_t> data;
    data.push_back(value & 0xFF);
    if (value > 255) {
        data.push_back((value >> 8) & 0xFF);
    }

    return write_data(dev_id, mem_addr, data, error, params_rx);
}

// 寄存器写入指令
bool ST3215::reg_write(uint8_t dev_id, uint8_t mem_addr, const std::vector<uint8_t>& data, uint8_t& error, std::vector<uint8_t>& params_rx) {
    std::vector<uint8_t> params_tx = {mem_addr};
    params_tx.insert(params_tx.end(), data.begin(), data.end());
    std::vector<uint8_t> packet = make_a_packet(dev_id, ST3215::INST_REG_WRITE, params_tx);
    _serial->write(packet.data(), packet.size());
    
    return receive_packet(dev_id, error, params_rx);
}

// 动作指令
bool ST3215::action() {
    // 广播动作指令，无返回 
    std::vector<uint8_t> params_tx = {};
    std::vector<uint8_t> packet = make_a_packet(0xFE, ST3215::INST_ACTION, params_tx);
    _serial->write(packet.data(), packet.size()); 

    return true;
}

// 同步写入
bool ST3215::sync_write(const std::vector<uint8_t>& dev_id_arr, uint8_t mem_addr, 
                       const std::vector<std::vector<uint8_t>>& dataArray) {
    // 广播动作指令，无返回
    if (dev_id_arr.empty() || dataArray.empty()) {
        return false;
    }
    if (dev_id_arr.size() != dataArray.size()) {
        return false;
    }
    
    std::vector<uint8_t> params_tx;
    params_tx.push_back(mem_addr); 
    params_tx.push_back(dataArray[0].size());
    for (size_t i = 0; i < dev_id_arr.size(); i++) {
        params_tx.push_back(dev_id_arr[i]);
        params_tx.insert(params_tx.end(), dataArray[i].begin(), dataArray[i].end());
    }
    std::vector<uint8_t> packet = make_a_packet(0xFE, ST3215::INST_SYNC_WRITE, params_tx);
    _serial->write(packet.data(), packet.size());
    return true;
}

// 同步读取
bool ST3215::sync_read(const std::vector<uint8_t>& dev_id_arr, uint8_t mem_addr, uint8_t length,
                      std::map<uint8_t, std::vector<uint8_t>>& params_rx_arr) {
    params_rx_arr.clear();
    
    std::vector<uint8_t> params_tx = {mem_addr, length};
    params_tx.insert(params_tx.end(), dev_id_arr.begin(), dev_id_arr.end());
    std::vector<uint8_t> packet = make_a_packet(0xFE, ST3215::INST_SYNC_READ, params_tx);
    _serial->write(packet.data(), packet.size());
    
    // 接收每个舵机的响应
    for (uint8_t dev_id : dev_id_arr) {
        std::vector<uint8_t> params_rx = {};
        uint8_t error;
        if (!receive_packet(dev_id, error, params_rx)) {
            Serial.printf("[Func] [ST3215::sync_read()] Failed to receive params_rx from servo %d\n", dev_id);
        }
        params_rx_arr[dev_id] = params_rx;
    }
    
    return !params_rx_arr.empty();
}