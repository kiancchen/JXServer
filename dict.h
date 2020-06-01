#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct dict {
    uint8_t length[257];
    uint8_t *code;
};


size_t file_size(FILE *fp);

uint8_t get_bit(uint8_t *buffer, int index);

void set_bit(uint8_t *code, int index);

uint8_t read_eight_bits(uint8_t *buffer, int start);

void read_dict(struct dict *dict);