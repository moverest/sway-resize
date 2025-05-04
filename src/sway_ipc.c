#include "sway_ipc.h"

#include "log.h"

#include <jansson.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

struct sway_ipc_msg_header {
    char     magic[6];
    uint32_t length;
    uint32_t type;
} __attribute__((__packed__));

int sway_ipc_open_socket() {
    char *socket_path = getenv("SWAYSOCK");
    if (socket_path == NULL) {
        LOG_ERR("$SWAYSOCK env is not defined. Can't find socket.");
        return -1;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        LOG_ERR("Unable to create UNIX socket.");
        return -2;
    }

    struct sockaddr_un addr = {
        .sun_family = AF_UNIX,
    };

    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = 0;

    if (connect(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) ==
        -1) {
        LOG_ERR("Unable to connect to '%s'.", socket_path);
        return -3;
    }

    return fd;
}

int sway_ipc_send(
    int fd, enum sway_ipc_msg_type type, char *payload, uint32_t len
) {
    struct sway_ipc_msg_header header = {
        .magic  = "i3-ipc",
        .length = len,
        .type   = type,
    };

    if (write(fd, &header, sizeof(header)) < 0) {
        LOG_ERR("Could not send header.");
        return 0;
    }

    if (write(fd, payload, len) < 0) {
        LOG_ERR("Could not send payload.");
        return 0;
    }

    return 1;
}

struct sway_ipc_msg *sway_ipc_recv(int fd) {
    struct sway_ipc_msg_header header;

    char  *buf   = (char *)&header;
    size_t buf_i = 0;
    while (buf_i < sizeof(header)) {
        ssize_t received = recv(fd, buf + buf_i, sizeof(header) - buf_i, 0);
        if (received < 0) {
            LOG_ERR("Could not recieve header.");
            return NULL;
        }

        buf_i += received;
    }

    struct sway_ipc_msg *msg =
        malloc(sizeof(struct sway_ipc_msg) + header.length + 1);
    msg->length = header.length;
    msg->type   = header.type;

    if (msg == NULL) {
        LOG_ERR("Could not allocate message buffer.");
        return NULL;
    }

    buf_i = 0;
    while (buf_i < header.length) {
        ssize_t received =
            recv(fd, msg->payload + buf_i, header.length - buf_i, 0);
        if (received < 0) {
            LOG_ERR("Could not receive payload.");
            return NULL;
        }

        buf_i += received;
    }

    // If the payload is a string, we want to make sure it is null terminated.
    msg->payload[buf_i] = 0;

    return msg;
}
