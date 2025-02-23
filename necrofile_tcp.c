#include "necrofile_tcp.h"
#include "necrofile_redis.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include "zend_smart_str.h"

pthread_t tcp_server_thread;
atomic_int tcp_server_running;
int tcp_server_fd = -1;

int create_tcp_server_socket(void)
{
    int tcp_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_server_fd < 0) {
        php_error_docref(NULL, E_WARNING, "Failed to create TCP server socket");
        return -1;
    }

    int reuse = 1;
    if (setsockopt(tcp_server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        php_error_docref(NULL, E_WARNING, "Failed to set SO_REUSEADDR option");
        close(tcp_server_fd);
        return -1;
    }

    return tcp_server_fd;
}

int bind_tcp_server_socket(int tcp_server_fd)
{
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(NECROFILE_G(server_address));
    server_addr.sin_port = htons(NECROFILE_G(tcp_port));

    if (bind(tcp_server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        php_error_docref(NULL, E_WARNING, "Failed to bind TCP server socket");
        close(tcp_server_fd);
        return -1;
    }

    return 0;
}

int create_epoll_instance(void)
{
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        php_error_docref(NULL, E_WARNING, "Failed to create epoll instance");
        return -1;
    }

    return epoll_fd;
}

int add_socket_to_epoll(int epoll_fd, int tcp_server_fd)
{
    struct epoll_event ev = {0};
    ev.events = EPOLLIN;
    ev.data.fd = tcp_server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tcp_server_fd, &ev) < 0) {
        php_error_docref(NULL, E_WARNING, "Failed to add server socket to epoll");
        close(epoll_fd);
        return -1;
    }

    return 0;
}

void handle_client_connection(int client_fd)
{
    redisReply *reply = redisCommand(redis_context, "SMEMBERS %s", REDIS_SET_KEY);

    if (!reply) {
        const char *response = "[]";
        ssize_t sent = write(client_fd, response, strlen(response));
        if (sent < 0) {
            php_error_docref(NULL, E_WARNING, "Failed to send response to client");
        }
        close(client_fd);
        return;
    }

    smart_str json = {0};
    smart_str_appendc(&json, '[');

    for (size_t i = 0; i < reply->elements; i++) {
        if (i > 0) {
            smart_str_appendl(&json, ",\n", 2);
        }
        smart_str_appendc(&json, '"');
        smart_str_appendl(&json, reply->element[i]->str, reply->element[i]->len);
        smart_str_appendc(&json, '"');
    }

    smart_str_appendc(&json, ']');
    smart_str_0(&json);

    size_t total_sent = 0;
    while (total_sent < json.s->len) {
        ssize_t sent = write(client_fd, json.s->val + total_sent, json.s->len - total_sent);
        if (sent < 0) {
            if (errno == EINTR) continue;
            break;
        }
        total_sent += sent;
    }

    smart_str_free(&json);
    freeReplyObject(reply);
    close(client_fd);
}

void *tcp_server(void *arg)
{
    int tcp_server_fd = create_tcp_server_socket();
    if (tcp_server_fd < 0) return NULL;

    if (bind_tcp_server_socket(tcp_server_fd) < 0) return NULL;

    if (listen(tcp_server_fd, SOMAXCONN) < 0) {
        php_error_docref(NULL, E_WARNING, "Failed to listen on TCP server socket");
        close(tcp_server_fd);
        return NULL;
    }

    int epoll_fd = create_epoll_instance();
    if (epoll_fd < 0) return NULL;

    if (add_socket_to_epoll(epoll_fd, tcp_server_fd) < 0) return NULL;

    struct epoll_event events[EPOLL_MAX_EVENTS];
    while (ATOMIC_LOAD(&tcp_server_running)) {
        int nfds = epoll_wait(epoll_fd, events, EPOLL_MAX_EVENTS, 1000); // 1s timeout
        if (nfds < 0) {
            if (errno == EINTR) continue;
            php_error_docref(NULL, E_WARNING, "epoll_wait failed");
            break;
        }

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == tcp_server_fd) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(tcp_server_fd, (struct sockaddr *)&client_addr, &client_len);
                if (client_fd < 0) {
                    php_error_docref(NULL, E_WARNING, "Failed to accept TCP client connection");
                    continue;
                }

                handle_client_connection(client_fd);
            }
        }
    }

    close(epoll_fd);
    close(tcp_server_fd);
    return NULL;
}