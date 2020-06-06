#include "func_handler.h"

struct dict dict;
char *dir_path;
struct linked_list queue;

void free_request(message *request){
    free(request->header);
    free(request->payload);
    free(request);
}

/**
 * Read the request from the client.
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
 * Read the configuration file.
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
 * Each request will be handled by a thread with this function.
 * @param arg data where stores the connect_fd and request message
 * @return NULL
 */
void *connection_handler(void *arg) {
    struct data *data = arg;

    while (1) {
        message *request = malloc(sizeof(message));


        // Read the header, payload length and payload
        uint8_t error = read_request(data->connect_fd, request);

        if (error == CON_CLS) {
            // Connection is closed
            free_request(request);
            close(data->connect_fd);
            break;
        }
        if (error == INVALID_MSG) {
            // Error occurs
            free_request(request);
            send_error(data->connect_fd);
            break;
        }

        unsigned int type = request->header->type;
        if (type == (unsigned) 0x0) {
            // Echo Functionality
            if (error == LEN_ZERO) {
                free_request(request);
                send_error(data->connect_fd);
                break;
            }
            echo_handler(data, &dict, request);

            free_request(request);
        } else if (type == (unsigned) 0x2) {
            // Directory list Functionality
            if (error != LEN_ZERO) {
                send_error(data->connect_fd);
                break;
            }
            directory_list_handler(data, &dict, dir_path, request);

        } else if (type == (unsigned) 0x4) {
            // File size query Functionality
            if (error == LEN_ZERO) {
                send_error(data->connect_fd);
                break;
            }
            if (file_size_handler(data, &dict, dir_path, request) == ERROR_OCCUR) {
                // Error occurs
                send_error(data->connect_fd);
                break;
            }

        } else if (type == (unsigned) 0x6) {
            if (retrieve_handler(data, &dict, dir_path, &queue, request) == ERROR_OCCUR) {
                // Error occurs
                break;
            }

        } else if (type == (unsigned) 0x8) {
            free_request(request);
            shutdown(data->connect_fd, SHUT_RDWR);
            close(data->connect_fd);
            exit(0);
            break;

        } else {
            send_error(data->connect_fd);
            break;
        }
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
