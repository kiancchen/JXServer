#include "func_handler.h"

/**
 * Get the file size of the given FILE pointer
 * @param fp The FILE pointer
 * @return The size of the file
 */
size_t file_size(FILE *fp) {
    fseek(fp, 0, SEEK_END);
    size_t sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    return sz;
}

/**
 * Get the bit at the given index of given list of uint8_t
 * @param buffer The list of uint8_t to be searched
 * @param index  The index to be searched
 * @return The bit on that position
 */
uint8_t get_bit(uint8_t *buffer, int index) {
    return ((buffer[index / 8]) & (1 << (7 - index % 8))) != 0;
}

/**
 * Set the bit at the given index of given list of uint8_t to 1
 * @param code The list of uint8_t to be set
 * @param index The index to be set
 */
void set_bit(uint8_t *code, int index) {
    code[index / 8] |= 1 << (7 - index % 8);
}

/**
 * Read eight bits from start of given list of uint8_t
 * @param buffer The list of uint8_t to be read
 * @param start The start index to be read from
 * @return The eight bit as an uint8_t
 */
uint8_t read_eight_bits(uint8_t *buffer, int start) {
    uint8_t result = 0;
    for (int i = 0; i < 8; ++i) {
        // if the bit is 1, set it in the result
        if (get_bit(buffer, start)) {
            set_bit(&result, i);
        }
        start += 1;
    }
    return result;
}

/**
 * Read the dictionary from the file named "compression.dict"
 * @param dict The struct dict where we store the information
 */
void read_dict(struct dict *dict) {
    // Read the whole file and store it in the buffer
    FILE *fp = fopen("compression.dict", "r");
    size_t total_sz = file_size(fp);
    size_t code_sz = total_sz - 256;
    size_t total_bits = total_sz * 8;
    uint8_t *buffer = malloc(sizeof(uint8_t) * total_sz);
    fread(buffer, sizeof(uint8_t), total_sz, fp);
    fclose(fp);

    dict->code = malloc(sizeof(uint8_t) * code_sz);
    memset(dict->code, 0, sizeof(uint8_t) * code_sz);
    dict->length[0] = 0;
    // Analysis the file
    int end = 0; // Each code ends at this index
    int len_i = 1; // The index where to store the length of codes in dict->length[]
    int code_i = 0; // The index where to store the code in dict->code[]
    for (int i = 0; i < total_bits; ++i) {
        // i is the number of byte to be encoded
        if (i == end && total_bits - end >= 8) {
            // If i reaches the end of a code AND there's more than 1 byte remaining (because of padding bits)
            // Read the length of next code, and store information of previous code
            int length_code = read_eight_bits(buffer, i);
            end += 8 + length_code;
            i += 7;
            dict->length[len_i] = dict->length[len_i - 1] + length_code;

            len_i++;
            continue;
        }
        // Set code
        if (get_bit(buffer, i)) {
            set_bit(dict->code, code_i);
        }
        code_i++;
    }
    free(buffer);

}

/**
 * Get the total number of bits needed to compress the payload
 * @param dict Where stores the codes
 * @param payload The information needed to be compressed
 * @param payload_length The length of payload
 * @return
 */
int get_code_length(struct dict *dict, const uint8_t *payload, uint64_t payload_length) {
    int code_length = 0;
    for (int i = 0; i < payload_length; ++i) {
        uint8_t p = payload[i];
        code_length += dict->length[p + 1] - dict->length[p];
    }
    return code_length;
}

/**
 * Get the code of a specific byte
 * @param dict Where stores the codes
 * @param payload The byte to be compressed
 * @return
 */
uint8_t *get_code(const struct dict *dict, const uint8_t payload) {
    int start = dict->length[payload];
    int end = dict->length[payload + 1];
    int length = end - start;
    uint8_t len_bytes = upper_divide(length, 8);
    uint8_t *code = malloc(sizeof(uint8_t) * len_bytes);
    memset(code, 0, len_bytes);
    int code_i = 0;
    // Read dict->code[] from start to end
    for (int i = start; i < end; ++i) {
        if (get_bit(dict->code, i)) {
            set_bit(code, code_i);
        }
        code_i++;
    }
    return code;
}

/**
 * Set the bits in dest to that in src from start with length of bits
 * @param dest The destination to be set
 * @param src  The source to be read
 * @param start The start index
 * @param length The length to be set
 */
void set_code(uint8_t *dest, uint8_t *src, int start, int length) {
    for (int i = 0; i < length; ++i) {
        if (get_bit(src, i)) {
            set_bit(dest, start + i);
        }
    }
}

/**
 * Compress the payload and return the compressed payload
 * @param dict Where stores the codes
 * @param payloads The payloads to be compressed
 * @param payload_length The length of the payload
 * @return The compressed payload
 */
uint8_t *compress(struct dict *dict, const uint8_t *payloads, uint64_t payload_length) {
    int total_length = get_code_length(dict, payloads, payload_length); // The length of bits of codes
    uint8_t n_padding = len_padding(total_length); // The length of padding bits
    total_length = upper_divide(total_length, 8); // The length of bytes of codes
    uint8_t *compressed = malloc(sizeof(uint8_t) * (total_length + 1));
    memset(compressed, 0, total_length + 1);
    int start = 0;
    // Set codes one by one payload
    for (int i = 0; i < payload_length; ++i) {
        uint8_t payload = payloads[i];

        int length = get_code_length(dict, &payload, 1);
        uint8_t *code = get_code(dict, payload); // The code of current payload

        set_code(compressed, code, start, length);
        start += length;
        free(code);
    }
    compressed[total_length] = n_padding;
    return compressed;
}

/**
 * Decompress the payloads
 * @param dict Where stores the codes
 * @param compressed The compressed payload needed to be decompressed
 * @param compressed_length The length of compressed payload
 * @param num_decompressed (Out param) The length of decompressed payloads
 * @return The decompressed payloads
 */
uint8_t *decompress(struct dict *dict, uint8_t *compressed, const uint64_t compressed_length, uint64_t *num_decompressed) {
    uint8_t *decompressed_code = malloc(sizeof(uint8_t) * 256);
    *num_decompressed = 0;
    uint64_t cursor_index = 0; // Index of current bit of compressed payload
    uint8_t padding_length = compressed[compressed_length - 1];
    uint64_t compressed_bit_length = (compressed_length - 1) * 8 - padding_length;
    uint8_t *code = malloc(sizeof(uint8_t) * 32); // The temp storing current searching code
    memset(code, 0, 32);
    uint16_t code_length = 0;
    // Search the compressed code one by one bit
    while (cursor_index < compressed_bit_length) {
        // Set the temp code
        if (get_bit(compressed, cursor_index)) {
            set_bit(code, code_length);
        }
        cursor_index++;
        code_length++;
        // Search the whole dictionary for the temp code
        for (int i = 0; i < 256; ++i) {
            int start = dict->length[i];
            int end = dict->length[i + 1];
            uint16_t matched_length = end - start;

            if (code_length != matched_length) {
                // If the length is not matched
                continue;
            }

            int found = 1;
            for (int j = 0; j < matched_length; ++j) {
                // Check bits one by one
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
