#include <Arduino.h>
#include <FastLED.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "ST3215.h"

// the uart used to control servos.
// GPIO 18 - S_RXD, GPIO 19 - S_TXD, as default.
// Serial1.begin(1000000, SERIAL_8N1, S_RXD, S_TXD);
#define SERVO_RXD_PIN 18
#define SERVO_TXD_PIN 19

// SSD1306显示器配置/引脚定义
#define SSD1306_SDA_PIN         21      // I2C数据引脚
#define SSD1306_SCL_PIN         22      // I2C时钟引脚
#define SSD1306_SCREEN_WIDTH    128     // OLED显示宽度
#define SSD1306_SCREEN_HEIGHT   32      // OLED显示高度
#define SSD1306_OLED_RESET      -1      // 重置引脚 (-1表示不使用)
#define SSD1306_SCREEN_ADDRESS  0x3C    // I2C地址

// WS2812B LED引脚定义
#define WS2812B_LED_PIN         23      // WS2812B数据引脚
#define WS2812B_NUM_LEDS        2       // LED数量


// 函数声明
void updateBreathingEffect();
void updateLEDs();
void updateDisplay();
void testServo();
void servoDemo();

// LED数组和显示器对象
CRGB leds[WS2812B_NUM_LEDS];
Adafruit_SSD1306 display(SSD1306_SCREEN_WIDTH, SSD1306_SCREEN_HEIGHT, &Wire, SSD1306_OLED_RESET);

// 舵机驱动对象
ST3215 servo(Serial1, 1000000);

// 全局变量用于显示舵机状态
std::vector<uint8_t> activeServoIds = {3, 4};
std::map<uint8_t, PositionData> lastServoPositions;

// 呼吸效果变量
float brightness1 = 0;      // LED1亮度（红色）
float brightness2 = 0;      // LED2亮度（绿色）
float breathDirection1 = 1; // LED1呼吸方向
float breathDirection2 = 1; // LED2呼吸方向
const float breathSpeed = 0.02; // 呼吸速度
unsigned long lastUpdate = 0;
const unsigned long updateInterval = 50; // 更新间隔50ms

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 WS2812B + SSD1306 + ST3215 Servo 启动...");
  
  // 初始化I2C
  Wire.begin(SSD1306_SDA_PIN, SSD1306_SCL_PIN);
  
  // 初始化SSD1306显示器
  if(!display.begin(SSD1306_SWITCHCAPVCC, SSD1306_SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 初始化失败!"));
    for(;;); // 死循环
  }
  
  // 清空显示缓冲区
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println(F("ESP32 Multi-Device"));
  display.display();
  
  // 初始化FastLED
  FastLED.addLeds<WS2812B, WS2812B_LED_PIN, GRB>(leds, WS2812B_NUM_LEDS);
  FastLED.setBrightness(123);
  
  // 初始化舵机驱动
  servo.begin();
  servo.enableDebug(true);
  Serial.println("ST3215舵机驱动初始化完成");
  
  // 测试舵机连接
  testServo();
  
  Serial.println("所有设备初始化完成!");
}

void loop() {
  unsigned long currentTime = millis();
  
  // 检查是否需要更新
  if (currentTime - lastUpdate >= updateInterval) {
    lastUpdate = currentTime;
    
    // 更新呼吸效果
    updateBreathingEffect();
    
    // 更新LED显示
    updateLEDs();
    
    // 更新OLED显示
    updateDisplay();
  }
  
  // 每5秒执行一次舵机演示
  static unsigned long lastServoDemo = 0;
  if (currentTime - lastServoDemo >= 5000) {
    lastServoDemo = currentTime;
    servoDemo();
  }
}

void updateBreathingEffect() {
  // LED1（红色）呼吸效果
  brightness1 += breathDirection1 * breathSpeed;
  if (brightness1 >= 1.0) {
    brightness1 = 1.0;
    breathDirection1 = -1;
  } else if (brightness1 <= 0.0) {
    brightness1 = 0.0;
    breathDirection1 = 1;
  }
  
  // LED2（绿色）呼吸效果（相位相反）
  brightness2 += breathDirection2 * breathSpeed;
  if (brightness2 >= 1.0) {
    brightness2 = 1.0;
    breathDirection2 = -1;
  } else if (brightness2 <= 0.0) {
    brightness2 = 0.0;
    breathDirection2 = 1;
  }
}

