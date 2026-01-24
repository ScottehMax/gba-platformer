#include "debug_utils.h"

int int_to_string(int value, char* buffer, int bufferSize, const char* prefix) {
    int idx = 0;

    // Add prefix if provided
    if (prefix != 0) {
        while (*prefix && idx < bufferSize - 1) {
            buffer[idx++] = *prefix++;
        }
    }

    // Handle negative numbers
    int absValue = value;
    if (value < 0) {
        if (idx < bufferSize - 1) {
            buffer[idx++] = '-';
        }
        absValue = -value;
    }

    // Convert digits
    int digits[10];  // Enough for 32-bit int
    int numDigits = 0;

    if (absValue == 0) {
        digits[numDigits++] = 0;
    } else {
        int temp = absValue;
        while (temp > 0 && numDigits < 10) {
            digits[numDigits++] = temp % 10;
            temp /= 10;
        }
    }

    // Write digits in reverse order
    for (int i = numDigits - 1; i >= 0 && idx < bufferSize - 1; i--) {
        buffer[idx++] = '0' + digits[i];
    }

    // Null terminate
    buffer[idx] = '\0';

    return idx;
}
