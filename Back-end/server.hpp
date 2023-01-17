#ifndef SERVER_HPP
#define SERVER_HPP


#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <set>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> //sockaddr, socklen_t
#include <arpa/inet.h>
#include <netdb.h>
#include <iostream>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
using namespace std;

#define INPUT_BUFFER_SIZE 1024 //test: 1024 bytes of buffer
#define DEFAULT_PORT 9999

#if __has_include(<format>)
#include <format>
#endif

#define FMT_HEADER_ONLY
#include <fmt/format.h>

inline std::string escapeJsonString(std::string input){
    for(int i=0;;i++){
        if(i>=input.length())
            break;
        if(input[i]=='\n'){
            input.erase(i,1);
            input.insert(i,"\\n");
            i++;
        }else if(input[i]=='\\'){
            input.insert(i,"\\");
            i++;
    }
    }
    return input;
}

void ShowCerts(SSL *ssl){
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

class db_user;
class question_bank;
class Server
{
public:
    Server(string, string);
    Server(string, string, int port);
    Server(const Server& orig);
    virtual ~Server();
    
    struct Connector {
        uint16_t source_fd;
    };
    
    void shutdown();
    void init();
    void loop();

    //callback setters
    void onConnect(void (*ncc)(uint16_t fd));
    void onInput(void (*rc)(uint16_t fd, char *buffer));
    void onDisconnect(void (*dc)(uint16_t fd));

    uint16_t sendMessage(Connector conn, const char *messageBuffer);
    uint16_t sendMessage(Connector conn, char *messageBuffer);
    uint16_t recvMessage(Connector conn, char *messageBuffer);
    uint16_t sendMessageSSL(SSL *ssl, char *messageBuffer);
    uint16_t sendMessageSSL(SSL *ssl, const char *messageBuffer);
    uint16_t recvMessageSSL(SSL *ssl, char *messageBuffer);

private:
    //fd_set file descriptor sets for use with FD_ macros
    fd_set masterfds;
    fd_set tempfds;

    //unsigned integer to keep track of maximum fd value, required for select()
    uint16_t maxfd;

    //socket file descriptors
    int mastersocket_fd; //master socket which receives new connections
    int tempsocket_fd; //temporary socket file descriptor which holds new clients

    //client connection data
    struct sockaddr_storage client_addr;
    //server socket details
    struct sockaddr_in servaddr;
    //input buffer
    char input_buffer[INPUT_BUFFER_SIZE];

    char remote_ip[INET6_ADDRSTRLEN];
    //int numbytes;
    SSL_CTX* ctx;
    SSL* ssl;
    map<int, SSL*> ssl_map;

    std::shared_ptr<db_user> user = std::make_shared<db_user>();
    std::shared_ptr<question_bank> question = std::make_shared<question_bank>();

    string message;
    map<int, string> bindIdentity;
    map<int, string> bindUsername;
    set<string> usernameSet;
    map<string, int> logined_users;

    void (*newConnectionCallback) (uint16_t fd);
    void (*receiveCallback) (uint16_t fd, char *buffer);
    void (*disconnectCallback) (uint16_t fd);

    void setup(int, string, string);
    void initializeSocket();
    void bindSocket();
    void startListen();
    void handleNewConnection();


    vector<string> recvInputFromExisting(Connector&);
    void sendMsgToExisting(Connector&, vector<string>&);
    vector<string> registerUser(Connector& connect_fd, string username, auto password, string identity);
    vector<string> authenticateUser(Connector& conn, string username, auto password);
    vector<string> logout(Connector&);
    int logout(string); // function overload
    vector<string> deleteUser(Connector&, string username);
    vector<string> deleteUserSelf(Connector&, auto password);
    vector<string> getUser(Connector& connect_fd);
    vector<string> getTeachers();

    vector<string> getSubjects();
    vector<string> getChapters(string subject);
    vector<string> addSubject(string);
    vector<string> addChapter(string subject, string);
    vector<string> getQuestions(string, string);
    vector<string> getQuestions(string, string, string);
    vector<string> writeQuestion(string, string, string, auto);
    vector<string> deleteQuestion(string, string, string);

    //void *getInetAddr(struct sockaddr *saddr);
};

#endif /* SERVER_HPP */