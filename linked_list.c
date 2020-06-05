#include "util.h"


struct node *new_node(char *filename, uint32_t id, uint64_t starting, uint64_t length) {
    struct node *node = malloc(sizeof(struct node));
    node->filename = malloc(sizeof(char) * (strlen(filename) + 1));
    strcpy(node->filename, filename);
    node->filename[strlen(filename)] = '\0';
    node->id = id;
    node->starting = starting;
    node->length = length;
    node->next = NULL;
    node->querying = 1;
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
    if (cur == NULL){
        linked_list->head = node;
        return;
    }

    while (cur->next != NULL) {
        cur = cur->next;
    }
    cur->next = node;
}
