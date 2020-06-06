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

char *concatenate_filename(uint8_t *payload, char *dir_path, uint64_t length) {
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

void compress_response(struct dict *dict, uint8_t **response, const uint8_t *payload, uint64_t *length, uint8_t type) {
    uint64_t compressed_length = get_code_length(dict, payload, *length);
    compressed_length = upper_divide(compressed_length, 8) + 1;
    // make the response
    *response = malloc(sizeof(uint8_t) * (HEADER_LENGTH + compressed_length));
    (*response)[0] = make_header(type, 1, 0);
    // fill the payload length bytes
    uint64_to_uint8((*response) + 1, htobe64(compressed_length));
    // get the compressed payload and copy to the response
    uint8_t *compressed = compress(dict, payload, *length);
    memcpy((*response) + 9, compressed, compressed_length);
    // the final length of the whole response
    *length = compressed_length + HEADER_LENGTH;
    free(compressed);
}

void decompress_payload(struct dict *dict, const message *request, uint8_t **request_payload, uint64_t *length) {
    if (request->header->compressed == (unsigned) 0) {
        *request_payload = malloc(sizeof(uint8_t) * request->length);
        memcpy(*request_payload, request->payload, request->length);
//        (*request_payload) = request->payload;
        (*length) = request->length;
    } else {
        (*request_payload) = decompress(dict, request->payload, request->length, length);
    }
}

void echo_handler(int connect_fd, struct dict *dict, message *request) {
    uint8_t *response;
    uint64_t length = request->length;
    if (request->header->compressed != (unsigned) 0 || request->header->req_compress != (unsigned) 1) {
        length += HEADER_LENGTH;
        response = malloc(sizeof(uint8_t) * length);
        // Copy the request
        msg_to_response(request, response);
        // Modify the header
        response[0] = make_header(0x1, request->header->compressed, 0);
    } else {
        compress_response(dict, &response, request->payload, &length, 0x1);
    }
    // Send the response
    send(connect_fd, response, sizeof(uint8_t) * length, 0);
    free(response);
    free_request(request);
}

void directory_list_handler(int connect_fd, struct dict *dict, char *dir_path, message *request) {
    uint8_t *response;
    uint64_t length;
    // get the list of files
    char *file_list = get_file_list(dir_path, &length);

    uint8_t *payload;
    if (length == 0) {
        payload = 0x00;
        length = 1;
    } else {
        // convert char* file_list to uint8_t
        payload = malloc(sizeof(uint8_t) * length);
        memcpy(payload, file_list, length);
    }

    if (request->header->req_compress == (unsigned) 0) {
        uncompressed_response(&response, payload, &length, 0x3);
    } else {
        compress_response(dict, &response, payload, &length, 0x3);
    }

    send(connect_fd, response, sizeof(uint8_t) * length, 0);
    free(response);
    free(file_list);
    free(payload);
    free_request(request);
}


uint8_t file_size_handler(int connect_fd, struct dict *dict, char *dir_path, message *request) {
    char *filename = concatenate_filename(request->payload, dir_path, request->length);
    FILE *f = fopen(filename, "r");
    if (!f) {
        return ERROR_OCCUR;
    }
    size_t sz = file_size(f);
    fclose(f);
    free(filename);
    uint64_t length = 8;
    // convert to network byte order
    uint64_t size_64 = htobe64(sz);
    uint8_t *payload = malloc(sizeof(uint8_t) * length);
    uint64_to_uint8(payload, size_64);

    uint8_t *response;
    if (request->header->req_compress == (unsigned) 0) {
        uncompressed_response(&response, payload, &length, 0x5);
    } else {
        compress_response(dict, &response, payload, &length, 0x5);
    }
    send(connect_fd, response, sizeof(uint8_t) * length, 0);
    free(payload);
    free(response);
    free_request(request);
    return SUCCESS;
}

uint8_t retrieve_handler(int connect_fd, struct dict *dict, char *dir_path, struct linked_list *queue,
                         message *request) {
    uint8_t *response;
    uint64_t length;
    uint8_t *request_payload;
    decompress_payload(dict, request, &request_payload, &length);
    // get the information from the file_data: session id; starting offset; data length;
    uint32_t id;
    uint64_t starting;
    uint64_t len_data;
    retrieve_get_info(request_payload, &id, &starting, &len_data);

    // Concatenate the filename
    uint64_t len_filename = length - RETRIEVE_INFO_LEN;
    char *filename = concatenate_filename(request_payload + RETRIEVE_INFO_LEN, dir_path, len_filename);

    // process request queue
    struct node *node = new_node(filename, id, starting, len_data);
    pthread_mutex_lock(&(queue->mutex));
    uint8_t signal = list_contains(queue, node);
    if (signal == NON_EXIST) {
        add_node(queue, node);
    } else if (signal == EXIST) {
        send_empty_retrieve(connect_fd);
        pthread_mutex_unlock(&(queue->mutex));
        free(filename);
        free(request_payload);
        return ERROR_OCCUR;
    } else if (signal == SAME_ID_DIFF_OTHER_QUERYING) {
        send_error(connect_fd);
        pthread_mutex_unlock(&(queue->mutex));
        free(filename);
        free(request_payload);
        return ERROR_OCCUR;
    } else if (signal == SAME_ID_DIFF_OTHER_QUERYED) {
        add_node(queue, node);
    }
    pthread_mutex_unlock(&(queue->mutex));
    // end of queue process

    // open and read the file
    FILE *f = fopen(filename, "r");
    free(filename);
    size_t sz;
    if (!f || (starting + len_data) > (sz = file_size(f))) {
        send_error(connect_fd);
        free(request_payload);
        return ERROR_OCCUR;
    }
    char *buffer = malloc(sizeof(char) * sz);
    fread(buffer, sizeof(char), sz, f);
    fclose(f);

    //make the file_data
    uint8_t *file_data = malloc(sizeof(uint8_t) * len_data);
    memcpy(file_data, buffer + starting, len_data);
    free(buffer);
    // Concatenate the payloads
    length = len_data + RETRIEVE_INFO_LEN;
    uint8_t *uncompressed_payload = malloc(sizeof(uint8_t) * length);
    memcpy(uncompressed_payload, request_payload, 20);
    memcpy(uncompressed_payload + 20, file_data, len_data);
    free(file_data);
    // make the response
    if (request->header->req_compress == (unsigned) 0) {
        uncompressed_response(&response, uncompressed_payload, &length, 0x7);
    } else {
        compress_response(dict, &response, uncompressed_payload, &length, 0x7);
    }
    node->querying = 0;
    send(connect_fd, response, sizeof(uint8_t) * length, 0);
    free(response);
    free(uncompressed_payload);
    free_request(request);
    free(request_payload);
    return SUCCESS;
}

void free_request(message *request) {
    free(request->header);
    free(request->payload);
    request->payload = NULL;
    free(request);
}

#endif