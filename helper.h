#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pty.h>        
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <bits/stdc++.h>
#include <arpa/inet.h>
using namespace std;


// raw mode closing and entering
extern struct termios orignal_terminal;
void enter_raw_mode();
void close_raw_mode();
void set_nonblocking(int fd);


// epoll functions
void add_sock_to_epoll(int epoll_fd, int sock);

