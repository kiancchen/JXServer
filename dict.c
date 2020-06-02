#include "dict.h"


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
    memset(dict->code, 0, sizeof(uint8_t) * code_sz);
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
            dict->length[len_i] = dict->length[len_i - 1] + num;

            len_i++;
            continue;
        }
        if (get_bit(buffer, i)) {
            set_bit(dict->code, code_i);
        }
        code_i++;
    }

//    for (int i = 1; i < 257; ++i) {
////        printf("From %d to %d\n", dict.length[i - 1], dict.length[i]);
//        printf("Length for %x: %d\n", i - 1, dict->length[i] - dict->length[i - 1]);
//        printf("code: ");
//        for (int j = dict->length[i - 1]; j < dict->length[i]; ++j) {
//            if (get_bit(dict->code, j)) {
//                printf("1");
//            } else {
//                printf("0");
//            }
//        }
//        puts("");
//    }
}

int get_code_length(struct dict *dict, const uint8_t *payload, uint8_t payload_length) {
    int code_length = 0;
    for (int i = 0; i < payload_length; ++i) {
        uint8_t p = payload[i];
        code_length += dict->length[p + 1] - dict->length[p];
    }
    return code_length;
}

uint8_t *get_code(const struct dict *dict, const uint8_t payload) {
    int start = dict->length[payload];
    int end = dict->length[payload + 1];
    int length = end - start;
    uint8_t len_bytes = upper_divide(length, 8);
    uint8_t *code = malloc(sizeof(uint8_t) * len_bytes);
    memset(code, 0, len_bytes);
    int code_i = 0;
    for (int i = start; i < end; ++i) {
        if (get_bit(dict->code, i)) {
            set_bit(code, code_i);
        }
        code_i++;
    }
    return code;
}

void set_code(uint8_t *dest, uint8_t *src, int start, int length) {
    for (int i = 0; i < length; ++i) {
        if (get_bit(src, i)) {
            set_bit(dest, start + i);
        }
    }
}


uint8_t *compress(struct dict *dict, const uint8_t *payloads, uint64_t payload_length) {
    int total_length = get_code_length(dict, payloads, payload_length);
    uint8_t n_padding = len_padding(total_length);
    total_length = upper_divide(total_length, 8);
    uint8_t *compressed = malloc(sizeof(uint8_t) * (total_length + 1));
    memset(compressed, 0, total_length + 1);
    int start = 0;
    for (int i = 0; i < payload_length; ++i) {
        uint8_t payload = payloads[i];
//        puts("Original: ");
//        for (int j = 0; j < 8; ++j) {
//            if (get_bit(&payload, j)) {
//                printf("1");
//            } else {
//                printf("0");
//            }
//        }
//        puts("");
//        puts("Code: ");

        int length = get_code_length(dict, &payload, 1);
        uint8_t *code = get_code(dict, payload);
//        for (int j = 0; j < length; ++j) {
//            if (get_bit(code, j)) {
//                printf("1");
//            } else {
//                printf("0");
//            }
//        }
//        puts("");
        set_code(compressed, code, start, length);
        start += length;
    }
    compressed[total_length] = n_padding;
    return compressed;
}



//int main(void) {
//    struct dict *dict = malloc(sizeof(struct dict));
//    read_dict(dict);
////    // test length
//    uint8_t payloads[2] = {0x00, 0xa};
//    uint8_t *compressed = compress(dict, payloads, 2);
//
//    for (int j =0; j < 40; ++j) {
//        if (get_bit(compressed, j)) {
//            printf("1");
//        } else {
//            printf("0");
//        }
//    }
//    puts("");
//
//
//
////    for (int i = 1; i < 257; ++i) {
//////        printf("From %d to %d\n", dict.length[i - 1], dict.length[i]);
////        printf("Length for %x: %d\n", i - 1, dict.length[i] - dict.length[i - 1]);
////        printf("code: ");
////        for (int j = dict.length[i - 1]; j < dict.length[i]; ++j) {
////            if (get_bit(dict.code, j)) {
////                printf("1");
////            } else {
////                printf("0");
////            }
////        }
////        puts("");
////    }
//////    for (int i = 0; i < 28; ++i) {
//////        if (get_bit(dict->code, i)) {
//////            printf("1");
//////        }else{
//////            printf("0");
//////        }
//////    }
////
//}