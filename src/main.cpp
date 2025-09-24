#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "st3215.h"
#include "board.h"
#include <vector>
#include <set>
#include "secret.h"

// WiFi 配置（通过secret.h文件配置）
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// TCP服务器配置
const int TCP_PORT = 8888;
WiFiServer server(TCP_PORT);
WiFiClient client;

// 全局对象
ST3215* servo = nullptr;
Adafruit_SSD1306 display(SSD1306_SCREEN_WIDTH, SSD1306_SCREEN_HEIGHT, &Wire, SSD1306_OLED_RESET);

// 舵机ID管理
std::set<uint8_t> servoIdList;  // 使用set自动去重和排序
std::vector<uint8_t> displayQueue;  // 显示队列
int currentDisplayIndex = 0;

// 状态变量
bool wifiConnected = false;
bool clientConnected = false;
unsigned long lastDisplayUpdate = 0;
unsigned long lastServoQuery = 0;
const unsigned long DISPLAY_UPDATE_INTERVAL = 1000;  // 1秒更新一次显示
const unsigned long SERVO_QUERY_INTERVAL = 1000;     // 1秒查询一次舵机

// 函数声明
void setupHardware();
void setupWiFi();
void handleTCPClient();
void updateDisplay();
void queryServoPosition();
void addServoToList(uint8_t servoId);
JsonDocument processCommand(const JsonDocument& request);
JsonDocument validateParameters(const JsonDocument& request);
bool isValidInteger(const JsonVariantConst& value, int minVal = INT_MIN, int maxVal = INT_MAX);
bool isValidUint8(const JsonVariantConst& value);
bool isValidUint16(const JsonVariantConst& value);

// 通用模板函数：从JSON中提取数字数组
template<typename T>
std::vector<T> extractNumberArray(const JsonVariant& jsonVar, T minVal, T maxVal, JsonDocument& response, const String& paramName = "parameter");

// 专用函数：提取设备ID数组
std::vector<uint8_t> extractDevIds(const JsonVariant& jsonVar, JsonDocument& response);

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("\n=== ESP32 ST3215 TCP Server Starting ===");
    
    // 初始化硬件
    setupHardware();
    
    // 初始化WiFi
    setupWiFi();
    
    // 启动TCP服务器
    if (wifiConnected) {
        server.begin();
        Serial.printf("TCP Server started on IP: %s, Port: %d\n", 
                     WiFi.localIP().toString().c_str(), TCP_PORT);
    }
    
    Serial.println("System initialization completed, waiting for client connections...");
}

void loop() {
    // 处理TCP客户端连接
    handleTCPClient();
    
    // 定期更新显示
    unsigned long currentTime = millis();
    if (currentTime - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
        lastDisplayUpdate = currentTime;
        updateDisplay();
    }
    
    // 定期查询舵机位置（如果有舵机在列表中）
    if (currentTime - lastServoQuery >= SERVO_QUERY_INTERVAL && !servoIdList.empty()) {
        lastServoQuery = currentTime;
        queryServoPosition();
    }
    
    delay(10); // 短暂延迟，避免CPU占用过高
}

void setupHardware() {
    // 初始化I2C
    Wire.begin(SSD1306_SDA_PIN, SSD1306_SCL_PIN);
    
    // 初始化OLED显示器
    if(!display.begin(SSD1306_SWITCHCAPVCC, SSD1306_SCREEN_ADDRESS)) {
        Serial.println("SSD1306 initialization failed!");
    } else {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("ESP32 TCP Server");
        display.println("Initializing...");
        display.display();
        Serial.println("SSD1306 initialized successfully");
    }
    
    // 初始化舵机驱动
    try {
        servo = new ST3215(Serial1, 1000000, false);  // 不启用调试以避免干扰TCP通信
        if (servo && servo->begin()) {
            Serial.println("ST3215 servo driver initialized successfully");
            servo->setTimeout(1000);  // 设置1秒超时
        } else {
            Serial.println("Failed to initialize ST3215 servo driver");
            delete servo;
            servo = nullptr;
        }
    } catch (const std::exception& e) {
        Serial.printf("Exception creating ST3215: %s\n", e.what());
        servo = nullptr;
    }
}

void setupWiFi() {
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(1000);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.println();
        Serial.printf("WiFi connected successfully!\n");
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("Subnet Mask: %s\n", WiFi.subnetMask().toString().c_str());
        Serial.printf("Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
    } else {
        Serial.println();
        Serial.println("WiFi connection failed! Please check SSID and password");
        wifiConnected = false;
    }
}

