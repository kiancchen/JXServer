#include "linked_list.h"


struct node *new_node(const char *filename, const uint32_t id, const uint64_t starting, uint64_t length) {
    struct node *node = malloc(sizeof(struct node));
    node->filename = filename;
    node->id = id;
    node->starting = starting;
    node->length = length;
    return node;
}

uint8_t list_contains(struct linked_list *linked_list, struct node *node) {
    struct node *cur = linked_list->head;
    while (cur != NULL) {
        if (cur->id == node->id) {
            if (strcmp(cur->filename, node->filename) == 0 && cur->starting == node->starting &&
                cur->length == node->length) {
                return EXIST;
            } else {
                if (cur->querying == 1) {
                    return SAME_ID_DIFF_OTHER_QUERYING;
                } else {
                    return SAME_ID_DIFF_OTHER_QUERYED;
                }
            }
        }
        cur = cur->next;
    }
    return NON_EXIST;
}

void add_node(struct linked_list* linked_list, struct node *node){
    struct node *cur = linked_list->head;
    while (cur->next != NULL) {
        cur = cur->next;
    }
    cur->next = node;
}