void updateLEDs() {
  // 计算实际亮度值（0-255）
  uint8_t red_brightness = (uint8_t)(brightness1 * 123);
  uint8_t green_brightness = (uint8_t)(brightness2 * 123);
  
  // 设置LED颜色
  leds[0] = CRGB(red_brightness, 0, 0);    // 第一个LED：红色
  leds[1] = CRGB(0, green_brightness, 0);  // 第二个LED：绿色
  
  // 显示LED
  FastLED.show();
}

void updateDisplay() {
  // 清空显示缓冲区
  display.clearDisplay();
  
  // 标题 (适配32像素高度)
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Multi-Device Status"));
  
  // LED状态（压缩显示）
  display.setCursor(0, 10);
  display.print(F("LED R:"));
  display.print((int)(brightness1 * 100));
  display.print(F("% G:"));
  display.print((int)(brightness2 * 100));
  display.println(F("%"));
  
  // 舵机状态（显示连接的舵机数量和位置信息）
  display.setCursor(0, 20);
  display.print(F("Servo: "));
  display.print(lastServoPositions.size());
  display.print(F("/2 active"));
  
  // 如果有舵机位置信息，显示第一个舵机的位置
  if (!lastServoPositions.empty()) {
    auto firstServo = lastServoPositions.begin();
    display.setCursor(90, 20);
    display.print(F("S"));
    display.print(firstServo->first);
    display.print(F(":"));
    display.print(firstServo->second.position);
  }
  
  // 更新显示
  display.display();
}

// 测试舵机连接
void testServo() {
  Serial.println("测试舵机3和舵机4连接...");
  
  // 只测试舵机3, 4
  std::vector<uint8_t> servoIds = {3, 4};
  
  for (uint8_t id : servoIds) {
    Serial.printf("测试舵机ID:%d...\n", id);
    
    // 测试ping指令
    if (servo.ping(id)) {
      Serial.printf("舵机ID:%d 连接成功\n", id);
      
      // 设置为位置模式
      if (servo.setMode(id, MODE_POSITION)) {
        Serial.printf("舵机%d已设置为位置模式\n", id);
      }
      
      // 启用力矩
      if (servo.setTorqueMode(id, TORQUE_ENABLE)) {
        Serial.printf("舵机%d力矩已启用\n", id);
      }
    } else {
      Serial.printf("舵机ID:%d 连接失败，请检查接线和ID设置\n", id);
    }
    delay(100); // 短暂延迟
  }
}

// 舵机演示
void servoDemo() {
  Serial.println("执行多舵机同步演示...");
  
  // 定义几个演示位置（0-4095范围）
  static uint16_t demoPositions[] = {512, 1024, 1536, 2048, 2560, 3072, 3584, 4095};
  static int currentPosIndex = 0;
  
  // 舵机ID列表（只有舵机3和4）
  std::vector<uint8_t> servoIds = {3, 4};
  
  // 为每个舵机生成不同的目标位置
  std::vector<uint16_t> targetPositions;
  for (size_t i = 0; i < servoIds.size(); i++) {
    int posIndex = (currentPosIndex + i) % (sizeof(demoPositions) / sizeof(demoPositions[0]));
    targetPositions.push_back(demoPositions[posIndex]);
  }
  
  // 同步移动多个舵机
  if (servo.moveToPosition(servoIds, targetPositions, 1000, 50)) {
    Serial.println("多舵机同步移动指令发送成功:");
    for (size_t i = 0; i < servoIds.size(); i++) {
      Serial.printf("  舵机%d -> 位置%d\n", servoIds[i], targetPositions[i]);
    }
    
    // 延迟一下让舵机移动
    delay(500);
    
    // 读取所有舵机的当前位置
    std::map<uint8_t, PositionData> results;
    if (servo.readPosition(servoIds, results)) {
      Serial.println("当前舵机状态:");
      for (const auto& pair : results) {
        uint8_t id = pair.first;
        const PositionData& data = pair.second;
        Serial.printf("  舵机%d: 位置=%d, 速度=%d, 负载=%d\n", 
                     id, data.position, data.velocity, data.load);
      }
      // 更新全局变量用于显示
      lastServoPositions = results;
    }
  } else {
    Serial.println("多舵机移动指令发送失败");
  }
  
  // 切换到下一个位置组合
  currentPosIndex = (currentPosIndex + 1) % (sizeof(demoPositions) / sizeof(demoPositions[0]));
}