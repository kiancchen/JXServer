#include "func_handler.h"

/**
 * Create the node storing the information of retrieve
 * @param filename filename
 * @param id session id
 * @param starting starting offset
 * @param length request length
 * @return the created node
 */
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
    pthread_mutex_init(&(node->mutex), NULL);
    node->multiplex = malloc(sizeof(struct multiplex));
    return node;
}



/**
 * Check if the linked list contains the specific node
 * @param linked_list The linked list to be searched
 * @param node The node to be looked for
 * @return The existing status of the ndoe
 */
uint8_t list_contains(struct linked_list *linked_list, struct node *node, struct node** existing) {
    struct node *cur = linked_list->head;
    uint8_t temp = NON_EXIST;
    struct node *temp_node = NULL;
    while (cur != NULL) {
        if (cur->id == node->id) {
            if (strcmp(cur->filename, node->filename) == 0 && cur->starting == node->starting &&
                cur->length == node->length) {
                if (cur->querying == 1) {
                    // This query is ongoing
                    *existing = cur;
                    return EXIST_QUERYING;
                } else {
                    // This query is finished
                    return EXIST_QUERIED;
                }
            } else {
                if (cur->querying == 1) {
                    // This query is ongoing
                    return SAME_ID_DIFF_OTHER_QUERYING;
                } else {
                    // This query is finished
                    temp_node = cur;
                    temp = SAME_ID_DIFF_OTHER_QUERIED;
                }
            }
        }
        cur = cur->next;
    }
    if (temp != NON_EXIST) {
        temp_node->querying = 1;
        return temp;
    }
    return NON_EXIST;
}

/**
 * Add the node to the linked list
 * @param linked_list The linked list to be added into
 * @param node The node to be added
 */
void add_node(struct linked_list *linked_list, struct node *node) {
    struct node *cur = linked_list->head;
    if (cur == NULL) {
        linked_list->head = node;
        return;
    }

    while (cur->next != NULL) {
        cur = cur->next;
    }
    cur->next = node;
}

/**
 * Free all memory used by a linked list
 * @param linked_list The list to be freed
 */
void destroy_linked_list(struct linked_list *linked_list) {
    struct node *cur = linked_list->head;

    while (cur != NULL) {
        struct node *temp = cur;
        cur = cur->next;
        free_node(temp);
    }

}

void free_node(struct node *temp) {
    free(temp->multiplex->buffer);
    free(temp->multiplex);
    free(temp->filename);
    free(temp);
}
