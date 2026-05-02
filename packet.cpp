#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <cstring>
#include <fcntl.h>
#include <arpa/inet.h>
#include <termios.h>
#include <string>
#include "packet.h"
using namespace std;


string make_message(int type,string message){
    uint32_t message_length = htonl(message.length()+1);
    string message_frame(4,'\0');
    memcpy(&message_frame[0], &message_length, 4);
    message_frame.push_back((char)type);
    message_frame += message;
    return message_frame;
}


// login packet
string message_of_login(string key,string username){
    string payload;
    payload.push_back((char)key.size());
    payload += key;
    payload.push_back((char)username.size());
    payload += username;
    return make_message(0,payload);
}

pair<string,string> decode_login(string& message){
    uint8_t key_len = (uint8_t)message[0];
    string key = message.substr(1, key_len);
    uint8_t username_len = (uint8_t)message[1 + key_len];
    string username = message.substr(1 + key_len + 1, username_len);
    message.erase(0, 1 + key_len + 1 + username_len);
    return {key,username};
}


// logout packet
string message_of_logout(string reason,string token){
    string payload;
    payload.push_back((char)token.size());
    payload += token;
    payload.push_back((char)reason.size());
    payload += reason;
    return make_message(1,payload);
}

pair<string,string> decode_logout(string& message){
    uint8_t token_len = (uint8_t)message[0];
    string token = message.substr(1, token_len);
    message.erase(0, 1 + token_len);
    uint8_t reason_len = (uint8_t)message[0];
    string reason = message.substr(1, reason_len);
    message.erase(0, 1 + reason_len);
    return {token,reason};
}

// data packet

string message_of_data(string token, string keystrokes){
    string payload;
    payload.push_back((char)token.size());
    payload += token;
    payload.push_back((char)keystrokes.size());
    payload += keystrokes;
    return make_message(2,payload);
}
pair<string,string> decode_data(string& message){
    uint8_t token_len = (uint8_t)message[0];
    string token = message.substr(1, token_len);
    string keystrokes = message.substr(1 + token_len + 1);
    message.erase(0, 1 + token_len + 1 + keystrokes.size());
    return {token,keystrokes};
}

// ACK packet

string message_of_ack(string token){
    string payload;
    payload.push_back((char)token.size());
    payload += token;
    return make_message(6,payload);
}

string decode_ack(string& message){
    uint8_t token_len = (uint8_t)message[0];
    string token = message.substr(1, token_len);
    message.erase(0, 1 + token_len);
    return token;
}

// error packet

string message_of_error(string error){
    string payload;
    payload.push_back((char)error.size());
    payload += error;
    return make_message(7,payload);
}

string decode_error(string& message){
    uint8_t error_len = (uint8_t)message[0];
    string error = message.substr(1, error_len);
    message.erase(0, 1 + error_len);
    return error;
}

// window size packet
string message_of_window_size(string token, unsigned short width, unsigned short height){
    string payload;
    payload.push_back((char)token.size());
    payload += token;
    string pkt_data(4,'\0');
    uint16_t w = htons(width);
    uint16_t h = htons(height);
    memcpy(&pkt_data[0], &w, 2);
    memcpy(&pkt_data[2], &h, 2);
    payload += pkt_data;
    return make_message(3,payload);
}

pair<string,WindowSize> decode_window_size(string& message){
    uint8_t token_len = (uint8_t)message[0];
    string token = message.substr(1, token_len);
    string pkt_data = message.substr(1 + token_len);
    WindowSize ws;
    uint16_t w;
    uint16_t h;
    uint16_t x_p;
    uint16_t y_p;
    memcpy(&w, pkt_data.data(), 2);
    ws.width = ntohs(w);
    memcpy(&h, pkt_data.data() + 2, 2);
    ws.height = ntohs(h);
    message.erase(0, 5+token_len);
    return {token,ws};
}


// decode one message

bool check_decode_message(string& message){
    if(message.size() < 4 ) return false;
    uint32_t nlen;
    memcpy(&nlen, message.data(), 4);
    uint32_t body_len = ntohl(nlen);
    if (body_len < 1) {
        message.erase(0, 4);
        return false;
    }
    const uint32_t MAX_PACKET = 1024 * 1024;
    if (body_len > MAX_PACKET) {
        message.clear();
        return false;
    }
    if(message.size() < 4 + body_len ) return false;
    return true;
}
pair<int,string> decode_message(string& message){
    if(message.size() < 4 ) return {-1,""};
    uint32_t nlen;
    memcpy(&nlen, message.data(), 4);
    uint32_t body_len = ntohl(nlen);
    if (body_len < 1) {
        message.erase(0, 4);
        return {-1,""};
    }
    const uint32_t MAX_PACKET = 1024 * 1024;
    if (body_len > MAX_PACKET) {
        message.clear();
        return {-1,""};
    }
    if(message.size() < 4 + body_len ) return {-1,""};

    uint8_t type = (uint8_t)message[4];
    string payload = message.substr(5, body_len - 1);

    message.erase(0, 4 + body_len);
    return {type,payload};
}


bool send_All(int fd, const string &s) {
    size_t sent = 0;
    while (sent < s.size()) {
        ssize_t n = send(fd, s.data() + sent, s.size() - sent, MSG_NOSIGNAL);
        if (n <= 0) return false;
        sent += n;
    }
    return true;
}