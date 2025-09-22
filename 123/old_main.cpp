#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "ST3215.h"
#include "secret.h"

// WiFi 配置
const char* ssid     = WIFI_SSID;      // see include/secret.h
const char* password = WIFI_PASSWORD; 

// TCP服务器配置
WiFiServer server(8888);
WiFiClient client;

// 全局对象
ST3215 servo(Serial1, 1000000);
Adafruit_SSD1306 display(SSD1306_SCREEN_WIDTH, SSD1306_SCREEN_HEIGHT, &Wire, SSD1306_OLED_RESET);
CRGB leds[WS2812B_NUM_LEDS];

// 状态变量
bool                wifiConnected         = false;
bool                clientConnected       = false;
unsigned long       lastDisplayUpdate     = 0;
const unsigned long displayUpdateInterval = 1000; // 1秒更新一次显示

// 函数声明
void setupHardware();
void setupWiFi();
void handleTCPClient();
void updateDisplay();
JsonDocument processCommand(const JsonDocument& request);
JsonDocument callServoFunction(const String& func, const JsonDocument& params);

void setup() {
    Serial.begin(115200);
    Serial.println("ESP32 ST3215 TCP Server启动...");
    
    // 初始化硬件
    setupHardware();
    
    // 初始化WiFi
    setupWiFi();
    
    // 启动TCP服务器
    server.begin();
    Serial.printf("TCP服务器已启动\n");
    Serial.printf("服务器IP地址: %s，端口: 8888\n", WiFi.localIP().toString().c_str());
    Serial.printf("系统初始化完成，等待客户端连接...\n");
}

void loop() {
    // 处理TCP客户端连接
    handleTCPClient();
    
    // 定期更新显示
    unsigned long currentTime = millis();
    if (currentTime - lastDisplayUpdate >= displayUpdateInterval) {
        lastDisplayUpdate = currentTime;
        updateDisplay();
    }
    
    delay(10); // 短暂延迟，避免CPU占用过高
}

void setupWiFi() {
    WiFi.begin(ssid, password);
    Serial.print("连接到WiFi");
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(1000);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.println();
        Serial.printf("WiFi连接成功!\n");
        Serial.printf("IP地址: %s\n"  , WiFi.localIP().toString().c_str());
        Serial.printf("子网掩码: %s\n", WiFi.subnetMask().toString().c_str());
        Serial.printf("网关: %s\n"    , WiFi.gatewayIP().toString().c_str());
    } else {
        Serial.println();
        Serial.println("WiFi连接失败! 请检查SSID和密码");
        wifiConnected = false;
    }
}

void setupHardware() {
    // 初始化I2C
    Wire.begin(SSD1306_SDA_PIN, SSD1306_SCL_PIN);
    
    // 初始化OLED显示器
    if(!display.begin(SSD1306_SWITCHCAPVCC, SSD1306_SCREEN_ADDRESS)) {
        Serial.println("SSD1306初始化失败!");
    } else {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0,0);
        display.println("ESP32 TCP Server");
        display.println("Initializing...");
        display.display();
    }
    
    // 初始化LED
    FastLED.addLeds<WS2812B, WS2812B_LED_PIN, GRB>(leds, WS2812B_NUM_LEDS);
    FastLED.setBrightness(50);
    
    // 设置LED状态指示灯
    leds[0] = CRGB::Red;    // 红色表示未连接
    leds[1] = CRGB::Black;  // 关闭
    FastLED.show();
    
    // 初始化舵机驱动
    servo.begin();
    servo.enableDebug(false); // 关闭调试信息，避免干扰TCP通信
    Serial.println("舵机驱动初始化完成");
}

void updateDisplay() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    
    // 标题
    display.println("ST3215 TCP Server");
    
    // WiFi状态
    display.setCursor(0, 10);
    if (wifiConnected) {
        display.print("WiFi: ");
        display.println(WiFi.localIP().toString());
    } else {
        display.println("WiFi: Disconnected");
    }
    
    // 客户端状态
    display.setCursor(0, 20);
    if (clientConnected) {
        display.println("Client: Connected");
    } else {
        display.println("Client: Waiting...");
    }
    
    display.display();
    
    // 更新LED状态
    if (wifiConnected) {
        leds[0] = CRGB::Green;  // 绿色表示WiFi已连接
    } else {
        leds[0] = CRGB::Red;    // 红色表示WiFi未连接
    }
    
    if (clientConnected) {
        leds[1] = CRGB::Blue;   // 蓝色表示客户端已连接
    } else {
        leds[1] = CRGB::Black;  // 关闭
    }
    
    FastLED.show();
}

