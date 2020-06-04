#include <stdint.h>
#include <stdlib.h>
#include <string.h>

//void byte_copy(uint8_t *dest, const uint8_t *src, int start, uint64_t length);

void payload_len_to_uint8(const uint64_t src, uint8_t *dest);

void uint64_to_uint8(const uint64_t src, uint8_t *dest);
