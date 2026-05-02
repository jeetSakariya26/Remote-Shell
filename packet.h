#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <termios.h>
#include <bits/stdc++.h>
using namespace std;

// types of packet
// 0 : login
// 1 : logout
// 2 : msg transfer
// 3 : share window size
// 4 : file transfer
// 5 : file ack
// 6 : ack
// 7 : error

struct WindowSize{
    unsigned short width;
    unsigned short height;
};

// made an message with [4 bytes total length] [1 byte for type] [message]
string make_message(int type,string message);

// login packet
// type 0
// [1 byte key length] [key] [1 byte username length] [username]
string message_of_login(string key, string username);
pair<string,string> decode_login(string& message);

// logout packet
// type 1
// [1 byte reason length] [reason]
string message_of_logout(string reason,string token);
pair<string,string> decode_logout(string& message);

// data packet
// type 2
// [1 byte token length] [token] [1 byte data length] [data]
string message_of_data(string token, string data);
pair<string,string> decode_data(string& message);

//login ack packet
// type 6
// [1 byte token length] [token]
string message_of_ack(string token);
string decode_ack(string& message);

// error packet
// type 7
// [1 byte error length] [error]
string message_of_error(string error);
string decode_error(string& message);

// window size packet
// type 3
// [1 byte token length] [token] [2 byte width] [2 byte height]
string message_of_window_size(string token, unsigned short width, unsigned short height);
pair<string,WindowSize> decode_window_size(string& message);



// decode message
pair<int ,string > decode_message(string& message);
bool check_decode_message(string& message);

bool send_All(int fd, const string &s);