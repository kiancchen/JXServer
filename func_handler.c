#ifndef HELPER_FUNC_C_
#define HELPER_FUNC_C_

#include "func_handler.h"

/**
 * Convert an uint64_t to a list of uint8_t
 * @param dest stores the result
 * @param src  uint64_t to be converted
 */
void uint64_to_uint8(uint8_t *dest, uint64_t src) {
    // Convert the length of uint64_t to uint8_t[8] and copy to the response
    memcpy(dest, &src, sizeof(uint64_t));
}

/**
 * Convert the struct message to a list of uint8_t
 * @param msg Source to be converted
 * @param response The list of uint8_t where stores the converted message
 */
void msg_to_response(const message *msg, uint8_t *response) {
    struct header *header = msg->header;
    // Convert the header to one byte of uint8_t
    response[0] = make_header(header->type, header->compressed, header->req_compress);
    uint64_to_uint8(response + 1, htobe64(msg->length));

    // Copy the payload
    for (int i = 0; i < msg->length; i++) {
        response[i + 9] = msg->payload[i];
    }
}

/**
 * Concatenate the filename with the directory path
 * @param file The original filename
 * @param dir_path The directory to be concatenated
 * @param length the length of the original filename
 * @return The concatenated filename
 */
char *concatenate_filename(uint8_t *file, char *dir_path, uint64_t length) {
    char *filename = malloc(sizeof(char) * (strlen(dir_path) + length + 2));
    filename[strlen(dir_path) + length + 1] = '\0';
    memcpy(filename, dir_path, strlen(dir_path));
    filename[strlen(dir_path)] = '/';
    memcpy(filename + strlen(dir_path) + 1, file, length);
    return filename;
}

/**
 * Get the retrieve query information
 * @param request_payload The request payload
 * @param id (Out param) Session ID
 * @param starting (Out param) The starting offset
 * @param len_data (Out param) The length of dada field
 */
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
 * Send an error message to the client
 * @param connect_fd Connection File Description
 */
void send_error(int connect_fd) {
    uint8_t error_header = make_header(0xf, 0, 0);
    uint64_t error_payload = 0x0;
    send(connect_fd, &error_header, sizeof(uint8_t), 0);
    send(connect_fd, &error_payload, sizeof(uint64_t), 0);
    close(connect_fd);
}

/**
 * Send an empty retrieve to the client
 * @param connect_fd Connection File Description
 */
void send_empty_retrieve(int connect_fd) {
    uint8_t header = make_header(0x7, 0, 0);
    uint64_t payload = 0x0;
    send(connect_fd, &header, sizeof(uint8_t), 0);
    send(connect_fd, &payload, sizeof(uint64_t), 0);
    close(connect_fd);
}

/**
 * Make a response of payload without compression
 * @param response (Out param) Store the response
 * @param payload The original payload
 * @param length The length of payload
 * @param type The type of response
 */
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


/**
 * Make a response of payload with compression
 * @param dict The code dictionary
 * @param response (Out param) Store the response
 * @param payload The original payload
 * @param length The length of payload
 * @param type The type of response
 */
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

/**
 * Check to decompress a payload
 * @param dict The code dictionary
 * @param request The request
 * @param request_payload The payload
 * @param length The length of payload
 */
void decompress_payload(struct dict *dict, const message *request, uint8_t **request_payload, uint64_t *length) {
    if (request->header->compressed == (unsigned) 0) {
        // If the payload does not need compression
        (*length) = request->length;
        (*request_payload) = malloc(sizeof(uint8_t) * *length);
        memcpy(*request_payload, request->payload, *length);

    } else {
        // If the payload need compression
        (*request_payload) = decompress(dict, request->payload, request->length, length);
    }
}

/**
 * Handle the echo functionality
 * @param data The data stores the connect fd
 * @param dict The code dictionary
 * @param request The request
 */
void echo_handler(const struct data *data, struct dict *dict, const message *request) {
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
    send(data->connect_fd, response, sizeof(uint8_t) * length, 0);
    free(response);
}

/**
 * Handle directory list functionality
 * @param data The data stores the connect fd
 * @param dict The code dictionary
 * @param dir_path The directory path
 * @param request The request
 */
void directory_list_handler(const struct data *data, struct dict *dict, char *dir_path, const message *request) {
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
        // If it needs compression
        uncompressed_response(&response, payload, &length, 0x3);
    } else {
        // If it does not need compression
        compress_response(dict, &response, payload, &length, 0x3);
    }

    send(data->connect_fd, response, sizeof(uint8_t) * length, 0);
    free(response);
    free(file_list);
    free(payload);
}

/**
 * Handle the file size functionality
 * @param data The data stores the connect fd
 * @param dict The code dictionary
 * @param dir_path The directory path
 * @param request The request
 * @return The signal if this function runs well
 */
