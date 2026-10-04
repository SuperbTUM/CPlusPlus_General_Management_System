#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
// #include <nlohmann/json.hpp>
// using json = nlohmann::json;
#include "glaze/glaze.hpp"
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

inline void ShowCerts(SSL *ssl){
    X509 *cert;
    char* line;
    cert = SSL_get_peer_certificate(ssl);
    if(SSL_get_verify_result(ssl) == X509_V_OK){
        cout<<"Authentication passed."<<endl;
    }
    if(cert) {
        line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
        cout<<"Certificate info: "<<line<<endl;
        free(line);
        line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
        cout<<"Provider: "<<line<<endl;
        free(line);
        X509_free(cert);
    }
    else cout<<"No certificate provided."<<endl;
}

class Client{
    private:
        int socket_fd{-1}, num_bytes{0};
        struct sockaddr_in serv_addr;
        char buffer[256];
        struct hostent *server{nullptr};
        SSL_CTX* ctx{nullptr};
        SSL* ssl{nullptr};
        s1 recv_struct{};

        int read_iterative(SSL* ssl, char* ptr, int size);
        int write_iterative(SSL* ssl, char* ptr, int size);
        void error(const char *msg);
        
    public:
        struct UserInfo{
            string username;
            string password;
            string identity;
            string status {};
        };
        struct Connector{
            int source_fd{-1};
            int getFd() const { return source_fd; }
            void setFd(int fd) { source_fd = fd; }
        };
        Client(char*, char*);
        Client(char*, char*, int);
        Client(const Client&) = delete;
        Client& operator=(const Client&) = delete;
        virtual ~Client();
        void setup(const int, const char*, const char*);
        void loop();
        void initConnect(hostent *server);

        int sendMessage(Connector conn, const char *messageBuffer);
        int sendMessage(Connector conn, char *messageBuffer);
        int recvMessage(Connector conn, char *messageBuffer);

        int sendMessageSSL(SSL *ssl, char *messageBuffer);
        int sendMessageSSL(SSL *ssl, const char *messageBuffer);
        int recvMessageSSL(SSL *ssl, char *messageBuffer);

};
#endif /* CLIENT_HPP */