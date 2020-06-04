#ifndef HELPER_FUNC_C_
#define HELPER_FUNC_C_

#include "helper_func.h"


void uint64_to_uint8(uint8_t *dest, const uint64_t src) {
    // Convert the length of uint64_t to uint8_t[8] and copy to the response
    memcpy(dest, &src, sizeof(uint64_t));
}

#endif