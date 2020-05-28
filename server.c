#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <pthread.h>

#define DEBUG 0
#define CON_CLS 0
#define INVALID_MSG 1
#define SUCCESS 2
#define LEN_ZERO 3

struct header {
    unsigned type: 4;
    unsigned compression: 1;
    unsigned req_compre: 1;
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
    if (DEBUG){
        for (int i = 0; i < 9; ++i) {
            printf("%hhx ", buffer[i]);
        }
    }
    puts("\n");
    if (num <= 0) {
        if (DEBUG) puts("1");
        return CON_CLS; // Connection closed
    }
    if (num != 9) {
        if (DEBUG) puts("2");
        return INVALID_MSG; // invalid input
    }

    // Convert first type and stores in the struct header
    struct header *hd = malloc(sizeof(struct header));
    hd->type = buffer[0] >> 4u;
    hd->compression = buffer[0] >> 3u;
    hd->req_compre = buffer[0] >> 2u;

    // Read the length
    uint64_t length = buffer[8];
    for (int i = 1; i < 8; ++i) {
        length = ((unsigned) buffer[i] << ((8u - i) * 8u)) | length;
    }
    if (length == 0) {
        if (DEBUG) puts("3");
        return CON_CLS; // invalid input
    }

    // Read the payload
    uint8_t *payload = malloc(sizeof(uint8_t) * length);
    num = recv(connect_fd, payload, length, 0);
    if (num <= 0) {
        if (DEBUG) puts("4");
        return CON_CLS; //Connection closed
    }
    if (num != length) {
        if (DEBUG) puts("5");
        return INVALID_MSG; // invalid input
    }

    // Stores above received information
    request->header = hd;
    request->length = length;
    request->payload = payload;
    return SUCCESS;
}

/**
 *
 * @param filename The filename of config
 * @param inaddr where stores the address
 * @param port  where stores the port
 */
void read_command(char *filename, struct in_addr *inaddr, uint16_t *port) {
    FILE *fp = fopen(filename, "rb");
    // get the length of the file
    fseek(fp, 0, SEEK_END);
    size_t sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // read from the file
    unsigned char *buffer = malloc(sizeof(unsigned char) * sz);
    fread(buffer, sizeof(buffer), 1, fp);
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
    free(buffer);
}

/**
 *
 * @param msg Source to be converted
 * @param response The list of uint8_t where stores the converted message
 */
void msg_to_response(message *msg, uint8_t *response) {
    struct header *header = msg->header;
    // Convert the header to one byte of uint8_t
    response[0] = header->type << 4 | header->compression << 3 | header->req_compre << 2;

    // Convert the length of uint64_t to uint8_t[8] and copy to the response
    uint8_t length[8];
    memcpy(length, &msg->length, sizeof(msg->length));
    for (int i = 1; i < 9; i++) {
        response[i] = length[8 - i];
    }
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
    uint8_t error_header = (unsigned) 0xf << 4u;
    uint64_t error_payload = 0x0;
    send(connect_fd, &error_header, sizeof(uint8_t), 0);
    send(connect_fd, &error_payload, sizeof(uint64_t), 0);
    close(connect_fd);
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
            if (DEBUG) puts("6");
            // Connection is closed
            close(data->connect_fd);
            break;
        }
        if (error == INVALID_MSG) {
            if (DEBUG) puts("7");
            // Error occurs
            send_error(data->connect_fd);
            break;
        }

        if (request->header->type == (unsigned) 0x0) {
            // Echo Functionality
            if (error == LEN_ZERO) {
                send_error(data->connect_fd);
                break;
            }
            uint8_t *response = malloc(sizeof(uint8_t) * (9 + request->length));
            // Copy the request
            msg_to_response(request, response);
            // Modify the header
            response[0] = 0x1 << 4 | request->header->compression << 3 | request->header->req_compre << 2;
            // Send the response
            send(data->connect_fd, response, sizeof(uint8_t) * (9 + request->length), 0);
            free(response);
        } else if (request->header->type == (unsigned) 0x0 && error == LEN_ZERO) {
            shutdown(data->connect_fd, SHUT_RDWR);
            close(data->connect_fd);

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
    // read the in_address and port from config file
    struct in_addr inaddr;
    uint16_t port;
    read_command(argv[1], &inaddr, &port);

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
    close(listenfd);
    return 0;
}