void handleTCPClient() {
    // 检查是否有新的客户端连接
    if (!client.connected()) {
        client = server.available();
        if (client) {
            clientConnected = true;
            Serial.println("New client connected");
            
            // 发送欢迎消息
            JsonDocument welcome;
            welcome["status"] = "connected";
            welcome["message"] = "ESP32 ST3215 TCP Server Ready";
            welcome["version"] = "2.0";
            welcome["ip"] = WiFi.localIP().toString();
            welcome["port"] = TCP_PORT;
            
            String welcomeStr;
            serializeJson(welcome, welcomeStr);
            client.println(welcomeStr);
        } else {
            clientConnected = false;
        }
        return;
    }
    
    // 处理客户端数据
    if (client.available()) {
        String jsonString = client.readStringUntil('\n');
        jsonString.trim();
        
        if (jsonString.length() > 0) {
            Serial.printf("Received JSON: %s\n", jsonString.c_str());
            
            // 解析JSON
            JsonDocument request;
            DeserializationError error = deserializeJson(request, jsonString);
            
            if (error) {
                // JSON解析错误
                JsonDocument errorResponse;
                errorResponse["error"] = 1;
                errorResponse["msg"] = "JSON parse error: " + String(error.c_str());
                
                String errorStr;
                serializeJson(errorResponse, errorStr);
                client.println(errorStr);
                return;
            }
            
            // 处理命令
            JsonDocument response = processCommand(request);
            
            // 发送响应
            String responseStr;
            serializeJson(response, responseStr);
            client.println(responseStr);
            
            Serial.printf("Sent response: %s\n", responseStr.c_str());
        }
    }
    
    // 检查客户端是否断开连接
    if (!client.connected()) {
        if (clientConnected) {
            Serial.println("Client disconnected");
            clientConnected = false;
        }
    }
}

void updateDisplay() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    
    // 第一行：显示IP和端口
    if (wifiConnected) {
        display.printf("IP:%s:%d", WiFi.localIP().toString().c_str(), TCP_PORT);
    } else {
        display.print("WiFi: Disconnected");
    }
    
    // 第二行：客户端状态
    display.setCursor(0, 10);
    if (clientConnected) {
        display.print("Client: Connected");
    } else {
        display.print("Client: Waiting...");
    }
    
    // 第三行：舵机信息
    display.setCursor(0, 20);
    if (!displayQueue.empty() && currentDisplayIndex < displayQueue.size()) {
        uint8_t servoId = displayQueue[currentDisplayIndex];
        uint16_t position = 0;
        
        // 尝试读取舵机位置
        if (servo && servo->getPosition(servoId, position)) {
            display.printf("Servo %d: %d", servoId, position);
        } else {
            display.printf("Servo %d: Error", servoId);
        }
    } else {
        display.print("No servo data");
    }
    
    display.display();
}

void queryServoPosition() {
    if (displayQueue.empty()) return;
    
    // 轮询到下一个舵机
    currentDisplayIndex = (currentDisplayIndex + 1) % displayQueue.size();
}

void addServoToList(uint8_t servoId) {
    size_t oldSize = servoIdList.size();
    servoIdList.insert(servoId);
    
    // 如果添加了新的舵机ID，更新显示队列
    if (servoIdList.size() > oldSize) {
        displayQueue.clear();
        for (uint8_t id : servoIdList) {
            displayQueue.push_back(id);
        }
        Serial.printf("Added servo ID %d to list (total: %d servos)\n", 
                     servoId, displayQueue.size());
    }
}

