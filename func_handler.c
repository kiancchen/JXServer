#ifndef HELPER_FUNC_C_
#define HELPER_FUNC_C_

#include "func_handler.h"

void uint64_to_uint8(uint8_t *dest, uint64_t src) {
    // Convert the length of uint64_t to uint8_t[8] and copy to the response
    memcpy(dest, &src, sizeof(uint64_t));
}

/**
 *
 * @param msg Source to be converted
 * @param response The list of uint8_t where stores the converted message
 */
void msg_to_response(const message *msg, uint8_t *response) {
    struct header *header = msg->header;
    // Convert the header to one byte of uint8_t
    response[0] = make_header(header->type, header->compressed, header->req_compress);
    uint64_to_uint8(response + 1, htobe64(msg->length));
//    payload_len_to_uint8(msg->length, response);

    // Copy the payload
    for (int i = 0; i < msg->length; i++) {
        response[i + 9] = msg->payload[i];
    }
}

char *concatenate_filename(uint8_t *payload, uint64_t length) {
    char *filename = malloc(sizeof(char) * (strlen(dir_path) + length + 2));
    filename[strlen(dir_path) + length + 1] = '\0';
    memcpy(filename, dir_path, strlen(dir_path));
    filename[strlen(dir_path)] = '/';
    memcpy(filename + strlen(dir_path) + 1, payload, length);
    return filename;
}

void retrieve_get_info(const uint8_t *request_payload, uint32_t *id, uint64_t *starting, uint64_t *len_data) {
    //  get the session id
    memcpy(id, request_payload, 4);
    (*id) = htobe32((*id));

    // get the starting offset
    memcpy(starting, request_payload + 4, 8);
    (*starting) = htobe64((*starting));

    // get the length of data required
    memcpy(len_data, request_payload + 12, 8);
    *len_data = htobe64(*len_data);
}

/**
 *
 * @param connect_fd connection file description
 */
void send_error(int connect_fd) {
    uint8_t error_header = make_header(0xf, 0, 0);
    uint64_t error_payload = 0x0;
    send(connect_fd, &error_header, sizeof(uint8_t), 0);
    send(connect_fd, &error_payload, sizeof(uint64_t), 0);
    close(connect_fd);
}

void send_empty_retrieve(int connect_fd) {
    uint8_t header = make_header(0x7, 0, 0);
    uint64_t payload = 0x0;
    send(connect_fd, &header, sizeof(uint8_t), 0);
    send(connect_fd, &payload, sizeof(uint64_t), 0);
    close(connect_fd);
}

void uncompressed_response(uint8_t **response, const uint8_t *payload, uint64_t *length,
                           uint8_t type) {
    // if compression not required
    (*response) = malloc(sizeof(uint8_t) * ((*length) + HEADER_LENGTH));
    (*response)[0] = make_header(type, 0, 0);
    // fill the payload length bytes
    uint64_to_uint8((*response) + 1, htobe64((*length)));
    // fill the payload as file list
    memcpy((*response) + 9, payload, (*length));
    (*length) += HEADER_LENGTH;
}

void compress_response(uint8_t **response, const uint8_t *payload, uint64_t *length, uint8_t type) {
    uint64_t compressed_length = get_code_length(&dict, payload, *length);
    compressed_length = upper_divide(compressed_length, 8) + 1;
    // make the response
    *response = malloc(sizeof(uint8_t) * (HEADER_LENGTH + compressed_length));
    (*response)[0] = make_header(type, 1, 0);
    // fill the payload length bytes
    uint64_to_uint8((*response) + 1, htobe64(compressed_length));
    // get the compressed payload and copy to the response
    uint8_t *compressed = compress(&dict, payload, *length);
    memcpy((*response) + 9, compressed, compressed_length);
    // the final length of the whole response
    *length = compressed_length + HEADER_LENGTH;
    free(compressed);
}

void decompress_payload(const message *request, uint8_t **request_payload, uint64_t *length) {
    if (request->header->compressed == (unsigned) 0) {
        (*request_payload) = request->payload;
        (*length) = request->length;
    } else {
        (*request_payload) = decompress(&dict, request->payload, request->length, length);
    }
}

#endif