#pragma once


#include <unordered_set>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> //sockaddr, socklen_t
#include <arpa/inet.h>
#include <netdb.h>
#include <iostream>
#include <exception>
#include <atomic>
#include <vector>
#include <algorithm>
#include <iterator>
#include <regex>
// #include <nlohmann/json.hpp>
// using json = nlohmann::json;
#include "glaze/glaze.hpp"

using namespace std;

#define INPUT_BUFFER_SIZE 1024 //test: 1024 bytes of buffer
#define DEFAULT_PORT 9999

#if __has_include(<format>)
#include <format>
#endif

#define FMT_HEADER_ONLY
#include <fmt/format.h>

// epoll
#include <sys/epoll.h>
#include <fcntl.h>
#define EVENTS_SIZE 20

#include <mutex>
#include <csignal>

const int max_concurrency = 8;

struct s1 {
    string command{};
    string username{};
    string password{};
    string identity{};
    string subject_name{};
    string chapter_name{};
    string question_id{};
    string question_text{};
    string bulletin_name{};
    string teacher_name{};
    string bulletin_text{};
};
// GLZ_META(s1, command, username, password, identity, subject_name, chapter_name, question_id, question_text, bulletin_name, teacher_name, bulletin_text);
template <>
struct glz::meta<s1>
{
    using T = s1;
    static constexpr auto value = object(&T::command, 
    &T::username, 
    &T::password, 
    &T::identity,
    &T::subject_name,
    &T::chapter_name,
    &T::question_id,
    &T::question_text,
    &T::bulletin_name,
    &T::teacher_name,
    &T::bulletin_text);
};


inline std::string escapeJsonString(std::string input){
    regex re1("\\n");
    regex re2("\\\\");
    input = regex_replace(regex_replace(input, re2, "\\\\"), re1, "\\n");
    return input;
}


class InterruptException : public std::exception
  {
   public:
    InterruptException(int _s) : signal_(_s) { }
    int signal() const noexcept
    {
      return this->signal_;
    }

   private:
    int signal_;
  };

/// method to throw exception at signal interrupt
void sig_to_exception(int s)
{
    throw InterruptException(s);
}

// #include "user_info.hpp"

struct Connector {
    private:
        uint16_t source_fd;
    public:
        Connector() {source_fd = 0;}
        Connector(uint16_t fd): source_fd(fd) {};
        void setFd(uint16_t fd_) {source_fd = fd_;}
        uint16_t getFd() const {return source_fd;}
        ~Connector() = default;
};

class db_user;
class question_bank;
class Server
{
protected:
    static shared_ptr<Server> server_;
    static std::mutex mutex_;

public:
    void operator=(Server const&) = delete;
    Server(Server const&) = delete;

    static shared_ptr<Server> getInstance(int port); 
    static shared_ptr<Server> getInstance(); 

    virtual ~Server();
    
    void shutdown();
    void init();
    // void loop(db_user&);
    void loop();

    //callback setters
    void onConnect(void (*ncc)(uint16_t fd));
    void onInput(void (*rc)(uint16_t fd, char *buffer));
    void onDisconnect(void (*dc)(uint16_t fd));

    uint16_t sendMessage(Connector conn, const char *messageBuffer);
    uint16_t sendMessage(Connector conn, char *messageBuffer);
    uint16_t recvMessage(Connector conn, char *messageBuffer);

private:

    s1 recv_struct{};
    unordered_set<string> subject_cache;
    unordered_map<string, unordered_set<string>> chapter_cache;
    std::atomic<int> user_count_cache = -1;
    std::atomic<int> subject_count_cache = -1;
    // make it singleton
    explicit Server();
    explicit Server(int port);
    //fd_set file descriptor sets for use with FD_ macros
    // fd_set masterfds;
    // fd_set tempfds;

    //unsigned integer to keep track of maximum fd value, required for select()
    // uint16_t maxfd;

    //socket file descriptors
    int mastersocket_fd; //master socket which receives new connections
    int eFd; // used for epoll
    epoll_event events[EVENTS_SIZE]; // callback array for epoll events
    epoll_event epev{};

    //client connection data
    struct sockaddr_storage client_addr;
    //server socket details
    struct sockaddr_in servaddr;
    //input buffer
    char input_buffer[INPUT_BUFFER_SIZE];

    char remote_ip[INET6_ADDRSTRLEN];
    //int numbytes;

    vector<std::shared_ptr<db_user>> users;
    vector<std::shared_ptr<question_bank>> questions;

    unordered_map<int, string> bindIdentity;
    unordered_map<int, string> bindUsername;
    unordered_set<string> usernameSet;
    unordered_map<string, int> logined_users;

    unordered_map<int, vector<string>> archived_msg;

    void (*newConnectionCallback) (uint16_t fd);
    void (*receiveCallback) (uint16_t fd, char *buffer);
    void (*disconnectCallback) (uint16_t fd);


    //function prototypes
    void setup(int port);
    void initializeSocket();
    void bindSocket();
    void startListen(int = 8);
    void handleNewConnection();

    tuple<vector<string>, Connector> recvInputFromExisting(shared_ptr<db_user>, shared_ptr<question_bank>, Connector&);
    void sendMsgToExisting(Connector&, vector<string> = vector<string>());
    vector<string> registerUser(shared_ptr<db_user>, Connector& connect_fd, string& username, auto password, string& identity);
    vector<string> authenticateUser(shared_ptr<db_user>, Connector& conn, string& username, auto password);
    vector<string> logout(shared_ptr<db_user>, Connector&);
    int logout(shared_ptr<db_user>, string&); // function overload
    vector<string> deleteUser(shared_ptr<db_user>, Connector&, string& username);
    vector<string> deleteUserSelf(shared_ptr<db_user>, Connector&, auto password);
    vector<string> getUser(shared_ptr<db_user>, Connector& connect_fd);
    vector<string> getTeachers(shared_ptr<db_user>);

    vector<string> getSubjects(shared_ptr<question_bank>);
    vector<string> getChapters(shared_ptr<question_bank>, string& subject);
    vector<string> addSubject(shared_ptr<question_bank>, string&);
    vector<string> addChapter(shared_ptr<question_bank>, string& subject, string&);
    vector<string> getQuestions(shared_ptr<question_bank>, string&, string&);
    vector<string> getQuestions(shared_ptr<question_bank>, string&, string&, string&);
    vector<string> writeQuestion(shared_ptr<question_bank>, string&, string&, string&, auto);
    vector<string> deleteQuestion(shared_ptr<question_bank>, string&, string&, string&);
    //void *getInetAddr(struct sockaddr *saddr);
    vector<string> readBulletin(shared_ptr<question_bank>, string&);
    vector<string> writeBulletin(shared_ptr<question_bank>, string&, string&, string&);
    vector<string> deleteBulletin(shared_ptr<question_bank>, string&);
};
