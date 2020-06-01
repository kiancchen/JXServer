#include "dict.h"
#include <stdio.h>
#include <stdlib.h>

size_t file_size(FILE *fp) {
    fseek(fp, 0, SEEK_END);
    size_t sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    return sz;
}

uint8_t get_bit(uint8_t *buffer, int index) {
    return ((buffer[index / 8]) & (1 << (7 - index % 8))) != 0;
}

void set_bit(uint8_t *code, int index) {
    code[index / 8] |= 1 << (7 - index % 8);
}

uint8_t read_eight_bits(uint8_t *buffer, int start) {
    uint8_t num = 0;
    for (int i = 0; i < 8; ++i) {
        if (get_bit(buffer, start)) {
//            printf("1");
            set_bit(&num, i);
        } else {
//            printf("0");
        }

        start += 1;
    }
//    printf("\n");
    return num;
}

void read_dict(struct dict *dict) {
    FILE *fp = fopen("compression.dict", "r");
    size_t total_sz = file_size(fp);
    size_t code_sz = total_sz - 256;
    size_t total_bits = total_sz * 8;
    uint8_t *buffer = malloc(sizeof(uint8_t) * total_sz);
    fread(buffer, sizeof(uint8_t), total_sz, fp);
    fclose(fp);
//    for (int i = 0; i < 20; ++i) {
//        printf("%x ", buffer[i]);
//    }
    // init dict

    dict->code = malloc(sizeof(uint8_t) * code_sz);
    dict->length[0] = 0;
    // store
    int end = 0;
    int len_i = 1;
    int code_i = 0;
    for (int i = 0; i < total_bits; ++i) {
        if (i == end && total_bits - end >= 8) {
            int num = read_eight_bits(buffer, i);
            end += 8 + num;
            i += 7;
            dict->length[len_i] = num;

            len_i++;
            continue;
        }
        if (get_bit(buffer, i)) {
            set_bit(dict->code, code_i);
        }
        code_i++;
    }

}


//int main(void) {
//    struct dict *dict = malloc(sizeof(struct dict));
//    read_dict(dict);
//
////    for (int i = 0; i < 28; ++i) {
////        if (get_bit(dict->code, i)) {
////            printf("1");
////        }else{
////            printf("0");
////        }
////    }
//
//    int end = 0;
//    for (int i = 1; i < 257; ++i) {
//        printf("Length for %x: %d\n", i - 1, dict->length[i]);
//        printf("code: ");
//        end += dict->length[i];
//        for (int j = end - dict->length[i]; j < end; ++j) {
//            if (get_bit(dict->code, j)) {
//                printf("1");
//            } else {
//                printf("0");
//            }
//        }
//        puts("");
//    }
// }