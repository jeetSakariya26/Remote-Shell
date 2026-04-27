#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pty.h>        
#include <sys/epoll.h>
#include <unordered_map>
#include <cstring>
#include <fcntl.h>
#include <sys/wait.h>
#include <bits/stdc++.h>
#include <arpa/inet.h>
#include "helper.h"
using namespace std;


struct termios orignal_terminal;
void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void enter_raw_mode() {
    tcgetattr(STDIN_FILENO, &orignal_terminal); // save original

    struct termios raw = orignal_terminal;

    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0; 
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void close_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orignal_terminal);
}

void add_sock_to_epoll(int epoll_fd, int sock_fd){
    struct epoll_event ev;
    ev.events=EPOLLIN;
    ev.data.fd=sock_fd;
    epoll_ctl(epoll_fd,EPOLL_CTL_ADD,sock_fd,&ev);
}
