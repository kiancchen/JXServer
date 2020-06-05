#ifndef LINKED_LIST_H_
#define LINKED_LIST_H_

#include "util.h"

#define NON_EXIST 0
#define EXIST 1
#define SAME_ID_DIFF_OTHER_QUERYED 2
#define SAME_ID_DIFF_OTHER_QUERYING 3

struct node {
    char *filename;
    uint32_t id;
    uint64_t starting;
    uint64_t length;
    struct node *next;
    uint8_t querying;
};

struct linked_list {
    struct node *head;
    pthread_mutex_t mutex;
};

struct node *new_node(char *filename, uint32_t id, uint64_t starting, uint64_t length);

uint8_t list_contains(struct linked_list *linked_list, struct node *node);

void add_node(struct linked_list* linked_list, struct node *node);

#endif
