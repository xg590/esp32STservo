#ifndef TEST_CORE_FUNCTION_H
#define TEST_CORE_FUNCTION_H

#include <Arduino.h>
#include "core_func.h"

// 测试用的舵机ID
extern const uint8_t TEST_SERVO_ID_1;
extern const uint8_t TEST_SERVO_ID_2;

// 全局ST3215对象指针
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

// 测试辅助函数声明
void printTestHeader(const char* testName);
void printTestResult(const char* description, bool result);

#endif // TEST_CORE_FUNCTION_H