JsonDocument processCommand(const JsonDocument& request) {
    JsonDocument response;
    
    // 首先进行参数验证
    JsonDocument validationResult = validateParameters(request);
    if (!validationResult["error"].isNull()) {
        return validationResult;
    }
    
    // 检查是否包含func字段
    if (request["func"].isNull()) {
        response["error"] = 1;
        response["msg"] = "Missing 'func' field";
        return response;
    }
    
    String func = request["func"].as<String>();
    
    if (!servo) {
        response["error"] = 3;
        response["msg"] = "Servo driver not initialized";
        return response;
    }
    
    try {
        if (func == "setTorqueMode") {
            // 设置力矩模式：{"func":"setTorqueMode","dev_id":1,"mode":"free"}
            if (request["dev_id"].isNull() || request["mode"].isNull()) {
                response["error"] = 2;
                response["msg"] = "Missing required parameters: dev_id and mode";
                return response;
            }
            
            uint8_t devId  = request["dev_id"].as<uint8_t>();
            String modeStr = request["mode"].as<String>();
            
            TorqueMode mode = TORQUE_FREE;
            if (modeStr == "free") mode = TORQUE_FREE;
            else if (modeStr == "enable") mode = TORQUE_ENABLE;
            else if (modeStr == "damped") mode = TORQUE_DAMPED;
            else {
                response["error"] = 2;
                response["msg"] = "Invalid mode. Valid modes: free, enable, damped";
                return response;
            }
            
            // 添加到舵机列表
            addServoToList(devId);
            
            if (servo->setTorqueMode(devId, mode)) {
                response["error"] = 0;
            } else {
                response["error"] = 4;
                response["msg"] = "Failed to set torque mode";
            }
            
        } else if (func == "setAcceleration") {
            // 设置加速度：{"func":"setAcceleration","dev_id":[1,2],"acc":[100,150]}
            if (request["dev_id"].isNull() || request["acc"].isNull()) {
                response["error"] = 2;
                response["msg"] = "Missing required parameters: dev_id and acc";
                return response;
            }
            
            std::vector<uint8_t> devIds;
            std::vector<uint8_t> accelerations;
            
            // 处理dev_id参数
            if (request["dev_id"].is<JsonArray>()) {
                JsonArrayConst idArray = request["dev_id"].as<JsonArrayConst>();
                for (JsonVariantConst v : idArray) {
                    devIds.push_back(v.as<uint8_t>());
                    addServoToList(devIds.back());
                }
            } else {
                uint8_t devId = request["dev_id"].as<uint8_t>();
                devIds.push_back(devId);
                addServoToList(devId);
            }
            
            // 处理acc参数
            if (request["acc"].is<JsonArray>()) {
                JsonArrayConst accArray = request["acc"].as<JsonArrayConst>();
                if (accArray.size() != devIds.size()) {
                    response["error"] = 2;
                    response["msg"] = "dev_id and acc arrays size mismatch";
                    return response;
                }
                for (JsonVariantConst v : accArray) {
                    accelerations.push_back(v.as<uint8_t>());
                }
            } else {
                accelerations.push_back(request["acc"].as<uint8_t>());
            }
            
            if (servo->setAcceleration(devIds, accelerations)) {
                response["error"] = 0;
            } else {
                response["error"] = 4;
                response["msg"] = "Failed to set acceleration";
            }
            
        } else if (func == "getAcceleration") {
            // 读取加速度：{"func":"getAcceleration","dev_id":[1,2]}
            if (request["dev_id"].isNull()) {
                response["error"] = 2;
                response["msg"] = "Missing required parameter: dev_id";
                return response;
            }
            
            std::vector<uint8_t> devIds;
            std::vector<uint8_t> accelerations;
            
            // 处理dev_id参数
            if (request["dev_id"].is<JsonArray>()) {
                JsonArrayConst idArray = request["dev_id"].as<JsonArrayConst>();
                for (JsonVariantConst v : idArray) {
                    devIds.push_back(v.as<uint8_t>());
                    addServoToList(devIds.back());
                }
            } else {
                uint8_t devId = request["dev_id"].as<uint8_t>();
                devIds.push_back(devId);
                addServoToList(devId);
            }
            
            if (servo->getAcceleration(devIds, accelerations)) {
                response["error"] = 0;
                if (devIds.size() == 1) {
                    response["acc"] = accelerations[0];
                } else {
                    JsonArray accArray = response["acc"].to<JsonArray>();
                    for (uint8_t acc : accelerations) {
                        accArray.add(acc);
                    }
                }
            } else {
                response["error"] = 4;
                response["msg"] = "Failed to read acceleration";
            }
            
        } else if (func == "setPosition") {
            // 设置舵机位置
            if (request["dev_id"].isNull() || request["posi"].isNull()) {
                response["error"] = 2;
                response["msg"] = "Missing required parameters: dev_id and posi";
                return response;
            }
            
            std::vector<uint8_t> devIds;
            std::vector<uint16_t> positions;
            std::vector<uint16_t> velocities;
            
            // 处理单个或多个舵机
            if (request["dev_id"].is<JsonArray>()) {
                // 多舵机
                JsonArrayConst idArray = request["dev_id"].as<JsonArrayConst>();
                JsonArrayConst posiArray = request["posi"].as<JsonArrayConst>();
                
                if (idArray.size() != posiArray.size()) {
                    response["error"] = 2;
                    response["msg"] = "dev_id and posi arrays size mismatch";
                    return response;
                }
                
                for (size_t i = 0; i < idArray.size(); i++) {
                    devIds.push_back(idArray[i].as<uint8_t>());
                    positions.push_back(posiArray[i].as<uint16_t>());
                    velocities.push_back(request["velo"] | 800);  // 默认速度800
                    
                    // 添加到舵机列表
                    addServoToList(devIds.back());
                }
            } else {
                // 单舵机
                uint8_t devId = request["dev_id"].as<uint8_t>();
                devIds.push_back(devId);
                positions.push_back(request["posi"].as<uint16_t>());
                velocities.push_back(request["velo"] | 800);
                
                // 添加到舵机列表
                addServoToList(devId);
            }
            
            if (servo->setPosition(devIds, positions, velocities)) {
                response["error"] = 0;
            } else {
                response["error"] = 4;
                response["msg"] = "Failed to set servo position";
            }
            
        } else if (func == "getPosition") {
            // 读取单个舵机位置：{"func":"getPosition","dev_id":1}
            if (request["dev_id"].isNull()) {
                response["error"] = 2;
                response["msg"] = "Missing required parameter: dev_id";
                return response;
            }
            
            uint8_t devId = request["dev_id"].as<uint8_t>();
            uint16_t position = 0;
            
            // 添加到舵机列表
            addServoToList(devId);
            
            if (servo->getPosition(devId, position)) {
                response["error"] = 0;
                response["posi"] = position;
            } else {
                response["error"] = 4;
                response["msg"] = "Failed to read servo position";
            }
            
        } else if (func == "getStatus") {
            // 读取舵机完整状态：{"func":"getStatus","dev_id":1}
            if (request["dev_id"].isNull()) {
                response["error"] = 2;
                response["msg"] = "Missing required parameter: dev_id";
                return response;
            }
            
            uint8_t devId = request["dev_id"].as<uint8_t>();
            ServoStatus status;
            
            // 添加到舵机列表
            addServoToList(devId);
            
            if (servo->getStatus(devId, status)) {
                response["error"] = 0;
                response["posi"] = status.posi;
                response["velo"] = status.velo;
                response["load"] = status.load;
                response["volt"] = status.volt;
                response["temp"] = status.temp;
                response["asyn"] = status.asyn;
                response["stat"] = status.stat;
                response["mvng"] = status.mvng;
                response["curr"] = status.curr;
            } else {
                response["error"] = 4;
                response["msg"] = "Failed to read servo status";
            }
            
        } else if (func == "changeId") {
            // 更改舵机ID
            if (request["old_id"].isNull() || request["new_id"].isNull()) {
                response["error"] = 2;
                response["msg"] = "Missing required parameters: old_id and new_id";
                return response;
            }
            
            uint8_t oldId = request["old_id"].as<uint8_t>();
            uint8_t newId = request["new_id"].as<uint8_t>();
            
            if (servo->changeId(oldId, newId)) {
                response["error"] = 0;
                
                // 更新舵机列表
                servoIdList.erase(oldId);
                addServoToList(newId);
            } else {
                response["error"] = 4;
                response["msg"] = "Failed to change servo ID";
            }
            
        } else if (func == "setPositionCorrection") {
            // 设置位置校正：{"func":"setPositionCorrection","dev_id":1,"correction":100,"save":true}
            if (request["dev_id"].isNull() || request["correction"].isNull()) {
                response["error"] = 2;
                response["msg"] = "Missing required parameters: dev_id and correction";
                return response;
            }
            
            uint8_t devId = request["dev_id"].as<uint8_t>();
            int16_t correction = request["correction"].as<int16_t>();
            bool save = request["save"] | true;  // 默认保存到EPROM
            
            // 添加到舵机列表
            addServoToList(devId);
            
            if (servo->setPositionCorrection(devId, correction, save)) {
                response["error"] = 0;
            } else {
                response["error"] = 4;
                response["msg"] = "Failed to set position correction";
            }
            
        } else if (func == "getPositionCorrection") {
            // 读取位置校正：{"func":"getPositionCorrection","dev_id":1}
            if (request["dev_id"].isNull()) {
                response["error"] = 2;
                response["msg"] = "Missing required parameter: dev_id";
                return response;
            }
            
            uint8_t devId = request["dev_id"].as<uint8_t>();
            int16_t correction = 0;
            
            // 添加到舵机列表
            addServoToList(devId);
            
            if (servo->getPositionCorrection(devId, correction)) {
                response["error"] = 0;
                response["correction"] = correction;
            } else {
                response["error"] = 4;
                response["msg"] = "Failed to read position correction";
            }
            
        } else if (func == "ping") {
            // Ping舵机（基类功能）
            if (request["dev_id"].isNull()) {
                response["error"] = 2;
                response["msg"] = "Missing required parameter: dev_id";
                return response;
            }
            
            uint8_t devId = request["dev_id"].as<uint8_t>();
            uint8_t error = 0;
            std::vector<uint8_t> params_rx;
            
            // 添加到舵机列表
            addServoToList(devId);
            
            if (servo->ping(devId, error, params_rx)) {
                response["error"] = 0;
                response["connected"] = true;
            } else {
                response["error"] = 4;
                response["msg"] = "Servo ping failed";
                response["connected"] = false;
            }
            
        } else if (func == "read") {
            // 读取内存地址数据：{"func":"read","dev_id":1,"mem_addr":56,"length":2}
            if (request["dev_id"].isNull() || request["mem_addr"].isNull() || request["length"].isNull()) {
                response["error"] = 2;
                response["msg"] = "Missing required parameters: dev_id, mem_addr, length";
                return response;
            }
            
            uint8_t devId = request["dev_id"].as<uint8_t>();
            uint8_t memAddr = request["mem_addr"].as<uint8_t>();
            uint8_t length = request["length"].as<uint8_t>();
            uint8_t error = 0;
            std::vector<uint8_t> params_rx;
            
            // 添加到舵机列表
            addServoToList(devId);
            
            if (servo->read(devId, memAddr, length, error, params_rx)) {
                response["error"] = 0;
                response["data"] = JsonArray();
                JsonArray dataArray = response["data"];
                for (uint8_t byte : params_rx) {
                    dataArray.add(byte);
                }
                response["error_code"] = error;
            } else {
                response["error"] = 4;
                response["msg"] = "Failed to read from memory address";
                response["error_code"] = error;
            }
            
        } else if (func == "write_data") {
            // 写入数据到内存地址：{"func":"write_data","dev_id":1,"mem_addr":56,"data":[100,200]}
            if (request["dev_id"].isNull() || request["mem_addr"].isNull() || request["data"].isNull()) {
                response["error"] = 2;
                response["msg"] = "Missing required parameters: dev_id, mem_addr, data";
                return response;
            }
            
            uint8_t devId = request["dev_id"].as<uint8_t>();
            uint8_t memAddr = request["mem_addr"].as<uint8_t>();
            uint8_t error = 0;
            std::vector<uint8_t> params_rx;
            std::vector<uint8_t> data;
            
            // 处理data参数
            if (request["data"].is<JsonArray>()) {
                JsonArrayConst dataArray = request["data"].as<JsonArrayConst>();
                for (JsonVariantConst v : dataArray) {
                    data.push_back(v.as<uint8_t>());
                }
            } else {
                response["error"] = 2;
                response["msg"] = "data parameter must be an array";
                return response;
            }
            
            // 添加到舵机列表
            addServoToList(devId);
            
            if (servo->write_data(devId, memAddr, data, error, params_rx)) {
                response["error"] = 0;
                response["error_code"] = error;
            } else {
                response["error"] = 4;
                response["msg"] = "Failed to write data to memory address";
                response["error_code"] = error;
            }
            
        } else if (func == "write_int") {
            // 写入整数到内存地址：{"func":"write_int","dev_id":1,"mem_addr":56,"value":1000}
            if (request["dev_id"].isNull() || request["mem_addr"].isNull() || request["value"].isNull()) {
                response["error"] = 2;
                response["msg"] = "Missing required parameters: dev_id, mem_addr, value";
                return response;
            }
            
            uint8_t devId = request["dev_id"].as<uint8_t>();
            uint8_t memAddr = request["mem_addr"].as<uint8_t>();
            int value = request["value"].as<int>();
            uint8_t error = 0;
            std::vector<uint8_t> params_rx;
            
            // 添加到舵机列表
            addServoToList(devId);
            
            if (servo->write_int(devId, memAddr, value, error, params_rx)) {
                response["error"] = 0;
                response["error_code"] = error;
            } else {
                response["error"] = 4;
                response["msg"] = "Failed to write integer to memory address";
                response["error_code"] = error;
            }
            
        } else if (func == "reg_write") {
            // 寄存器写入：{"func":"reg_write","dev_id":1,"mem_addr":56,"data":[100,200]}
            if (request["dev_id"].isNull() || request["mem_addr"].isNull() || request["data"].isNull()) {
                response["error"] = 2;
                response["msg"] = "Missing required parameters: dev_id, mem_addr, data";
                return response;
            }
            
            uint8_t devId = request["dev_id"].as<uint8_t>();
            uint8_t memAddr = request["mem_addr"].as<uint8_t>();
            uint8_t error = 0;
            std::vector<uint8_t> params_rx;
            std::vector<uint8_t> data;
            
            // 处理data参数
            if (request["data"].is<JsonArray>()) {
                JsonArrayConst dataArray = request["data"].as<JsonArrayConst>();
                for (JsonVariantConst v : dataArray) {
                    data.push_back(v.as<uint8_t>());
                }
            } else {
                response["error"] = 2;
                response["msg"] = "data parameter must be an array";
                return response;
            }
            
            // 添加到舵机列表
            addServoToList(devId);
            
            if (servo->reg_write(devId, memAddr, data, error, params_rx)) {
                response["error"] = 0;
                response["error_code"] = error;
            } else {
                response["error"] = 4;
                response["msg"] = "Failed to register write";
                response["error_code"] = error;
            }
            
        } else if (func == "action") {
            // 执行动作：{"func":"action"}
            if (servo->action()) {
                response["error"] = 0;
            } else {
                response["error"] = 4;
                response["msg"] = "Failed to execute action";
            }
            
        } else if (func == "sync_write") {
            // 同步写入：{"func":"sync_write","dev_id":[1,2],"mem_addr":56,"data":[[100,200],[150,250]]}
            if (request["dev_id"].isNull() || request["mem_addr"].isNull() || request["data"].isNull()) {
                response["error"] = 2;
                response["msg"] = "Missing required parameters: dev_id, mem_addr, data";
                return response;
            }
            
            std::vector<uint8_t> devIds;
            uint8_t memAddr = request["mem_addr"].as<uint8_t>();
            std::vector<std::vector<uint8_t>> dataArray;
            
            // 处理dev_id参数
            if (request["dev_id"].is<JsonArray>()) {
                JsonArrayConst idArray = request["dev_id"].as<JsonArrayConst>();
                for (JsonVariantConst v : idArray) {
                    devIds.push_back(v.as<uint8_t>());
                    addServoToList(devIds.back());
                }
            } else {
                response["error"] = 2;
                response["msg"] = "dev_id parameter must be an array for sync_write";
                return response;
            }
            
            // 处理data参数（二维数组）
            if (request["data"].is<JsonArray>()) {
                JsonArrayConst outerArray = request["data"].as<JsonArrayConst>();
                if (outerArray.size() != devIds.size()) {
                    response["error"] = 2;
                    response["msg"] = "dev_id and data arrays size mismatch";
                    return response;
                }
                
                for (JsonVariantConst outerV : outerArray) {
                    if (outerV.is<JsonArray>()) {
                        std::vector<uint8_t> innerData;
                        JsonArrayConst innerArray = outerV.as<JsonArrayConst>();
                        for (JsonVariantConst innerV : innerArray) {
                            innerData.push_back(innerV.as<uint8_t>());
                        }
                        dataArray.push_back(innerData);
                    } else {
                        response["error"] = 2;
                        response["msg"] = "data parameter must be a 2D array";
                        return response;
                    }
                }
            } else {
                response["error"] = 2;
                response["msg"] = "data parameter must be a 2D array";
                return response;
            }
            
            if (servo->sync_write(devIds, memAddr, dataArray)) {
                response["error"] = 0;
            } else {
                response["error"] = 4;
                response["msg"] = "Failed to sync write";
            }
            
        } else if (func == "sync_read") {
            // 同步读取：{"func":"sync_read","dev_id":[1,2],"mem_addr":56,"length":2}
            if (request["dev_id"].isNull() || request["mem_addr"].isNull() || request["length"].isNull()) {
                response["error"] = 2;
                response["msg"] = "Missing required parameters: dev_id, mem_addr, length";
                return response;
            }
            
            std::vector<uint8_t> devIds;
            uint8_t memAddr = request["mem_addr"].as<uint8_t>();
            uint8_t length = request["length"].as<uint8_t>();
            std::vector<std::vector<uint8_t>> dataArray;
            
            // 处理dev_id参数
            if (request["dev_id"].is<JsonArray>()) {
                JsonArrayConst idArray = request["dev_id"].as<JsonArrayConst>();
                for (JsonVariantConst v : idArray) {
                    devIds.push_back(v.as<uint8_t>());
                    addServoToList(devIds.back());
                }
            } else {
                response["error"] = 2;
                response["msg"] = "dev_id parameter must be an array for sync_read";
                return response;
            }
            
            if (servo->sync_read(devIds, memAddr, length, dataArray)) {
                response["error"] = 0;
                response["data"] = JsonArray();
                JsonArray outerArray = response["data"];
                
                for (const auto& innerData : dataArray) {
                    JsonArray innerArray = outerArray.add<JsonArray>();
                    for (uint8_t byte : innerData) {
                        innerArray.add(byte);
                    }
                }
            } else {
                response["error"] = 4;
                response["msg"] = "Failed to sync read";
            }
            
        } else {
            response["error"] = 1;
            response["msg"] = "Unknown function: " + func;
            response["available_functions"] = "[setTorqueMode, setAcceleration, getAcceleration, setPosition, getPosition, getStatus, changeId, setPositionCorrection, getPositionCorrection, ping, read, write_data, write_int, reg_write, action, sync_write, sync_read]";
        }
        
    } catch (const SerialTimeoutException& e) {
        response["error"] = 5;
        response["msg"] = "Serial timeout: " + String(e.what());
    } catch (const std::exception& e) {
        response["error"] = 6;
        response["msg"] = "Exception: " + String(e.what());
    } catch (...) {
        response["error"] = 7;
        response["msg"] = "Unknown error occurred";
    }
    
    return response;
}

