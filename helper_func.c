#ifndef HELPER_FUNC_C_
#define HELPER_FUNC_C_

#include "helper_func.h"


void uint64_to_uint8(const uint64_t src, uint8_t *dest) {
    // Convert the length of uint64_t to uint8_t[8] and copy to the response
//    uint8_t length[8];
    memcpy(dest, &src, sizeof(uint64_t));
//    for (int i = 0; i < 8; i++) {
//        dest[i] = length[i];
//    }
}

#endif