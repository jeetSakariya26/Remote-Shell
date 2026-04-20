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
#include <arpa/inet.h>
#include <bits/stdc++.h>
using namespace std;

#define PORT 9090
#define MAX_EVENTS 2
#define BUFFER_SIZE 1024

static struct termios saved_termios;
void enter_raw_mode() {
    tcgetattr(STDIN_FILENO, &saved_termios);
    struct termios raw = saved_termios;

    // cfmakeraw() sets: ~ECHO ~ICANON ~ISIG ~IEXTEN ~IXON ~ICRNL
    //                   ~OPOST  CS8  VMIN=1 VTIME=0
    cfmakeraw(&raw);

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    std::cerr << "[client] Entered raw mode. Press Ctrl-Q to quit.\n";
}

void restore_terminal() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_termios);
    std::cerr << "\n[client] Terminal restored.\n";
}
void add_sock_to_epoll(int epoll_fd, int sock_fd){
    struct epoll_event ev;
    ev.events=EPOLLIN;
    ev.data.fd=sock_fd;
    epoll_ctl(epoll_fd,EPOLL_CTL_ADD,sock_fd,&ev);
}

int main(){
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("connect");
        exit(1);
    }

    enter_raw_mode();

    int epoll_fd=epoll_create1(0);
    struct epoll_event ev[MAX_EVENTS];  
    
    add_sock_to_epoll(epoll_fd, STDIN_FILENO);
    add_sock_to_epoll(epoll_fd, sock);
    
    bool status=true;

    while(status){
        int num_events = epoll_wait(epoll_fd, ev, MAX_EVENTS, -1);
        for (int i = 0; i < num_events; i++) {
            int fd=ev[i].data.fd;
        
            // checking fd is socket or stdin
            if (fd == sock) {
                char buffer[BUFFER_SIZE];
                int n=read(fd, buffer, sizeof(buffer));

                if(n <= 0){
                    status=false;
                    break;
                }

                write(STDOUT_FILENO, buffer, n);

            } else if (fd == STDIN_FILENO) {
                char buffer[BUFFER_SIZE];
                int n=read(fd, buffer, sizeof(buffer));

                if(n <= 0){
                    status=false;
                    break;
                }

                for(int j = 0; j < n; j++) {
                    if (buffer[j] == '\x11') {
                        status=false;
                        break;
                    }
                }

                write(sock, buffer, n);
            }
        }
    }

    restore_terminal();
    close(sock);
    close(epoll_fd);

    return 0;
}