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
#include <filesystem>
#include <arpa/inet.h>
#include <string>
#include "helper.h"
#include "packet.h"
#include <bits/stdc++.h>
using namespace std;
namespace fs = filesystem;

#define PORT 9090
#define BUFFER_SIZE 4096
#define MAX_EVENTS 20



class User{
    public:
    string username;
    string ip_addr;
    int port;
    int client_sock;
    int ptyMaster;
    string token;
    string buffer;
};

// unordered_map<int, shared_ptr<User>> client_to_user;
// unordered_map<int, shared_ptr<User>> ptyMaster_to_user;
// auto user = make_shared<User>();
unordered_map<int, User> client_to_user, ptyMaster_to_user;


void cleanup_connection(User user,int epoll_fd) {
    int client_fd = user.client_sock;
    int pty_fd = user.ptyMaster;

    client_to_user.erase(client_fd);
    ptyMaster_to_user.erase(pty_fd);

    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
    close(client_fd);
    if (pty_fd >= 0) {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, pty_fd, nullptr);
        close(pty_fd);
    }
    waitpid(-1, nullptr, WNOHANG);
    cout << "[server] cleaned up client_fd=" << client_fd << "\n";
}


int create_pty(string &work_dir) {
    int master, slave;
    struct winsize ws;
    ws.ws_col = 80;
    ws.ws_row = 24;
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;

    // Pass the winsize to openpty
    if (openpty(&master, &slave, nullptr, nullptr, &ws) < 0) {
        perror("openpty"); return -1;
    }
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); close(master); close(slave); return -1; }
    if (pid == 0) {
        close(master);
        setsid();
        ioctl(slave, TIOCSCTTY, 0);
        dup2(slave, STDIN_FILENO);
        dup2(slave, STDOUT_FILENO);
        dup2(slave, STDERR_FILENO);
        if (slave > STDERR_FILENO) close(slave);
        if (!work_dir.empty()) {
            fs::create_directories(work_dir);
            fs::current_path(work_dir);
        }
        execlp("/bin/bash", "bash", "--norc", "--noprofile", nullptr);
        _exit(1);
    }
    close(slave);
    set_nonblocking(master);
    return master;
}

string gen_random_token(){
    string token = "";
    for(int i=0;i<6;i++){
        token += rand() % 10 + '0';
    }
    return token;
}

string gen_random_key(){
    string key = "";
    for(int i=0;i<6;i++){
        key += (rand() % 10 + '0');
    }
    return key;
}


bool authorize_user(int client_sock, string key,User &user){
    char response[BUFFER_SIZE];
    int n=recv(client_sock,response,BUFFER_SIZE,0);
    
    if( n <= 0){
        return false;
    }
    string res(response,n);
    auto [type, payload] = decode_message(res);
    if(type == 0){
        auto [private_key,username] = decode_login(payload);
        if(key != private_key){
            return false;
        }
        user.username = username;
        user.token = gen_random_token();
        string message = message_of_ack(user.token);
        send(client_sock,message.c_str(),message.size(),0);
        return true;
    }
    return false;
}

pair<int,string> get_ip_and_port(int client_sock){
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    getpeername(client_sock, (sockaddr*)&addr, &len);

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
    string ip_str(ip);
    int port = ntohs(addr.sin_port);
    return {port,ip_str};
}

int main(){
    srand(time(NULL));
    int sock;
    sockaddr_in server_addr;
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if((sock = socket(AF_INET,SOCK_STREAM,0)) < 0){
        perror("socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if(bind(sock,(sockaddr*)&server_addr,sizeof(server_addr)) < 0){
        perror("bind");
        exit(1);
    }

    if(listen(sock,10) < 0){
        perror("listen");
        exit(1);
    }

    string private_key = gen_random_key();
    cout<<"[server] listening on port "<<PORT<<endl;
    cout<<"[server] private key: "<<private_key<<endl;

    int epoll_fd = epoll_create1(0);
    struct epoll_event ev[MAX_EVENTS];

    add_sock_to_epoll(epoll_fd, sock);

    while(true){
        int events = epoll_wait(epoll_fd, ev, MAX_EVENTS, -1);
        for(int i=0;i<events;i++){
            int socket_fd = ev[i].data.fd;

            if(socket_fd == sock){
                int client_sock = accept(sock,NULL,NULL);
                if(client_sock < 0){
                    perror("accept");
                    exit(1);
                }
                User user;
                bool is_authorized = authorize_user(client_sock,private_key,user);
                if(!is_authorized){
                    close(client_sock);
                    continue;
                }
                user.client_sock = client_sock;
                auto [port , ip] = get_ip_and_port(client_sock);
                user.ip_addr = ip;
                user.port = port;
                string dir = ip + "_" + user.username;
                int ptyMaster = create_pty(dir);
                user.ptyMaster = ptyMaster;
                client_to_user[client_sock] = user;
                ptyMaster_to_user[ptyMaster] = user;

                add_sock_to_epoll(epoll_fd, client_sock);
                add_sock_to_epoll(epoll_fd, ptyMaster);

            }else if(client_to_user.find(socket_fd) != client_to_user.end()){
                User &user = client_to_user[socket_fd];
                int ptyMaster = user.ptyMaster;
                char response[BUFFER_SIZE];
                int n = recv(socket_fd,response,BUFFER_SIZE,0);
                if(n <= 0){
                    cleanup_connection(user,epoll_fd);
                    continue;
                }
                user.buffer.append(response,n);

                while(check_decode_message(user.buffer)){
                    auto [type , payload] = decode_message(user.buffer);
                    if(type == 1){
                        cleanup_connection(user,epoll_fd);
                    }else if(type == 2){
                        auto [token,data] = decode_data(payload);
                        if(user.token != token){
                            string message = message_of_error("invalid token");
                            send(socket_fd,message.c_str(),message.size(),0);
                            continue;
                        }
                        ssize_t wr_data = 0;
                        while(wr_data < data.size()) {
                            int w = write(ptyMaster, data.c_str() + wr_data, data.size() - wr_data);
                            if (w <= 0) break; 
                            wr_data += w;
                        }
                    }else continue;
                }
            }else if(ptyMaster_to_user.find(socket_fd) != ptyMaster_to_user.end()){
                User &user = ptyMaster_to_user[socket_fd];
                char response[BUFFER_SIZE];
                int n = read(socket_fd,response,BUFFER_SIZE);
                if(n <= 0){
                    string logout_msg = message_of_logout("remote shell exited");
                    send_All(user.client_sock, logout_msg);
                    cleanup_connection(user,epoll_fd);
                    continue;
                }
                string data(response,n);
                string message = message_of_data(user.token,data);
                send_All(user.client_sock,message);
            }
        }
    }
    close(sock);
    return 0;
}

