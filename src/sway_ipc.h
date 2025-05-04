#ifndef __SWAY_IPC_H_INCLUDED__
#define __SWAY_IPC_H_INCLUDED__

#include <stdint.h>

struct sway_ipc_msg {
    uint32_t length;
    uint32_t type;
    char     payload[];
};

enum sway_ipc_msg_type {
    SWAY_MSG_RUN_COMMAND = 0,
    SWAY_MSG_GET_TREE    = 4,
};

int sway_ipc_open_socket();
int sway_ipc_send(
    int fd, enum sway_ipc_msg_type type, char *payload, uint32_t len
);
struct sway_ipc_msg *sway_ipc_recv(int fd);

#endif