JsonDocument validateParameters(const JsonDocument& request) {
    JsonDocument response;
    
    // 检查JSON是否为对象
    if (!request.is<JsonObject>()) {
        response["error"] = 2;
        response["msg"] = "Datatype check for parameter failed: Request must be a JSON object";
        return response;
    }
    
    // 如果包含func字段，验证其类型
    if (!request["func"].isNull()) {
        if (!request["func"].is<const char*>()) {
            response["error"] = 2;
            response["msg"] = "Datatype check for parameter failed: func must be a string";
            return response;
        }
    }
    
    // 验证dev_id参数
    if (!request["dev_id"].isNull()) {
        JsonVariantConst devId = request["dev_id"];
        if (devId.is<JsonArray>()) {
            // 数组情况
            JsonArrayConst arr = devId.as<JsonArrayConst>();
            for (JsonVariantConst v : arr) {
                if (!isValidUint8(v)) {
                    response["error"] = 2;
                    response["msg"] = "Datatype check for parameter failed: dev_id array elements must be integers 0-255";
                    return response;
                }
            }
        } else {
            // 单个值情况
            if (!isValidUint8(devId)) {
                response["error"] = 2;
                response["msg"] = "Datatype check for parameter failed: dev_id must be an integer 0-255";
                return response;
            }
        }
    }
    
    // 验证posi参数
    if (!request["posi"].isNull()) {
        JsonVariantConst posi = request["posi"];
        if (posi.is<JsonArray>()) {
            // 数组情况
            JsonArrayConst arr = posi.as<JsonArrayConst>();
            for (JsonVariantConst v : arr) {
                if (!isValidUint16(v)) {
                    response["error"] = 2;
                    response["msg"] = "Datatype check for parameter failed: posi array elements must be integers 0-65535";
                    return response;
                }
            }
        } else {
            // 单个值情况
            if (!isValidUint16(posi)) {
                response["error"] = 2;
                response["msg"] = "Datatype check for parameter failed: posi must be an integer 0-65535";
                return response;
            }
        }
    }
    
    // 验证velo参数
    if (!request["velo"].isNull()) {
        if (!isValidUint16(request["velo"])) {
            response["error"] = 2;
            response["msg"] = "Datatype check for parameter failed: velo must be an integer 0-65535";
            return response;
        }
    }
    
    // 验证old_id和new_id参数
    if (!request["old_id"].isNull()) {
        if (!isValidUint8(request["old_id"])) {
            response["error"] = 2;
            response["msg"] = "Datatype check for parameter failed: old_id must be an integer 0-255";
            return response;
        }
    }
    
    if (!request["new_id"].isNull()) {
        if (!isValidUint8(request["new_id"])) {
            response["error"] = 2;
            response["msg"] = "Datatype check for parameter failed: new_id must be an integer 0-255";
            return response;
        }
    }
    
    // 验证mode参数
    if (!request["mode"].isNull()) {
        if (!request["mode"].is<const char*>()) {
            response["error"] = 2;
            response["msg"] = "Datatype check for parameter failed: mode must be a string";
            return response;
        }
    }
    
    // 验证acc参数
    if (!request["acc"].isNull()) {
        JsonVariantConst acc = request["acc"];
        if (acc.is<JsonArray>()) {
            // 数组情况
            JsonArrayConst arr = acc.as<JsonArrayConst>();
            for (JsonVariantConst v : arr) {
                if (!isValidUint8(v)) {
                    response["error"] = 2;
                    response["msg"] = "Datatype check for parameter failed: acc array elements must be integers 0-255";
                    return response;
                }
            }
        } else {
            // 单个值情况
            if (!isValidUint8(acc)) {
                response["error"] = 2;
                response["msg"] = "Datatype check for parameter failed: acc must be an integer 0-255";
                return response;
            }
        }
    }
    
    // 验证correction参数
    if (!request["correction"].isNull()) {
        if (!isValidInteger(request["correction"], -2047, 2047)) {
            response["error"] = 2;
            response["msg"] = "Datatype check for parameter failed: correction must be an integer -2047 to 2047";
            return response;
        }
    }
    
    // 验证save参数
    if (!request["save"].isNull()) {
        if (!request["save"].is<bool>()) {
            response["error"] = 2;
            response["msg"] = "Datatype check for parameter failed: save must be a boolean";
            return response;
        }
    }
    
    // 验证mem_addr参数
    if (!request["mem_addr"].isNull()) {
        if (!isValidUint8(request["mem_addr"])) {
            response["error"] = 2;
            response["msg"] = "Datatype check for parameter failed: mem_addr must be an integer 0-255";
            return response;
        }
    }
    
    // 验证length参数
    if (!request["length"].isNull()) {
        if (!isValidUint8(request["length"])) {
            response["error"] = 2;
            response["msg"] = "Datatype check for parameter failed: length must be an integer 0-255";
            return response;
        }
    }
    
    // 验证value参数
    if (!request["value"].isNull()) {
        if (!isValidInteger(request["value"], INT_MIN, INT_MAX)) {
            response["error"] = 2;
            response["msg"] = "Datatype check for parameter failed: value must be an integer";
            return response;
        }
    }
    
    // 验证data参数
    if (!request["data"].isNull()) {
        JsonVariantConst data = request["data"];
        if (data.is<JsonArray>()) {
            // 可能是一维或二维数组
            JsonArrayConst arr = data.as<JsonArrayConst>();
            for (JsonVariantConst v : arr) {
                if (v.is<JsonArray>()) {
                    // 二维数组情况
                    JsonArrayConst innerArr = v.as<JsonArrayConst>();
                    for (JsonVariantConst innerV : innerArr) {
                        if (!isValidUint8(innerV)) {
                            response["error"] = 2;
                            response["msg"] = "Datatype check for parameter failed: data array elements must be integers 0-255";
                            return response;
                        }
                    }
                } else {
                    // 一维数组情况
                    if (!isValidUint8(v)) {
                        response["error"] = 2;
                        response["msg"] = "Datatype check for parameter failed: data array elements must be integers 0-255";
                        return response;
                    }
                }
            }
        } else {
            response["error"] = 2;
            response["msg"] = "Datatype check for parameter failed: data must be an array";
            return response;
        }
    }
    
    // 如果所有验证都通过，返回空的响应
    return response;
}

