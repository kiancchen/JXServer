#include "func_handler.h"


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
    free(buffer);

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

int get_code_length(struct dict *dict, const uint8_t *payload, uint64_t payload_length) {
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


        int length = get_code_length(dict, &payload, 1);
        uint8_t *code = get_code(dict, payload);

        set_code(compressed, code, start, length);
        start += length;
        free(code);
    }
    compressed[total_length] = n_padding;
    return compressed;
}

uint8_t *decompress(struct dict *dict, uint8_t *compressed, const uint64_t compressed_length, uint64_t *num_decompressed) {
    uint8_t *decompressed_code = malloc(sizeof(uint8_t) * 256);
    *num_decompressed = 0;
    uint64_t cursor_index = 0;
    uint8_t padding_length = compressed[compressed_length - 1];
    uint64_t bit_length = (compressed_length - 1) * 8 - padding_length;
    uint8_t *code = malloc(sizeof(uint8_t) * 32);
    memset(code, 0, 32);
    uint16_t code_length = 0;
    while (cursor_index < bit_length) {
        if (get_bit(compressed, cursor_index)) {
            set_bit(code, code_length);
        }
        cursor_index++;
        code_length++;
        for (int i = 0; i < 256; ++i) {
            int start = dict->length[i];
            int end = dict->length[i + 1];
            uint16_t matched_length = end - start;

            if (code_length != matched_length) {
                continue;
            }

            int found = 1;
            for (int j = 0; j < matched_length; ++j) {
                if (get_bit(code, j) != get_bit(dict->code, start + j)) {
                    found = 0;
                    break;
                }
            }
            if (found) {
                decompressed_code[(*num_decompressed)++] = i;
                memset(code, 0, code_length);
                code_length = 0;
                break;
            }
        }
    }
    uint8_t *result = malloc(sizeof(uint8_t) * *num_decompressed);
    memcpy(result, decompressed_code, *num_decompressed);
    free(decompressed_code);
    free(code);
    return result;
}


//int main(void) {
//    struct dict *dict = malloc(sizeof(struct dict));
//    read_dict(dict);
////    // test length
//    uint8_t compressed[75] = {0xd9, 0x8b, 0x49, 0x2d, 0x98, 0xb4, 0x91, 0xd9, 0x8b, 0x49, 0xd, 0x98, 0xb4, 0x8f, 0xd9,
//            0x8b, 0x48, 0xed, 0x98, 0xb4, 0x8e, 0xd9, 0x8b, 0x48, 0xed, 0x98, 0xb4, 0x8e, 0xd9, 0x8b, 0x48, 0xed,
//            0x98, 0xb4, 0x8e, 0xd9, 0x8b, 0x48, 0xed, 0x98, 0xb4, 0x91, 0xd9, 0x8b, 0x48, 0xed, 0x98, 0xb4, 0x8e,
//            0xd9, 0x8b, 0x48, 0xed, 0x98, 0xb4, 0x8e, 0xd9, 0x8b, 0x48, 0xed, 0x98, 0xb4, 0x8e, 0xd9, 0x8b, 0x48,
//            0xeb, 0x8b, 0x97, 0x26, 0xfb, 0x31, 0x69, 0x1c, 0x1};
////    uint8_t little_c[75];
////    for (int i = 0; i < 75; ++i) {
////        little_c[i] = compressed[74 - i];
////    }
//
//    uint8_t *decode = decompress(dict, compressed, 75);
//    for (int i = 0; i < 27; ++i) {
//        printf("%x ", decode[i]);
//    }
//
////    for (int j =0; j < 8; ++j) {
////        if (get_bit(&compressed, j)) {
////            printf("1");
////        } else {
////            printf("0");
////        }
////    }
////    puts("");
//}