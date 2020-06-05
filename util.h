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
#define SUCCESS (2)
#define LEN_ZERO (3)
#define make_header(type, com, req) (type << 4 | com << 3 | req << 2)
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
    message *msg;
};

void uint64_to_uint8(uint8_t *dest, uint64_t src);

#endif