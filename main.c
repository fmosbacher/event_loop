#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <unistd.h>

#define LOG(msg, ...)                                                          \
    printf("[LOG] %s %d: " msg "\n", __FILE__, __LINE__, ##__VA_ARGS__)

typedef struct task {
    int epoll;
    int socket;
    void (*run)(struct task *self);
    void *data;
} Task;

typedef struct {
    int client;
    char *buffer;
} EchoContext;

void echo(Task *self) {
    EchoContext *cx = self->data;
    assert(send(cx->client, cx->buffer, strlen(cx->buffer), 0) > -1);
    LOG("Echoed back to client");
}

void handle_client(Task *self) {
    int client = self->socket;

    int buf_len = 128;
    char *buffer = malloc(buf_len);

    int bytes_read = recv(client, buffer, buf_len - 1, 0);
    assert(bytes_read > 0);

    buffer[bytes_read] = '\0';

    LOG("Received msg - %s", buffer);

    int timer = timerfd_create(0, 0);
    assert(timer > -1);

    struct itimerspec timer_spec = {0};

    timer_spec.it_value.tv_sec = 3;

    assert(timerfd_settime(timer, 0, &timer_spec, NULL) > -1);

    EchoContext *cx = malloc(sizeof(EchoContext));

    cx->client = client;
    cx->buffer = buffer;

    Task *task = malloc(sizeof(Task));

    task->epoll = self->epoll;
    task->socket = timer;
    task->data = cx;
    task->run = echo;

    struct epoll_event *event = malloc(sizeof(struct epoll_event));

    event->events = EPOLLIN | EPOLLET;
    event->data.ptr = task;

    epoll_ctl(self->epoll, EPOLL_CTL_ADD, timer, event);
}

void accept_client(Task *self) {
    int server = self->socket;

    struct sockaddr_in *addr = malloc(sizeof(struct sockaddr_in));
    socklen_t *sock_len = malloc(sizeof(sock_len));

    int client = accept(server, (struct sockaddr *)addr, sock_len);
    assert(client > -1);

    LOG("Client connected");

    Task *task = malloc(sizeof(Task));

    task->epoll = self->epoll;
    task->socket = client;
    task->data = NULL;
    task->run = handle_client;

    struct epoll_event *event = malloc(sizeof(struct epoll_event));

    event->events = EPOLLIN | EPOLLET;
    event->data.ptr = task;

    epoll_ctl(self->epoll, EPOLL_CTL_ADD, client, event);
}

int main(void) {
    // TODO: close, free and error handling

    int epoll = epoll_create1(0);
    assert(epoll > -1);

    int server = socket(AF_INET, SOCK_STREAM, 0);
    assert(server > -1);

    struct sockaddr_in *addr = malloc(sizeof(struct sockaddr_in));
    assert(addr != NULL);

    int port = 8080;

    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = INADDR_ANY;
    addr->sin_port = htons(port);

    assert(bind(server, (struct sockaddr *)addr, sizeof(struct sockaddr_in)) >
           -1);

    assert(listen(server, SOMAXCONN) > -1);

    LOG("Listening on port %d...", port);

    Task *task = malloc(sizeof(Task));

    task->epoll = epoll;
    task->socket = server;
    task->data = NULL;
    task->run = accept_client;

    struct epoll_event *event = malloc(sizeof(struct epoll_event));

    event->events = EPOLLIN | EPOLLET;
    event->data.ptr = task;

    epoll_ctl(epoll, EPOLL_CTL_ADD, server, event);

    struct epoll_event incoming_events[SOMAXCONN];

    while (true) {
        int n_events = epoll_wait(epoll, incoming_events, SOMAXCONN, -1);

        for (int i = 0; i < n_events; i++) {
            Task *task = incoming_events[i].data.ptr;
            task->run(task);
        }
    }
}