#ifndef TEST_CORE_FUNCTION_H
#define TEST_CORE_FUNCTION_H

#include <Arduino.h>
#include "st3215.h"

// 测试用的舵机ID
extern const uint8_t TEST_SERVO_ID_1;
extern const uint8_t TEST_SERVO_ID_2;

// 全局STServo对象指针
extern ST3215* servo;

// 主要测试函数声明
void runAllTests();

// 各个测试函数声明
void testPingFunction();
void testReadFunction();
void testWriteFunctions();
void testRegWriteFunction();
void testActionFunction();
void testSyncReadFunction();
void testSyncWriteFunction();

#endif // TEST_CORE_FUNCTION_H
