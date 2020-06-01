#include <stdint.h>

struct dict {
    uint8_t length[257];
    uint8_t *code;
};


void read_dict(struct dict *dict);