#ifndef DEBUG_UTILS_H
#define DEBUG_UTILS_H

/**
 * Converts an integer to a string with optional prefix
 *
 * @param value The integer value to convert
 * @param buffer The output buffer to write the string to
 * @param bufferSize Maximum size of the buffer
 * @param prefix Optional prefix string (e.g., "X: ", "VY: "), can be NULL
 * @return Number of characters written (excluding null terminator)
 */
int int_to_string(int value, char* buffer, int bufferSize, const char* prefix);

#endif
