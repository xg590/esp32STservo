#ifndef ST3215_H
#define ST3215_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include <vector>
#include <map>

// 舵机协议指令定义
#define INST_PING       0x01
#define INST_READ       0x02
#define INST_WRITE      0x03
#define INST_REG_WRITE  0x04
#define INST_ACTION     0x05
#define INST_SYNC_READ  0x82
#define INST_SYNC_WRITE 0x83

// 舵机型号枚举
enum ServoModel {
    STS_MODEL = 0,
    SCS_MODEL = 1
};

// 舵机工作模式枚举
enum ServoMode {
    MODE_POSITION = 0,  // 位置模式
    MODE_WHEEL    = 1,  // 轮式模式
    MODE_PWM      = 2,  // PWM模式
    MODE_STEP     = 3   // 步进模式
};

// 力矩模式枚举
enum TorqueMode {
    TORQUE_FREE   = 0,  // 自由模式
    TORQUE_ENABLE = 1,  // 使能模式
    TORQUE_DAMPED = 2   // 阻尼模式
};

// 波特率映射结构
struct BaudRateMap {
    const char* name;
    uint8_t value;
};

// 舵机状态结构
struct ServoStatus {
    uint16_t position;
    uint16_t speed;
    uint16_t load;
    uint8_t voltage;
    uint8_t temperature;
    uint16_t current;
    bool moving;
};

// 位置读取结果结构
struct PositionData {
    uint16_t position;
    uint16_t velocity;
    uint16_t load;
};

// 编码器解包装器类（处理跨零点问题）
class EncoderUnwrapper {
private:
    int32_t lastRaw;
    int32_t position;
    uint16_t totalSteps;
    bool initialized;

public:
    EncoderUnwrapper(int32_t currentPosition = 0, uint16_t maxPosition = 4095);
    void update(uint16_t rawValue);
    int32_t getPosition() const;
    float getDegrees() const;
    void reset(int32_t position = 0);
};

class ST3215 {
private:
    // 串口通信对象
    HardwareSerial* _serial;
    
    // 当前舵机型号
    ServoModel _model;
    
    // STS舵机内存地址映射
    struct STSMemoryMap {
        // EPROM (只读)
        static const uint8_t SMS_STS_MODEL = 0x03;
        
        // EPROM (读写)
        static const uint8_t ID = 0x05;
        static const uint8_t BAUD_RATE = 0x06;
        static const uint8_t STEP_CORR = 0x1F;
        static const uint8_t MODE = 0x21;
        
        // SRAM (读写)
        static const uint8_t TORQUE_ENABLE = 0x28;
        static const uint8_t ACC = 0x29;
        static const uint8_t GOAL_POSITION = 0x2A;
        static const uint8_t GOAL_TIME = 0x2C;
        static const uint8_t GOAL_SPEED = 0x2E;
        static const uint8_t EPROM_LOCK = 0x37;
        
        // SRAM (只读)
        static const uint8_t PRESENT_POSITION = 0x38;
        static const uint8_t PRESENT_SPEED = 0x3A;
        static const uint8_t PRESENT_LOAD = 0x3C;
        static const uint8_t PRESENT_VOLTAGE = 0x3E;
        static const uint8_t PRESENT_TEMPERATURE = 0x3F;
        static const uint8_t MOVING = 0x42;
        static const uint8_t PRESENT_CURRENT = 0x45;
    };
    
    // SCS舵机内存地址映射
    struct SCSMemoryMap {
        static const uint8_t EPROM_LOCK = 0x30;
    };
    
    // 当前使用的内存地址映射
    uint8_t MEM_ADDR_ID;
    uint8_t MEM_ADDR_BAUD_RATE;
    uint8_t MEM_ADDR_STEP_CORR;
    uint8_t MEM_ADDR_MODE;
    uint8_t MEM_ADDR_TORQUE_ENABLE;
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
    uint8_t MEM_ADDR_MOVING;
    uint8_t MEM_ADDR_PRESENT_CURRENT;
    
    // 私有方法声明
    std::vector<uint8_t> makePacket(uint8_t id, uint8_t instruction, const std::vector<uint8_t>& params);
    bool receivePacket(uint8_t id, std::vector<uint8_t>& params, bool paramsInBytes = false);
    uint8_t calculateChecksum(const std::vector<uint8_t>& data);
    void updateMemoryMap();
    std::vector<uint8_t> intToByteArray(int value);
    int byteArrayToInt(const std::vector<uint8_t>& bytes);
    
public:
    // 构造函数
    ST3215(HardwareSerial& serial, uint32_t baudrate = 1000000);
    
    // 析构函数
    ~ST3215();
    
    // 初始化方法
    bool begin();
    void end();
    
    // 模型设置和获取
    bool setModel(ServoModel model);
    ServoModel getModel() const;
    
    // 基础通信方法
    bool ping(uint8_t id);
    bool read(uint8_t id, uint8_t address, uint8_t length, std::vector<uint8_t>& data);
    bool write(uint8_t id, uint8_t address, const std::vector<uint8_t>& data);
    bool write(uint8_t id, uint8_t address, int value);
    bool regWrite(uint8_t id, uint8_t address, const std::vector<uint8_t>& data);
    bool action();
    
    // 同步操作方法
    bool syncWrite(const std::vector<uint8_t>& ids, uint8_t address, 
                   const std::vector<std::vector<uint8_t>>& dataArray);
    bool syncRead(const std::vector<uint8_t>& ids, uint8_t address, uint8_t length,
                  std::map<uint8_t, std::vector<uint8_t>>& results);
    
    // 高级控制方法
    bool setMode(uint8_t id, ServoMode mode);
    ServoMode getMode(uint8_t id);
    bool setTorqueMode(uint8_t id, TorqueMode mode);
    bool moveToPosition(uint8_t id, uint16_t position, uint16_t velocity = 800, uint8_t acceleration = 100);
    bool moveToPosition(const std::vector<uint8_t>& ids, const std::vector<uint16_t>& positions, 
                        uint16_t velocity = 800, uint8_t acceleration = 100);
    
    // Python接口兼容方法
    bool move2Posi(uint8_t id, uint16_t position, uint16_t velocity = 800, uint8_t acceleration = 100);
    bool move2Posi(const std::vector<uint8_t>& ids, const std::vector<uint16_t>& positions, 
                   uint16_t velocity = 800, uint8_t acceleration = 100);
    std::map<uint8_t, PositionData> readPosi(const std::vector<uint8_t>& ids);
    std::map<uint8_t, PositionData> readPosi(uint8_t id);
    
    // 状态读取方法
    bool readPosition(uint8_t id, PositionData& data);
    bool readPosition(const std::vector<uint8_t>& ids, std::map<uint8_t, PositionData>& results);
    bool readServoStatus(uint8_t id, ServoStatus& status);
    
    // 配置方法
    bool changeId(uint8_t oldId, uint8_t newId);
    bool setPositionCorrection(uint8_t id, int16_t correction, bool save = false);
    bool getPositionCorrection(uint8_t id, int16_t& correction);
    
    // 调试方法
    void enableDebug(bool enable);
    void printPacket(const std::vector<uint8_t>& packet);
    
    // 静态常量方法（避免链接错误）
    static const BaudRateMap* getBaudRates();
    static int getBaudRateCount();
    
private:
    bool _debugEnabled;
    uint32_t _timeout;
};

#endif // ST3215_H
