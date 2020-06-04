#include "helper_func.h"

//void byte_copy(uint8_t *dest, const uint8_t *src, int start, uint64_t length) {
//    for (int i = 0; i < length; ++i) {
//        dest[start + i] = src[i];
//    }
//}

void payload_len_to_uint8(const uint64_t src, uint8_t *dest) {
    // Convert the length of uint64_t to uint8_t[8] and copy to the response
    uint8_t length[8];
    memcpy(length, &src, sizeof(uint64_t));
    for (int i = 1; i < 9; i++) {
        dest[i] = length[8 - i];
    }
}

void uint64_to_uint8(const uint64_t src, uint8_t *dest) {
    // Convert the length of uint64_t to uint8_t[8] and copy to the response
    uint8_t length[8];
    memcpy(length, &src, sizeof(uint64_t));
    for (int i = 0; i < 8; i++) {
        dest[i] = length[i];
    }
}

