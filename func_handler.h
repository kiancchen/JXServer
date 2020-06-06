#ifndef UTIL_H_
#define UTIL_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "dict.h"
#include "directory.h"
#include "linked_list.h"


#define CON_CLS (0)
#define INVALID_MSG (1)
#define ERROR_OCCUR (1)
#define SUCCESS (2)
#define LEN_ZERO (3)
#define make_header(type, com, req) (type << 4u | com << 3u | req << 2u)
#define HEADER_LENGTH (9)
#define RETRIEVE_INFO_LEN (20)

struct header {
    unsigned type: 4;
    unsigned compressed: 1;
    unsigned req_compress: 1;
    unsigned  : 2;
};

typedef struct {
    struct header *header;
    uint64_t length;
    uint8_t *payload;
} message;

struct data {
    int connect_fd;
    int listen_fd;
    message *msg;
};

void uint64_to_uint8(uint8_t *dest, uint64_t src);

void msg_to_response(const message *msg, uint8_t *response);

char *concatenate_filename(uint8_t *payload, char *dir_path, uint64_t length);

void retrieve_get_info(const uint8_t *request_payload, uint32_t *id, uint64_t *starting, uint64_t *len_data);

void send_error(int connect_fd);

void send_empty_retrieve(int connect_fd);

void uncompressed_response(uint8_t **response, const uint8_t *payload, uint64_t *length, uint8_t type);

void compress_response(struct dict *dict, uint8_t **response, const uint8_t *payload, uint64_t *length, uint8_t type);

void decompress_payload(struct dict *dict, const message *request, uint8_t **request_payload, uint64_t *length);

void echo_handler(const struct data *data, struct dict *dict, message *request);

void directory_list_handler(const struct data *data, struct dict *dict, char *dir_path, message *request);

uint8_t file_size_handler(const struct data *data, struct dict *dict, char *dir_path, message *request);

uint8_t retrieve_handler(const struct data *data, struct dict *dict, char *dir_path, struct linked_list *queue,
                         message *request);

void free_request(message *request);

#endif