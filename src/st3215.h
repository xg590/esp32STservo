#ifndef ST3215_H
#define ST3215_H

#include "core.h"

// 力矩模式枚举
enum TorqueMode {
    TORQUE_FREE   = 0,  // 自由模式
    TORQUE_ENABLE = 1,  // 使能模式
    TORQUE_DAMPED = 2   // 阻尼模式
};

// 舵机状态结构
struct ServoStatus {
    uint16_t posi;  // 位置
    uint16_t velo;  // 速度
    uint16_t load;  // 负载
    uint8_t  volt;  // 电压
    uint8_t  temp;  // 温度
    uint8_t  asyn;  // 异步状态
    uint8_t  stat;  // 状态
    bool     mvng;  // 运动状态
    uint16_t curr;  // 电流
};

// ST3215类继承STServo基类
class ST3215 : public STServo {
public:
    // 构造函数
    ST3215(HardwareSerial& serial, uint32_t baudrate = 1000000, bool debugEnabled = false);
    
    // 析构函数
    virtual ~ST3215();
    
    // 扩展功能方法
    bool setTorqueMode(uint8_t dev_id, TorqueMode mode);
    
    // 加速度控制
    bool setAcceleration(const std::vector<uint8_t>& dev_id_vec, const std::vector<uint8_t>& acc_vec);
    bool getAcceleration(const std::vector<uint8_t>& dev_id_vec, std::vector<uint8_t>& acc_vec);
    
    // 位置和速度控制
    bool setPosition(const std::vector<uint8_t>& dev_id_vec, const std::vector<uint16_t>& posi_vec, 
                     const std::vector<uint16_t>& velo_vec);
    
    // 单个舵机位置读取
    bool getPosition(uint8_t dev_id, uint16_t& posi);
    // 多个舵机位置和速度读取
    bool getPosition(const std::vector<uint8_t>& dev_id_vec, std::vector<uint16_t>& posi_vec, 
                     std::vector<uint16_t>& velo_vec);
    
    // 状态读取
    bool getStatus(uint8_t dev_id, ServoStatus& status);
    
    bool changeId(uint8_t old_dev_id, uint8_t new_dev_id);
    
    bool setPositionCorrection(uint8_t dev_id, int16_t correction, bool save = true);
    bool getPositionCorrection(uint8_t dev_id, int16_t& correction);
    
    void enableDebug(bool enable);
};

#endif // ST3215_H