void handleTCPClient() {
    // 检查是否有新的客户端连接
    if (!client.connected()) {
        client = server.available();
        if (client) {
            clientConnected = true;
            Serial.println("新客户端连接");
            
            // 发送欢迎消息
            JsonDocument welcome;
            welcome["status"] = "connected";
            welcome["message"] = "ESP32 ST3215 TCP Server Ready";
            welcome["version"] = "1.0";
            
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
            Serial.printf("收到JSON: %s\n", jsonString.c_str());
            
            // 解析JSON
            JsonDocument request;
            DeserializationError error = deserializeJson(request, jsonString);
            
            if (error) {
                // JSON解析错误
                JsonDocument errorResponse;
                errorResponse["ok"] = false;
                errorResponse["error"] = "JSON parse error";
                errorResponse["message"] = error.c_str();
                
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
            
            Serial.printf("发送响应: %s\n", responseStr.c_str());
        }
    }
    
    // 检查客户端是否断开连接
    if (!client.connected()) {
        if (clientConnected) {
            Serial.println("客户端断开连接");
            clientConnected = false;
        }
    }
}

JsonDocument processCommand(const JsonDocument& request) {
    JsonDocument response;
    
    // 检查是否包含func字段
    if (!request.containsKey("func")) {
        response["ok"] = false;
        response["error"] = "Missing 'func' field";
        return response;
    }
    
    String func = request["func"];
    
    try {
        // 调用相应的舵机函数
        response = callServoFunction(func, request);
    } catch (const SerialTimeoutException& e) {
        // 专门处理串口超时异常
        response["ok"] = false;
        response["error"] = "[Timeout] Serial Read";
        response["message"] = e.what();
    } catch (const std::exception& e) {
        response["ok"] = false;
        response["error"] = "Function call failed";
        response["message"] = e.what();
    } catch (...) {
        response["ok"] = false;
        response["error"] = "Unknown error";
    }
    
    return response;
}

JsonDocument callServoFunction(const String& func, const JsonDocument& params) {
    JsonDocument response; 
    response["ok"] = false;
    response["func"] = func;
    
    if (func == "ping") {
        uint8_t id = params["id"] | 1;
        bool result = servo.ping(id);
        response["ok"] = result;
        response["id"] = id;
        response["connected"] = result;
        
    } else if (func == "move2Posi") {
        // move2Posi(self, dev_id=1, posi=0, velo=800, acc=100):
        if (!params.containsKey("dev_id") || !params.containsKey("posi")) {
            response["error"] = "Missing required parameters: dev_id and posi";
            return response;
        }
        // 多舵机控制
        JsonArray idsArray = params["dev_id"];
        JsonArray positionsArray = params["posi"];
        
        std::vector<uint8_t> ids;
        std::vector<uint16_t> positions;
        
        for (JsonVariant v : idsArray) {
            ids.push_back(v.as<uint8_t>());
        }
        for (JsonVariant v : positionsArray) {
            positions.push_back(v.as<uint16_t>());
        }
        
        uint16_t velocity = params["velo"] | 800;
        uint8_t acceleration = params["acc"] | 100;
        
        bool result = servo.move2Posi(ids, positions, velocity, acceleration);
        response["ok"] = result;
        response["ids"] = idsArray;
        response["positions"] = positionsArray; 
                
    } else if (func == "readPosi") {
        if (!params.containsKey("dev_id")) {
            response["error"] = "Missing required parameter: dev_id";
            return response;
        }
        // 多舵机读取
        JsonArray idsArray = params["dev_id"];
        std::vector<uint8_t> ids;
        
        for (JsonVariant v : idsArray) {
            ids.push_back(v.as<uint8_t>());
        }
        
        auto results = servo.readPosi(ids);
        response["ok"] = !results.empty();
        
        JsonObject data = response["data"].to<JsonObject>();
        for (const auto& pair : results) {
            JsonObject servoData = data[String(pair.first)].to<JsonObject>();
            servoData["position"] = pair.second.position;
            servoData["velocity"] = pair.second.velocity;
            servoData["load"] = pair.second.load;
        } 
            
    } else if (func == "setMode") {
        uint8_t id = params["id"] | 1;
        String modeStr = params["mode"] | "posi";
        
        ServoMode mode = MODE_POSITION;
        if (modeStr == "wheel") mode = MODE_WHEEL;
        else if (modeStr == "pwm") mode = MODE_PWM;
        else if (modeStr == "step") mode = MODE_STEP;
        
        bool result = servo.setMode(id, mode);
        response["ok"] = result;
        response["id"] = id;
        response["mode"] = modeStr;
        
    } else if (func == "setTorqueMode") {
        uint8_t id = params["id"] | 1;
        String modeStr = params["mode"] | "free";
        
        TorqueMode mode = TORQUE_FREE;
        if (modeStr == "torque") mode = TORQUE_ENABLE;
        else if (modeStr == "damped") mode = TORQUE_DAMPED;
        
        bool result = servo.setTorqueMode(id, mode);
        response["ok"] = result;
        response["id"] = id;
        response["torque_mode"] = modeStr;
        
    } else if (func == "changeId") {
        uint8_t oldId = params["old_id"] | 1;
        uint8_t newId = params["new_id"] | 2;
        
        bool result = servo.changeId(oldId, newId);
        response["ok"] = result;
        response["old_id"] = oldId;
        response["new_id"] = newId;
        
    } else {
        response["ok"] = false;
        response["error"] = "Unknown function";
        response["available_functions"] = "[ping, move2Posi, readPosi, setMode, setTorqueMode, changeId]";
    }
    
    return response;
}
