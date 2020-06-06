#include "func_handler.h"

struct dict dict;
char *dir_path;
struct linked_list queue;


/**
 *
 * @param connect_fd Connection file description
 * @param request where to stores the request
 * @return 0 for connection closed; 1 for invalid input; 2 for success
 */
uint8_t read_request(int connect_fd, message *request) {
    // init as NULL
    request->header = NULL;
    request->payload = NULL;

    // Read the header and payload length
    uint8_t buffer[9];
    int num = recv(connect_fd, buffer, 9, 0);

    if (num <= 0) {
        return CON_CLS; // Connection closed
    }
    if (num != 9) {
        return INVALID_MSG; // invalid input
    }

    // Convert first type and stores in the struct header
    struct header *hd = malloc(sizeof(struct header));
    hd->type = buffer[0] >> 4u;
    hd->compressed = buffer[0] >> 3u;
    hd->req_compress = buffer[0] >> 2u;
    request->header = hd;

    // Read the length
    uint64_t length = buffer[8];
    for (int i = 1; i < 8; ++i) {
        length = ((unsigned) buffer[i] << ((8u - i) * 8u)) | length;
    }
    if (length == 0) {
        return LEN_ZERO; // invalid input
    }
    request->length = length;

    // Read the payload
    uint8_t *payload = malloc(sizeof(uint8_t) * length);
    num = recv(connect_fd, payload, length, 0);
    if (num <= 0) {
        return CON_CLS; //Connection closed
    }
    if (num != length) {
        return INVALID_MSG; // invalid input
    }
    request->payload = payload;

    return SUCCESS;
}

/**
 *
 * @param filename The filename of config
 * @param inaddr where stores the address
 * @param port  where stores the port
 */
char *read_config(const char *filename, struct in_addr *inaddr, uint16_t *port) {
    FILE *fp = fopen(filename, "rb");
    // get the length of the file
    size_t sz = file_size(fp);

    // read from the file as buffer
    unsigned char *buffer = malloc(sizeof(unsigned char) * sz);
    fread(buffer, sizeof(char), sz, fp);
    fclose(fp);

    // read the address and convert to the text represented address
    char address[16];
    sprintf(address, "%u.%u.%u.%u", buffer[0], buffer[1], buffer[2], buffer[3]);

    // convert the text represented address to Network Byte Order integer
    inet_pton(AF_INET, address, inaddr);

    // read the port_long and convert to the text represented port_long
    char raw_port[6];
    sprintf(raw_port, "%u", ((unsigned int) buffer[4] << 8u) | (unsigned int) buffer[5]);
    // convert to the long integer
    long port_long = strtol(raw_port, NULL, 10);
    // Convert port number to network byte order
    (*port) = htons((uint16_t) port_long);

    // read the directory path
    size_t dir_sz = sz - 6;
    char *dir_path = malloc(sizeof(char) * (dir_sz + 1));
    dir_path[dir_sz] = '\0';
    memcpy(dir_path, buffer + 6, dir_sz);

    free(buffer);
    return dir_path;
}


void echo_handler(const struct data *data, const message *request) {
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
        compress_response(&dict, &response, request->payload, &length, 0x1);
    }
    // Send the response
    send(data->connect_fd, response, sizeof(uint8_t) * length, 0);
    free(response);
}

void directory_list_handler(const struct data *data, const message *request) {
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

    if (request->header->req_compress == 0) {
        uncompressed_response(&response, payload, &length, 0x3);
    } else {
        compress_response(&dict, &response, payload, &length, 0x3);
    }

    send(data->connect_fd, response, sizeof(uint8_t) * length, 0);
    free(response);
    free(file_list);
    free(payload);
}


void file_size_handler(const struct data *data, const message *request, size_t sz) {
    // convert to network byte order
    uint64_t length = 8;
    uint64_t size_64 = htobe64(sz);
    uint8_t *payload = malloc(sizeof(uint8_t) * length);
    uint64_to_uint8(payload, size_64);

    uint8_t *response;
    if (request->header->req_compress == 0) {
        uncompressed_response(&response, payload, &length, 0x5);
    } else {
        compress_response(&dict, &response, payload, &length, 0x5);
    }
    send(data->connect_fd, response, sizeof(uint8_t) * length, 0);
    free(payload);
    free(response);
}

uint8_t retrieve_handler(const struct data *data, const message *request, uint8_t **response, uint64_t *length) {
    uint8_t *request_payload;
    decompress_payload(&dict, request, &request_payload, length);
    // get the information from the file_data: session id; starting offset; data length;
    uint32_t id;
    uint64_t starting;
    uint64_t len_data;
    retrieve_get_info(request_payload, &id, &starting, &len_data);

    // Concatenate the filename
    uint64_t len_filename = *length - RETRIEVE_INFO_LEN;
    char *filename = concatenate_filename(request_payload + RETRIEVE_INFO_LEN, dir_path, len_filename);

    // process request queue
    struct node *node = new_node(filename, id, starting, len_data);
    pthread_mutex_lock(&(queue.mutex));
    uint8_t signal = list_contains(&queue, node);
    if (signal == NON_EXIST) {
        add_node(&queue, node);
    } else if (signal == EXIST) {
        send_empty_retrieve(data->connect_fd);
        pthread_mutex_unlock(&(queue.mutex));
        return 0;
    } else if (signal == SAME_ID_DIFF_OTHER_QUERYING) {
        send_error(data->connect_fd);
        pthread_mutex_unlock(&(queue.mutex));
        return 0;
    } else if (signal == SAME_ID_DIFF_OTHER_QUERYED) {
        add_node(&queue, node);
    }
    pthread_mutex_unlock(&(queue.mutex));
    // end of queue process

    // open and read the file
    FILE *f = fopen(filename, "r");
    free(filename);
    size_t sz;
    if (!f || (starting + len_data) > (sz = file_size(f))) {
        send_error(data->connect_fd);
        return 0;
    }
    char *buffer = malloc(sizeof(char) * sz);
    fread(buffer, sizeof(char), sz, f);
    fclose(f);

    //make the file_data
    uint8_t *file_data = malloc(sizeof(uint8_t) * len_data);
    memcpy(file_data, buffer + starting, len_data);
    free(buffer);
    // Concatenate the payloads
    *length = len_data + RETRIEVE_INFO_LEN;
    uint8_t *uncompressed_payload = malloc(sizeof(uint8_t) * *length);
    memcpy(uncompressed_payload, request_payload, 20);
    memcpy(uncompressed_payload + 20, file_data, len_data);
    free(file_data);
    // make the response

    if (request->header->req_compress == (unsigned) 0) {
        uncompressed_response(response, uncompressed_payload, length, 0x7);
    } else {
        compress_response(&dict, response, uncompressed_payload, length, 0x7);
    }
    node->querying = 0;
    return 1;
}

