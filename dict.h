#ifndef DICT_H_
#define DICT_H_

#include "func_handler.h"
#include <dirent.h>

#define upper_divide(a, b) ((a % b == 0) ? (a / b) : ((a / b) + 1))
#define len_padding(a) (((a % 8) == 0) ? (0) : (8 - (a % 8)))


struct dict {
    int length[257];
    uint8_t *code;
};

size_t file_size(FILE *fp);

uint8_t get_bit(uint8_t *buffer, int index);

void set_bit(uint8_t *code, int index);

uint8_t read_eight_bits(uint8_t *buffer, int start);

void read_dict(struct dict *dict);

int get_code_length(struct dict *dict, const uint8_t *payload, uint64_t payload_length);

uint8_t *compress(struct dict *dict, const uint8_t *payloads, uint64_t payload_length);

uint8_t *decompress(struct dict *dict, uint8_t *compressed, const uint64_t compressed_length, uint64_t *num_decompressed);

#endif