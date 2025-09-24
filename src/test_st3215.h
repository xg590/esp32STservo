#ifndef TEST_EXT_FUNC_H
#define TEST_EXT_FUNC_H

#include "st3215.h" 

// 外部舵机对象引用（在main.cpp中定义）
extern ST3215* servo;

// 扩展功能测试函数声明
void runAllExtTests();
void testTorqueModeFunction();
void testAccelerationFunction();         // 合并：set & get acceleration
void testPositionFunction();             // 合并：set & get position  
void testStatusAndIdFunction();          // 合并：getStatus & changeId
void testPositionCorrectionFunction();
void testGetPositionFunction();

#endif // TEST_EXT_FUNC_H
