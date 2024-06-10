#pragma once

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
// #include <openssl/ssl.h>
// #include <openssl/err.h>
// #include <nlohmann/json.hpp>
// using json = nlohmann::json;
#include "glaze/glaze.hpp"
#include "glaze/core/macros.hpp"
using namespace std;

#if __has_include(<format>)
#include <format>
#endif

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#define INPUT_BUFFER_SIZE 256 //test: 256 bytes of buffer
#define PORT 9999


struct s1 {
    int code{};
    string identity{};
    string question_text{};
    string username{};
    int counts{0};
};

template <>
struct glz::meta<s1>
{
    using T = s1;
    static constexpr auto value = object("code", &T::code, "identity", &T::identity, "question_text", &T::question_text, 
    "username", &T::username, "counts", &T::counts);
};

inline std::string escapeJsonString(std::string input){
    regex re1("\\n");
    regex re2("\\\\");
    input = regex_replace(regex_replace(input, re2, "\\\\"), re1, "\\n");
    return input;
}

class Client{
    private:
        int socket_fd, num_bytes;
        struct sockaddr_in serv_addr;
        char buffer[256];
        struct hostent *server;
        // SSL_CTX* ctx;
        // SSL* ssl;

        int read_iterative(char* ptr, int size);
        int write_iterative(char* ptr, int size);
        int recv_iterative(char* ptr, int size, int flag);
        int send_iterative(char* otr, int size, int flag);
        void error(const char *msg);

        s1 recv_struct{};
        
    public:
        struct UserInfo{
            string username;
            string password;
            string identity;
            string status {};
        };
        struct Connector{
            uint16_t source_fd;
        };
        Client();
        Client(int);
        Client(const Client& orig);
        virtual ~Client();
        void setup(int port);
        void loop();
        void initConnect(hostent *server);

        uint16_t sendMessage(Connector conn, const char *messageBuffer);
        uint16_t sendMessage(Connector conn, char *messageBuffer);
        uint16_t recvMessage(Connector conn, char *messageBuffer);

        // uint16_t sendMessageSSL(SSL *ssl, char *messageBuffer);
        // uint16_t sendMessageSSL(SSL *ssl, const char *messageBuffer);
        // uint16_t recvMessageSSL(SSL *ssl, char *messageBuffer);

};