#ifndef NECROFILE_TCP_H
#define NECROFILE_TCP_H

#include "php_necrofile.h"

#define EPOLL_MAX_EVENTS 64

void *tcp_server(void *arg);
int create_tcp_server_socket(void);
void handle_client_connection(int client_fd);

extern pthread_t tcp_server_thread;
extern atomic_int tcp_server_running;
extern int tcp_server_fd;

#endif /* NECROFILE_TCP_H */