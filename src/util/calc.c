#include "calc.h"

float approach(float value, float target, float amount) {
    if (value < target) {
        value += amount;
        if (value > target) value = target;
    } else if (value > target) {
        value -= amount;
        if (value < target) value = target;
    }
    return value;
}
