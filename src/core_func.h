
#ifndef ST3215_H
#define ST3215_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include <vector>
#include <map>

// 自定义异常类
class SerialTimeoutException : public std::runtime_error {
    public:
        SerialTimeoutException(const std::string& message) : std::runtime_error(message) {}
};

// 舵机协议指令定义（移至类内部作为静态常量）
// 舵机模型定义（移至类内部作为静态常量）

// STS系列内存地址映射
namespace STSMemoryMap {
    // EPROM (只读)
    const uint8_t SMS_STS_MODEL       = 0x03;
    // EPROM (读写)
    const uint8_t ID                  = 0x05;
    const uint8_t BAUD_RATE           = 0x06;
    const uint8_t STEP_CORR           = 0x1F;
    const uint8_t MODE                = 0x21;
    // SRAM (读写)
    const uint8_t TORQUE_SWITCH       = 0x28;
    const uint8_t ACC                 = 0x29;
    const uint8_t GOAL_POSITION       = 0x2A;
    const uint8_t GOAL_TIME           = 0x2C;
    const uint8_t GOAL_SPEED          = 0x2E;
    const uint8_t EPROM_LOCK          = 0x37;
    // SRAM (只读)
    const uint8_t PRESENT_POSITION    = 0x38;
    const uint8_t PRESENT_SPEED       = 0x3A;
    const uint8_t PRESENT_LOAD        = 0x3C;
    const uint8_t PRESENT_VOLTAGE     = 0x3E;
    const uint8_t PRESENT_TEMPERATURE = 0x3F;
    const uint8_t SERVO_STATUS        = 0x41;
    const uint8_t ASYNC_ACTION        = 0x42;
    const uint8_t MOVING              = 0x42;
    const uint8_t PRESENT_CURRENT     = 0x45;
}

// SCS系列内存地址映射
namespace SCSMemoryMap {
    const uint8_t EPROM_LOCK          = 0x37;
}

class ST3215 {
    public:
        // 舵机协议指令定义
        static const uint8_t INST_PING       = 0x01;
        static const uint8_t INST_READ       = 0x02;
        static const uint8_t INST_WRITE      = 0x03;
        static const uint8_t INST_REG_WRITE  = 0x04;
        static const uint8_t INST_ACTION     = 0x05;
        static const uint8_t INST_SYNC_READ  = 0x82;
        static const uint8_t INST_SYNC_WRITE = 0x83;
        
        // 舵机模型定义
        static const uint8_t STS_MODEL       = 1;
        static const uint8_t SCS_MODEL       = 2;
        
        // STS系列内存地址映射（动态成员变量）
        uint8_t MEM_ADDR_SMS_STS_MODEL;
        uint8_t MEM_ADDR_ID;
        uint8_t MEM_ADDR_BAUD_RATE;
        uint8_t MEM_ADDR_STEP_CORR;
        uint8_t MEM_ADDR_MODE;
        uint8_t MEM_ADDR_TORQUE_SWITCH;
        uint8_t MEM_ADDR_ACC;
        uint8_t MEM_ADDR_GOAL_POSITION;
        uint8_t MEM_ADDR_GOAL_TIME;
        uint8_t MEM_ADDR_GOAL_SPEED;
        uint8_t MEM_ADDR_EPROM_LOCK;
        uint8_t MEM_ADDR_PRESENT_POSITION;
        uint8_t MEM_ADDR_PRESENT_SPEED;
        uint8_t MEM_ADDR_PRESENT_LOAD;
        uint8_t MEM_ADDR_PRESENT_VOLTAGE;
        uint8_t MEM_ADDR_PRESENT_TEMPERATURE;
        uint8_t MEM_ADDR_SERVO_STATUS;
        uint8_t MEM_ADDR_ASYNC_ACTION;
        uint8_t MEM_ADDR_MOVING;
        uint8_t MEM_ADDR_PRESENT_CURRENT;
        
    private:
        HardwareSerial* _serial;
        uint8_t _model;
        bool _debugEnabled;
        uint32_t _timeout;

        // 私有辅助方法
        uint8_t               calculate_checksum(const std::vector<uint8_t>& data);
        std::vector<uint8_t>  make_a_packet(uint8_t dev_id, uint8_t instruction, const std::vector<uint8_t>& params_tx);
        uint8_t               serial_read_a_byte(const char* errMsg);
        bool                  receive_packet(uint8_t dev_id, uint8_t& error, std::vector<uint8_t>& params_rx);
        void                  update_memory_map();

    public:
        // 构造函数和析构函数
        ST3215(HardwareSerial& serial, uint32_t baudrate = 1000000, bool debugEnabled = false);
        ~ST3215();
        
        // 初始化和清理方法
        bool begin();
        void end();
        
        // 核心通信方法
        bool ping(uint8_t dev_id, uint8_t& error, std::vector<uint8_t>& params_rx);
        bool read(uint8_t dev_id, uint8_t mem_addr, uint8_t length, uint8_t& error, std::vector<uint8_t>& params_rx);
        bool write_data(uint8_t dev_id, uint8_t mem_addr, const std::vector<uint8_t>& data, uint8_t& error, std::vector<uint8_t>& params_rx);
        bool write_int(uint8_t dev_id, uint8_t mem_addr, int value, uint8_t& error, std::vector<uint8_t>& params_rx);
        bool reg_write(uint8_t dev_id, uint8_t mem_addr, const std::vector<uint8_t>& data, uint8_t& error, std::vector<uint8_t>& params_rx);
        bool action();
        bool sync_write(const std::vector<uint8_t>& dev_id_arr, uint8_t mem_addr, 
                       const std::vector<std::vector<uint8_t>>& dataArray);
        bool sync_read(const std::vector<uint8_t>& dev_id_arr, uint8_t mem_addr, uint8_t length,
                      std::map<uint8_t, std::vector<uint8_t>>& params_rx_arr);
        
        // 调试和工具方法
        void printPacket(const std::vector<uint8_t>& packet);
        
        // 字节序转换工具方法
        void intToBytes(int value, uint8_t& lowByte, uint8_t& highByte);
        int bytesToInt(uint8_t lowByte, uint8_t highByte);
        
        // 配置方法
        void setDebug(bool enabled) { _debugEnabled = enabled; }
        void setTimeout(uint32_t timeout) { _timeout = timeout; }
        void setModel(uint8_t model) { _model = model; update_memory_map(); }
};

#endif // ST3215_H
