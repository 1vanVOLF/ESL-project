#ifndef HSV_COLOR_H
#define HSV_COLOR_H

#include <stdint.h>

#define HUE_MAX        360
#define PERCENT_MAX    100

typedef struct
{
    uint16_t hue;    
    uint8_t  saturation; 
    uint8_t  value;  
} hsv_t;

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_t;

rgb_t hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v);

#endif