/**
 *
 * @param arg data where stores the connect_fd and request message
 * @return NULL
 */
void *connection_handler(void *arg) {
    struct data *data = arg;

    while (1) {
        message *request = malloc(sizeof(message));
        data->msg = request;

        // Read the header, payload length and payload
        uint8_t error = read_request(data->connect_fd, request);

        if (error == CON_CLS) {
            // Connection is closed
            close(data->connect_fd);
            break;
        }
        if (error == INVALID_MSG) {
            // Error occurs
            send_error(data->connect_fd);
            break;
        }

        unsigned int type = request->header->type;
        if (type == (unsigned) 0x0) {
            // Echo Functionality
            if (error == LEN_ZERO) {
                send_error(data->connect_fd);
                break;
            }
            echo_handler(data, request);

        } else if (type == (unsigned) 0x2) {
            // Directory list Functionality
            if (error != LEN_ZERO) {
                send_error(data->connect_fd);
                break;
            }
            directory_list_handler(data, request);

        } else if (type == (unsigned) 0x4) {
            // File size query Functionality
            if (error == LEN_ZERO) {
                send_error(data->connect_fd);
                break;
            }

            char *filename = concatenate_filename(request->payload, dir_path, request->length);
            FILE *f = fopen(filename, "r");
            if (!f) {
                send_error(data->connect_fd);
                break;
            }
            size_t sz = file_size(f);
            fclose(f);
            free(filename);

            file_size_handler(data, request, sz);

        } else if (type == (unsigned) 0x6) {
            uint8_t *response;
            uint64_t length;
            if (retrieve_handler(data, request, &response, &length) == 0){
                break;
            }
            send(data->connect_fd, response, sizeof(uint8_t) * length, 0);
            free(response);
        } else if (type == (unsigned) 0x8) {
            shutdown(data->connect_fd, SHUT_RDWR);
            close(data->connect_fd);
            break;

        } else {
            send_error(data->connect_fd);
            break;
        }
        // Clean up
        free(request->header);
        free(request->payload);
        free(request);
    }
    // Clean up
    free(data);
    return NULL;
}


int main(int argc, char **argv) {
    pthread_mutex_init(&(queue.mutex), NULL);
    queue.head = NULL;
    // read the in_address and port from config file
    struct in_addr inaddr;
    uint16_t port;
    dir_path = read_config(argv[1], &inaddr, &port);
    read_dict(&dict);
    // Create socket, and check for error
    // AF_INET = this is an IPv4 socket
    // SOCK_STREAM = this is a TCP socket
    // listenfd is already a file descriptor. It just isn't connected to anything yet.
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        perror("create_listenfd error");
        exit(EXIT_FAILURE);
    }
    // Normally, if we close a socket, there's a TIME_WAIT process. After that, we can reuse the address and port.
    // We can set to allow the address and port to be reused immediately after closing the socket.
    int option = 1; // turn on the options
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &option, sizeof(int));

    // New a sockaddr_in
    struct sockaddr_in address;
    address.sin_family = AF_INET; // Address Family. (IPv4)
    address.sin_addr = inaddr; // Address
    address.sin_port = port; // Port
    // Bind the address and port with the socket
    // This is the destination where the client will require to connect
    if (bind(listenfd, (struct sockaddr *) &address, sizeof(address)) < 0) {
        perror("bind_listenfd error");
        exit(EXIT_FAILURE);
    }

    // Wait the client to connect
    // SOMAXCONN = The max number of outstanding connections in the socket's listen queue. Normally 128.
    if (listen(listenfd, SOMAXCONN) < 0) {
        perror("listen_listenfd error");
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Accept the connection request from the client
        // It will block until there is any connection
        int connect_fd;
        socklen_t address_len = sizeof(address);
        if ((connect_fd = accept(listenfd, (struct sockaddr *) &address, &address_len)) < 0) {
            perror("accept_listenfd error");
            exit(EXIT_FAILURE);
        }

        // data stores the connect_fd and request
        struct data *data = malloc(sizeof(struct data));
        data->connect_fd = connect_fd;
        // Create a thread for every new connect to process the request
        pthread_t thread;
        pthread_create(&thread, NULL, connection_handler, data);
    }
    destroy_linked_list(&queue);
    pthread_mutex_destroy(&(queue.mutex));
    free(dir_path);
    close(listenfd);
    return 0;
}
