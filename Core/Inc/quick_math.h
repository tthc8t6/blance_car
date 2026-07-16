#ifndef __QMATH_H__
#define __QMATH_H__

#include "stm32f1xx_hal.h"
#include "math.h"

float qsin(float x);
float qcos(float x);
float qtan(float x);
float qasin(float x);
float qacos(float x);
float qatan(float x);
float qatan2(float y, float x);

#endif // __QMATH_H__