bool isValidInteger(const JsonVariantConst& value, int minVal, int maxVal) {
    if (!value.is<int>() && !value.is<unsigned int>()) {
        return false;
    }
    
    int intVal = value.as<int>();
    return (intVal >= minVal && intVal <= maxVal);
}

bool isValidUint8(const JsonVariantConst& value) {
    return isValidInteger(value, 0, 255);
}

bool isValidUint16(const JsonVariantConst& value) {
    return isValidInteger(value, 0, 65535);
}

// 通用模板函数实现：从JSON中提取数字数组
template<typename T>
std::vector<T> extractNumberArray(const JsonVariant& jsonVar, T minVal, T maxVal, JsonDocument& response, const String& paramName) {
    std::vector<T> result;
    
    if (jsonVar.is<JsonArray>()) {
        // 数组情况：[1,2,3] 或 ["1","2","3"]
        JsonArrayConst arr = jsonVar.as<JsonArrayConst>();
        result.reserve(arr.size()); // 预分配内存提高性能
        
        int index = 0;
        for (JsonVariantConst v : arr) {
            // 支持数字和字符串两种格式
            long value = -1;
            if (v.is<int>()) {
                value = v.as<int>();
            } else if (v.is<long>()) {
                value = v.as<long>();
            } else if (v.is<const char*>()) {
                String str = v.as<String>();
                value = str.toInt();
                // 验证字符串是否为有效数字
                if (value == 0 && str != "0") {
                    response["error"] = 2;
                    response["msg"] = paramName + " array element " + String(index) + " is not a valid number: " + str;
                    return {};
                }
            } else {
                response["error"] = 2;
                response["msg"] = paramName + " array element " + String(index) + " must be a number or string";
                return {};
            }
            
            // 验证范围
            if (value < static_cast<long>(minVal) || value > static_cast<long>(maxVal)) {
                response["error"] = 2;
                response["msg"] = paramName + " array element " + String(index) + " must be " + 
                                String(static_cast<long>(minVal)) + "-" + String(static_cast<long>(maxVal)) + ", got " + String(value);
                return {};
            }
            
            result.push_back(static_cast<T>(value));
            index++;
        }
    } else if (jsonVar.is<int>() || jsonVar.is<long>() || jsonVar.is<const char*>()) {
        // 单个数字或字符串情况：1 或 "1"
        long value = -1;
        if (jsonVar.is<int>()) {
            value = jsonVar.as<int>();
        } else if (jsonVar.is<long>()) {
            value = jsonVar.as<long>();
        } else {
            String str = jsonVar.as<String>();
            value = str.toInt();
            // 验证字符串是否为有效数字
            if (value == 0 && str != "0") {
                response["error"] = 2;
                response["msg"] = paramName + " is not a valid number: " + str;
                return {};
            }
        }
        
        // 验证范围
        if (value < static_cast<long>(minVal) || value > static_cast<long>(maxVal)) {
            response["error"] = 2;
            response["msg"] = paramName + " must be " + String(static_cast<long>(minVal)) + "-" + 
                            String(static_cast<long>(maxVal)) + ", got " + String(value);
            return {};
        }
        
        result.push_back(static_cast<T>(value));
    } else {
        response["error"] = 2;
        response["msg"] = paramName + " must be a number, string, or array of numbers/strings";
        return {};
    }
    
    // 验证不能为空
    if (result.empty()) {
        response["error"] = 2;
        response["msg"] = paramName + " array cannot be empty";
        return {};
    }
    
    return result;
}

// 专用函数：提取设备ID数组（保持向后兼容）
std::vector<uint8_t> extractDevIds(const JsonVariant& jsonVar, JsonDocument& response) {
    std::vector<uint8_t> devIds = extractNumberArray<uint8_t>(jsonVar, 0, 255, response, "dev_id");
    
    // 成功提取后，自动添加到舵机列表
    if (!devIds.empty() && response["error"].isNull()) {
        for (uint8_t id : devIds) {
            addServoToList(id);
        }
    }
    
    return devIds;
}
