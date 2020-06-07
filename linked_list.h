#ifndef LINKED_LIST_H_
#define LINKED_LIST_H_

#include "func_handler.h"

#define NON_EXIST 0
#define EXIST_QUERYING 1
#define EXIST_QUERIED 4
#define SAME_ID_DIFF_OTHER_QUERIED 2
#define SAME_ID_DIFF_OTHER_QUERYING 3

struct multiplex{
    char* buffer;
    uint64_t buffer_size;
    uint64_t sent_size;
};


struct node {
    char *filename;
    uint32_t id;
    uint64_t starting;
    uint64_t length;
    struct node *next;
    uint8_t querying;
    struct multiplex *multiplex;
    pthread_mutex_t mutex;
};

struct linked_list {
    struct node *head;
    pthread_mutex_t mutex;
};

struct node *new_node(char *filename, uint32_t id, uint64_t starting, uint64_t length);

uint8_t list_contains(struct linked_list *linked_list, struct node *node);

void add_node(struct linked_list* linked_list, struct node *node);

void destroy_linked_list(struct linked_list* linked_list);

void free_node(struct node *temp);

#endif
