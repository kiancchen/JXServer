#include "util.h"

struct dict dict;
char *dir_path;
struct linked_list queue;

/**
 *
 * @param connect_fd Connection file description
 * @param request where to stores the request
 * @return 0 for connection closed; 1 for invalid input; 2 for success
 */
int read_request(int connect_fd, message *request) {
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

void echo_handler(const struct data *data, const message *request) {
    uint8_t *response;
    uint64_t length;
    if (request->header->compressed != (unsigned) 0 || request->header->req_compress != (unsigned) 1) {
        length = HEADER_LENGTH + request->length;
        response = malloc(sizeof(uint8_t) * length);
        // Copy the request
        msg_to_response(request, response);
        // Modify the header
        response[0] = make_header(0x1, request->header->compressed, 0);
    } else {
        length = get_code_length(&dict, request->payload, request->length);
        length = upper_divide(length, 8) + 1; // add the bytes of padding length

        response = malloc(sizeof(uint8_t) * (HEADER_LENGTH + length));
        response[0] = make_header(0x1, 1, 0);
        uint64_to_uint8(response + 1, htobe64(length));
        uint8_t *compressed = compress(&dict, request->payload, request->length);
        memcpy(response + 9, compressed, length);

        length += HEADER_LENGTH;
        free(compressed);
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
        // if compression not required
        response = malloc(sizeof(uint8_t) * (length + HEADER_LENGTH));
        response[0] = make_header(0x3, 0, 0);
        // fill the payload length bytes
        uint64_to_uint8(response + 1, htobe64(length));
//        payload_len_to_uint8(length, response);
        // fill the payload as file list
        memcpy(response + 9, payload, length);
        length += HEADER_LENGTH;
    } else {

        // get the compressed length of payload
        uint64_t compressed_length = get_code_length(&dict, payload, length);
        compressed_length = upper_divide(compressed_length, 8) + 1;
        // make the response
        response = malloc(sizeof(uint8_t) * (HEADER_LENGTH + compressed_length));
        response[0] = make_header(0x3, 1, 0);
        // fill the payload length bytes
        uint64_to_uint8(response + 1, htobe64(compressed_length));
        // get the compressed payload and copy to the response
        uint8_t *compressed = compress(&dict, payload, length);
        memcpy(response + 9, compressed, compressed_length);
        // the final length of the whole response
        length = compressed_length + HEADER_LENGTH;
        free(compressed);
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
        response = malloc(sizeof(uint8_t) * (HEADER_LENGTH + length));
        response[0] = make_header(0x5, 0, 0);
        uint64_to_uint8(response + 1, htobe64(length));
        memcpy(response + 9, payload, length);
        length += HEADER_LENGTH;
    } else {
        uint64_t compressed_length = get_code_length(&dict, payload, 8);
        compressed_length = upper_divide(compressed_length, 8) + 1;
        // make the response
        response = malloc(sizeof(uint8_t) * (HEADER_LENGTH + compressed_length));
        response[0] = make_header(0x5, 1, 0);
        // fill the payload length bytes
        uint64_to_uint8(response + 1, htobe64(compressed_length));
        // get the compressed payload and copy to the response
        uint8_t *compressed = compress(&dict, payload, length);
        memcpy(response + 9, compressed, compressed_length);
        // the final length of the whole response
        length = compressed_length + HEADER_LENGTH;
        free(compressed);
    }
    send(data->connect_fd, response, sizeof(uint8_t) * length, 0);
    free(payload);
    free(response);
}

char *concatenate_filename(uint8_t *payload, uint64_t length) {
    char *filename = malloc(sizeof(char) * (strlen(dir_path) + length + 2));
    filename[strlen(dir_path) + length + 1] = '\0';
    memcpy(filename, dir_path, strlen(dir_path));
    filename[strlen(dir_path)] = '/';
    memcpy(filename + strlen(dir_path) + 1, payload, length);
    return filename;
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
        int error = read_request(data->connect_fd, request);

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
            if (error != LEN_ZERO) {
                send_error(data->connect_fd);
                break;
            }
            directory_list_handler(data, request);

        } else if (type == (unsigned) 0x4) {
            if (error == LEN_ZERO) {
                send_error(data->connect_fd);
                break;
            }

            char *filename = concatenate_filename(request->payload, request->length);
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
            uint8_t *request_payload;
            uint64_t length;
            if (request->header->compressed == 0) {
                request_payload = request->payload;
                length = request->length;
            } else {
                request_payload = decompress(&dict, request->payload, request->length, &length);
            }


            uint32_t id[1];
            memcpy(id, request_payload, 4);
            *id = htobe32(*id);
//            printf("id: %u\n", *id);

            uint64_t starting[1];
            memcpy(starting, request_payload + 4, 8);
            *starting = htobe64(*starting);
//            printf("Starting: %lu\n", *starting);

            uint64_t len_data[1];
            memcpy(len_data, request_payload + 12, 8);
            *len_data = htobe64(*len_data);
//            printf("len data: %lu\n", *len_data);

            // Concatenate the filename
            uint64_t len_filename = length - 20;
            char *filename = concatenate_filename(request_payload, len_filename);
//            printf("Filename: %s\n", filename);

            // process request queue
            struct node *node = new_node(filename, *id, *starting, *len_data);
            pthread_mutex_lock(&(queue.mutex));
            uint8_t signal = list_contains(&queue, node);
            if (signal == NON_EXIST) {
                add_node(&queue, node);
            } else if (signal == EXIST) {
                send_empty_retrieve(data->connect_fd);
                pthread_mutex_unlock(&(queue.mutex));
                break;
            } else if (signal == SAME_ID_DIFF_OTHER_QUERYING) {
                send_error(data->connect_fd);
                pthread_mutex_unlock(&(queue.mutex));
                break;
            } else if (signal == SAME_ID_DIFF_OTHER_QUERYED) {
                add_node(&queue, node);
            }
            pthread_mutex_unlock(&(queue.mutex));


            FILE *f = fopen(filename, "r");
            free(filename);
            if (!f) {
                send_error(data->connect_fd);
                break;
            }
            size_t sz = file_size(f);
            if ((*starting + *len_data) > sz) {

                send_error(data->connect_fd);
                break;
            }
            char *buffer = malloc(sizeof(char) * sz);
            fread(buffer, sizeof(char), sz, f);
            fclose(f);
            //make the payload

            uint8_t *payload = malloc(sizeof(uint8_t) * *len_data);
            memcpy(payload, buffer + *starting, *len_data);
            // make the response
            uint8_t *response;

            if (request->header->req_compress == 0) {
                length = *len_data + 20;
                // make the response
                response = malloc(sizeof(uint8_t) * (HEADER_LENGTH + length));
                response[0] = make_header(0x7, 0, 0);
                // make the payload length
                uint64_to_uint8(response + 1, htobe64(length));
                // fill the file info
                memcpy(response + 9, request_payload, 20);
                // fill the file data
                memcpy(response + 29, payload, *len_data);

                length += HEADER_LENGTH;
            } else {
                length = 20 + *len_data;
                // Concatenate the payloads
                uint8_t *uncompressed_payload = malloc(sizeof(uint8_t) * length);
                memcpy(uncompressed_payload, request_payload, 20);
                memcpy(uncompressed_payload + 20, payload, *len_data);

                // Get the length of compressed payload
                uint64_t compressed_length = get_code_length(&dict, uncompressed_payload, length);
                compressed_length = upper_divide(compressed_length, 8) + 1;
                // make the response
                response = malloc(sizeof(uint8_t) * (HEADER_LENGTH + compressed_length));
                response[0] = make_header(0x7, 1, 0);
                // fill the payload length bytes
                uint64_to_uint8(response + 1, htobe64(compressed_length));
                // get the compressed payload and copy to the response
                uint8_t *compressed = compress(&dict, uncompressed_payload, length);
                memcpy(response + 9, compressed, compressed_length);


                // the final length of the whole response
                length = compressed_length + HEADER_LENGTH;
                free(uncompressed_payload);
                free(compressed);
            }
            node->querying = 0;
            send(data->connect_fd, response, sizeof(uint8_t) * length, 0);
            free(buffer);
            free(payload);
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
    pthread_mutex_destroy(&(queue.mutex));
    free(dir_path);
    close(listenfd);
    return 0;
}
