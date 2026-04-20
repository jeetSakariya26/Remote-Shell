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
using namespace std;
namespace fs = std::filesystem;

#define PORT 9090
#define MAX_EVENTS 10

class Socket{
    public:
        int master_fd;
        int sock_fd;
};

class Packet{
    public:
        int type;
        string token;
        string data;
};

unordered_map<int,int> clients_to_ptyMaster, ptyMaster_to_clients;

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
void cleanup_connection(int epoll_fd, int active_fd) {
    int client_fd = -1;
    int pty_fd = -1;

    if (clients_to_ptyMaster.count(active_fd)) {
        client_fd = active_fd;
        pty_fd = clients_to_ptyMaster[active_fd];
    } else if (ptyMaster_to_clients.count(active_fd)) {
        pty_fd = active_fd;
        client_fd = ptyMaster_to_clients[active_fd];
    }

    if (client_fd != -1 && pty_fd != -1) {
        std::cout << "Cleaning up connection. Client FD: " << client_fd << ", PTY FD: " << pty_fd << std::endl;
        
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, pty_fd, nullptr);
        
        close(client_fd);
        close(pty_fd);
        
        clients_to_ptyMaster.erase(client_fd);
        ptyMaster_to_clients.erase(pty_fd);

        // Reap any zombie bash processes
        waitpid(-1, nullptr, WNOHANG);
    }
}

int create_pty(int client_sock) {
    int master, slave;
    char slave_name[256];

    if (openpty(&master, &slave, slave_name, nullptr, nullptr) < 0) {
        perror("openpty");
        return -1;
    }
    
    pid_t child = fork();
    if (child < 0) { perror("fork"); return -1; }

    if (child == 0) {
        // slave does not need the master
        close(master);

        // new session (detach from parent's tty)
        setsid();
        if (ioctl(slave, TIOCSCTTY, 0) < 0) {
            perror("TIOCSCTTY");
        }

        // make slave the controlling terminal of this session
        if (tcsetpgrp(slave, getpid()) < 0) {
            perror("tcsetpgrp");
            return -1;
        }

        // slave is now the controlling terminal
        dup2(slave, 0);  // stdin
        dup2(slave, 1);  // stdout
        dup2(slave, 2);  // stderr

        char addr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_sock, addr, sizeof(addr));
        string client_addr(addr);
        if(!fs::exists(client_addr)){
            fs::create_directory(client_addr);
        }
        fs::current_path(client_addr);

        // start the shell
        execlp("/bin/bash", "bash", "--norc", "--noprofile", nullptr);
    }
    // master does not need the slave
    close(slave);
    set_nonblocking(master);
    return master;
}

void add_sock_to_epoll(int epoll_fd, int sock_fd){
    struct epoll_event ev;
    ev.events=EPOLLIN;
    ev.data.fd=sock_fd;
    epoll_ctl(epoll_fd,EPOLL_CTL_ADD,sock_fd,&ev);
}


bool command_is_valid(string command){
    
}

int main(){
    int sock;
    sockaddr_in server_addr;
    memset(&server_addr,0,sizeof(server_addr));

    sock = socket(AF_INET,SOCK_STREAM,0);
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));


    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if(bind(sock,(sockaddr*)&server_addr,sizeof(server_addr)) < 0){
        perror("bind");
        exit(1);
    }

    if(listen(sock,5) < 0){
        perror("listen");
        exit(1);
    }

    int epoll_fd = epoll_create1(0);
    struct epoll_event ev[MAX_EVENTS];

    add_sock_to_epoll(epoll_fd, sock);

    while(true){
        int n = epoll_wait(epoll_fd,ev,MAX_EVENTS,-1);
        for(int i=0;i<n;i++){
            int fd=ev[i].data.fd;
            if(fd == sock){
                int client_sock = accept(sock,nullptr,nullptr);
                set_nonblocking(client_sock);
                int ptyMaster = create_pty(client_sock);


                if (ptyMaster < 0) {          
                    close(client_sock); 
                    continue; 
                }


                clients_to_ptyMaster[client_sock] = ptyMaster;
                ptyMaster_to_clients[ptyMaster] = client_sock;
                add_sock_to_epoll(epoll_fd, client_sock);
                add_sock_to_epoll(epoll_fd, ptyMaster);

            }else if(clients_to_ptyMaster.find(fd) != clients_to_ptyMaster.end()){
                int ptyMaster = clients_to_ptyMaster[fd];
                char buf[1024];
                int n=read(fd,buf,sizeof(buf));
                if(n <= 0){
                    cleanup_connection(epoll_fd,fd);
                    continue;
                }
                write(ptyMaster,buf,n);
            }else if(ptyMaster_to_clients.find(fd) != ptyMaster_to_clients.end()){
                int client_sock = ptyMaster_to_clients[fd];
                char buf[1024];
                int n=read(fd,buf,sizeof(buf));
                if(n <= 0){
                    cleanup_connection(epoll_fd,fd);
                    continue;
                }
                write(client_sock,buf,n);
            }
        }
    }

    return 0;
}