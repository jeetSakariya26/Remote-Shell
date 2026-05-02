#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pty.h>        
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <bits/stdc++.h>
#include <arpa/inet.h>
#include "helper.h"
#include "packet.h"
using namespace std;

#define PORT 9090
#define BUFFER_SIZE 4096
#define MAX_EVENTS 2

volatile sig_atomic_t flag = 0;

void changed_windowSize(int signal){
    flag = 1;
}

bool send_windowSize(int sock,string token){
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
        perror("ioctl failed");
        string message = message_of_logout("user wants to logout",token);
        send_All(sock,message);
        return false;
    }
    string message = message_of_window_size(token,ws.ws_col,ws.ws_row);
    send_All(sock, message);
    return true;
}


string authentication(int &sock,string username,string key,string ip){
    if((sock = socket(AF_INET,SOCK_STREAM,0)) < 0){
        perror("socket");
        exit(1);
    }
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    
    if(inet_pton(AF_INET,ip.c_str(),&server_addr.sin_addr) <= 0){
        perror("Invalid IP");
        exit(1);
    }
    
    if(connect(sock,(sockaddr*)&server_addr,sizeof(server_addr)) < 0){
        perror("connect");
        exit(1);
    }
    
    cout<<"Sending authentication request..."<<endl;
    string message = message_of_login(key,username);
    send_All(sock, message);
    cout<<"authenticating..."<<endl;
    
    // get only one response
    char response[BUFFER_SIZE];
    int n=recv(sock,response,BUFFER_SIZE,0);
    if (n <= 0) {
        cout<<"\r\n\033[0m[client] Connection closed due to error\r\n";
        string n_key;
        cout<<"Enter new key: ";
        cin>>n_key;
        return authentication(sock,username,n_key,ip);
    }
    string res(response,n);
    auto [type,payload] = decode_message(res);
    if(type == 6){
        cout<<"Authentication successful"<<endl;
        return decode_ack(payload);
    }else{
        string authentication_error = decode_error(payload);
        cout<<"Authentication failed due to: "<<authentication_error<<endl;
        string n_key;
        cout<<"Enter new key: ";
        cin>>n_key;
        return authentication(sock,username,n_key,ip);
    }
    return "";
}

int main(){
    string username;
    string key;
    string ip;
    cout<<"Enter username : ";cin>>username;
    cout<<"Enter key      : ";cin>>key;
    cout<<"Enter IP       : ";cin>>ip;
    int sock;
    string buffer;
    string token = authentication(sock,username,key,ip);
    // write(STDOUT_FILENO, "\033[?1049h\033[H\033[2J", 14);
    enter_raw_mode();
    
    bool current_window_size = send_windowSize(sock,token);
    if(!current_window_size){
        cout<<"\r\n\033[0m[client] Connection closed due to error\r\n";
        close(sock);
        return 0;
    }

    string welcome_msg = "[client] connected — raw terminal active (Ctrl-Q to quit)\r\n";
    write(STDOUT_FILENO, welcome_msg.c_str(), welcome_msg.size());

    

    signal(SIGWINCH, changed_windowSize);
    
    // while(check_decode_message(buffer)){
    //     auto [type, payload] = decode_message(buffer);
    //     if(type == 2){
    //         auto [t, data] = decode_data(payload);
    //         write(STDOUT_FILENO, data.c_str(), data.size());
    //     }
    // }

    int epoll_fd = epoll_create1(0);
    epoll_event ev[MAX_EVENTS];
    add_sock_to_epoll(epoll_fd, sock);
    add_sock_to_epoll(epoll_fd, STDIN_FILENO);

    while(true){
        int events = epoll_wait(epoll_fd, ev, MAX_EVENTS, -1);
        if(flag){
            if(!send_windowSize(sock,token)){
                cout<<"\r\n\033[0m[client] Connection closed due to error\r\n";
                close(sock);
                return 0;
            }
            flag = 0;
        }
        for(int i=0;i<events;i++){
            int socket_fd = ev[i].data.fd;


            if(socket_fd == sock){
                char response[BUFFER_SIZE];
                int n=recv(sock,response,BUFFER_SIZE,0);

                if( n <= 0){
                    // write(STDOUT_FILENO, "\033[?1049l", 8);
                    close_raw_mode();    
                    close(sock);
                    close(epoll_fd);
                    cout<<"\r\n\033[0m[client] disconnected\r\n";
                    exit(1);
                }
                buffer.append(response,n);
                while(check_decode_message(buffer)){
                    auto [type,payload] = decode_message(buffer);
                    if(type == 1){
                        // write(STDOUT_FILENO, "\033[?1049l", 8);
                        close_raw_mode();
                        close(sock);
                        close(epoll_fd);
                        cout<<"\r\n\033[0m[client] disconnected\r\n";
                        exit(0);
                    }else if(type == 2){
                        auto [token,data] = decode_data(payload);
                        ssize_t wr_data = 0;
                        while(wr_data < data.size()){
                            int w= write(STDOUT_FILENO,data.c_str()+wr_data,data.size()-wr_data);
                            wr_data+=w;
                        }
                    }
                }
            }else if(socket_fd == STDIN_FILENO){
                char input[BUFFER_SIZE];
                int n=read(STDIN_FILENO,input,BUFFER_SIZE);
                if(n <= 0) continue;
                for(int i=0;i<n;i++){
                    if(input[i] == 0x11){
                        string message = message_of_logout("user wants to logout",token);
                        send_All(sock,message);
                        cout<<"\r\n\033[0m[client] disconnected\r\n";
                        // write(STDOUT_FILENO, "\033[?1049l", 8);
                        close_raw_mode();
                        close(sock);
                        close(epoll_fd);
                        exit(0);
                    }
                }
                string msg(input,n);
                string message = message_of_data(token,msg);
                send_All(sock,message);
            }
        }
    }
    // write(STDOUT_FILENO, "\033[?1049l", 8);
    close_raw_mode();
    close(sock);
    close(epoll_fd);
    return 0;
}