uint8_t file_size_handler(const struct data *data, struct dict *dict, char *dir_path, const message *request) {
    uint64_t length;
    uint8_t *request_payload;
    decompress_payload(dict, request, &request_payload, &length);
    // Concatenate the filename
    char *filename = concatenate_filename(request_payload, dir_path, length);
    free(request_payload);
    FILE *f = fopen(filename, "r");
    if (!f) {
        return ERROR_OCCUR;
    }
    size_t sz = file_size(f);
    fclose(f);
    free(filename);
    // convert to network byte order
    length = 8;
    uint64_t size_64 = htobe64(sz);
    uint8_t *uncompressed_payload = malloc(sizeof(uint8_t) * length);
    uint64_to_uint8(uncompressed_payload, size_64);

    uint8_t *response;
    if (request->header->req_compress == (unsigned) 0) {
        // If it needs compression
        uncompressed_response(&response, uncompressed_payload, &length, 0x5);
    } else {
        // If it does not need compression
        compress_response(dict, &response, uncompressed_payload, &length, 0x5);
    }
    send(data->connect_fd, response, sizeof(uint8_t) * length, 0);
    free(uncompressed_payload);
    free(response);
    return SUCCESS;
}

/**
 * Handle the retrieve file functionality
 * @param data The data stores the connect fd
 * @param dict The code dictionary
 * @param dir_path The directory path
 * @param queue The request queue
 * @param request The request
 * @return The signal if this function runs well
 */
uint8_t retrieve_handler(const struct data *data, struct dict *dict, char *dir_path, struct linked_list *queue,
                         const message *request) {
    // Check to decompress the payload
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
    struct node *existing = NULL;
    uint8_t signal = list_contains(queue, node, &existing);
    if (signal == NON_EXIST || signal == SAME_ID_DIFF_OTHER_QUERIED) {
        // If the request does not exist, add to the queue and respond to it
        add_node(queue, node);
    } else if (signal == EXIST_QUERIED) {
        // If the request exists, send an empty response
        send_empty_retrieve(data->connect_fd);
        free(filename);
        free(request_payload);
        free_node(node);
        pthread_mutex_unlock(&(queue->mutex));
        return ERROR_OCCUR;
    } else if (signal == SAME_ID_DIFF_OTHER_QUERYING) {
        // If the request with same session id is querying, send an error response
        send_error(data->connect_fd);
        free(filename);
        free(request_payload);
        free_node(node);
        pthread_mutex_unlock(&(queue->mutex));
        return ERROR_OCCUR;
    } else if (signal == EXIST_QUERYING) {
        node = existing;
    }
    pthread_mutex_unlock(&(queue->mutex));

    // end of queue process
    if (signal == NON_EXIST || signal == SAME_ID_DIFF_OTHER_QUERIED) {
        // open and read the file
        FILE *f = fopen(filename, "r");
        free(filename);
        size_t sz;
        if (!f || (starting + len_data) > (sz = file_size(f))) {
            // If the file cannot be opened or the range is bad
            send_error(data->connect_fd);
            free(request_payload);
            return ERROR_OCCUR;
        }
        node->multiplex->buffer_size = sz;
        node->multiplex->buffer = malloc(sizeof(char) * sz);
        fread(node->multiplex->buffer, sizeof(char), sz, f);
        fclose(f);
    }

    pthread_mutex_lock(&(node->mutex));
    if (!node->querying){
        send_empty_retrieve(data->connect_fd);
        free(request_payload);
        pthread_mutex_unlock(&(node->mutex));
        return SUCCESS;
    }
    //make the file_data
    uint8_t *file_data = malloc(sizeof(uint8_t) * node->length);
    memcpy(file_data, node->multiplex->buffer + starting, node->length);
    node->multiplex->sent_size += node->length;
    if (node->multiplex->sent_size == node->length) {
        node->querying = 0;
    }

    // Concatenate the payloads
    length = node->length + RETRIEVE_INFO_LEN;
    uint8_t *uncompressed_payload = malloc(sizeof(uint8_t) * length);
    memcpy(uncompressed_payload, request_payload, 20);
    memcpy(uncompressed_payload + 20, file_data, node->length);
    free(file_data);

    // make the response
    uint8_t *response;
    if (request->header->req_compress == (unsigned) 0) {
        // If it does not need compression
        uncompressed_response(&response, uncompressed_payload, &length, 0x7);
    } else {
        // If it does not need compression
        compress_response(dict, &response, uncompressed_payload, &length, 0x7);
    }
    send(data->connect_fd, response, sizeof(uint8_t) * length, 0);
    free(uncompressed_payload);
    free(response);

    pthread_mutex_unlock(&(node->mutex));
    free(request_payload);
    return SUCCESS;
}

#endif