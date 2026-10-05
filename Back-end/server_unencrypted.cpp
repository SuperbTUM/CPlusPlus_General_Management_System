#include "server_unencrypted.hpp"
#include "user_info.cpp"
#include "question_bank.cpp"
#include <omp.h>
#include <utility>
#include <span>
#include <ranges>
#include <algorithm>
#include <print>
using namespace std;

vector<string>& helper(vector<string>& msg, string&& keyword) {
    auto formatting = [&](string a) constexpr -> string{return (keyword == "code" || keyword == "counts")? fmt::format("{{\"{}\":{}}}", keyword, a): fmt::format("{{\"{}\":\"{}\"}}", keyword, a);};
    std::ranges::transform(msg, msg.begin(), formatting);
    return msg;
}

Server::Server()
{
    users.resize(max_concurrency);
    std::ranges::for_each(users, [](std::shared_ptr<db_user> &ptr) {
        ptr = std::make_shared<db_user>();
    });
    questions.resize(max_concurrency);
    std::ranges::for_each(questions, [](std::shared_ptr<question_bank> &ptr) {
        ptr = std::make_shared<question_bank>();
    });
    setup(DEFAULT_PORT);
}

Server::Server(int port)
{   
    users.resize(max_concurrency);
    std::ranges::for_each(users, [](std::shared_ptr<db_user> &ptr) {
        ptr = std::make_shared<db_user>();
    });
    questions.resize(max_concurrency);
    std::ranges::for_each(questions, [](std::shared_ptr<question_bank> &ptr) {
        ptr = std::make_shared<question_bank>();
    });
    setup(port);
}

// Server::Server(const Server& orig)
// {
//     // masterfds = orig.masterfds;
//     // tempfds = orig.tempfds;
//     // maxfd = orig.maxfd;
//     mastersocket_fd = orig.mastersocket_fd;
//     tempsocket_fd = orig.tempsocket_fd;

//     char input_buffer[INPUT_BUFFER_SIZE];
//     strcpy(input_buffer, orig.input_buffer);
//     char remote_ip[INET6_ADDRSTRLEN];
//     strcpy(remote_ip, orig.remote_ip);
// }

Server::~Server()
{
	#ifdef SERVER_DEBUG
	std::cout << "[SERVER] [DESTRUCTOR] Destroying Server...\n";
	#endif
	close(mastersocket_fd);
}

void Server::setup(int port)
{
    mastersocket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (mastersocket_fd < 0) {
        perror("Socket creation failed");
    }

    // FD_ZERO(&masterfds);
    // FD_ZERO(&tempfds);

    memset(&servaddr, 0, sizeof (servaddr)); //bzero
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htons(INADDR_ANY);
    servaddr.sin_port = htons(port);

    bzero(&input_buffer, INPUT_BUFFER_SIZE); //zero the input buffer before use to avoid random data appearing in first receives

}

void Server::initializeSocket()
{
	#ifdef SERVER_DEBUG
	std::cout << "[SERVER] initializing socket\n";
	#endif
	int opt_value = 1;
	int ret_test = setsockopt(mastersocket_fd, SOL_SOCKET, SO_REUSEADDR, (char *) &opt_value, sizeof (int));
	setsockopt(mastersocket_fd, IPPROTO_TCP, TCP_NODELAY, (char *) &opt_value, sizeof (int));
	#ifdef SERVER_DEBUG
	printf("[SERVER] setsockopt() ret %d\n", ret_test);
    #endif
	if (ret_test < 0) {
        	perror("[SERVER] [ERROR] setsockopt() failed");
		shutdown();
    	}
}

void Server::bindSocket()
{
	#ifdef SERVER_DEBUG
	std::cout << "[SERVER] binding...\n";
	#endif
	int bind_ret = bind(mastersocket_fd, (struct sockaddr*) &servaddr, sizeof (servaddr));
	#ifdef SERVER_DEBUG
	printf("[SERVER] bind() ret %d\n", bind_ret);
	#endif
	if (bind_ret < 0) {
		perror("[SERVER] [ERROR] bind() failed");
	}
	// FD_SET(mastersocket_fd, &masterfds); //insert the master socket file-descriptor into the master fd-set
	// maxfd = mastersocket_fd; //set the current known maximum file descriptor count
}

void Server::startListen(int connection)
{
	#ifdef SERVER_DEBUG
	std::cout << "[SERVER] listen starting...\n";
	#endif
	int listen_ret = listen(mastersocket_fd, connection);
	#ifdef SERVER_DEBUG
	printf("[SERVER] listen() ret %d\n", listen_ret);
	#endif
	if (listen_ret < 0) {
		perror("[SERVER] [ERROR] listen() failed");
	}
    eFd = epoll_create(1);
    epev.events = EPOLLIN;
    epev.data.fd = mastersocket_fd;
    epoll_ctl(eFd, EPOLL_CTL_ADD, mastersocket_fd, &epev);
}

void Server::shutdown()
{
	int close_ret = close(mastersocket_fd);
	#ifdef SERVER_DEBUG
	printf("[SERVER] [DEBUG] [SHUTDOWN] closing master fd..  ret '%d'.\n",close_ret);
	#endif
    for(int i=0; i<max_concurrency; i++) {
        users[i].reset();
        questions[i].reset();
    }
    
}

void Server::handleNewConnection()
{
	#ifdef SERVER_DEBUG
  	std::cout << "[SERVER] [CONNECTION] handling new connection\n";
    #endif
    socklen_t addrlen = sizeof (client_addr);
    int tempsocket_fd = accept(mastersocket_fd, (struct sockaddr*) &client_addr, &addrlen);
    	
	if (tempsocket_fd < 0) {
        perror("[SERVER] [ERROR] accept() failed");
        return;
	}

    int flags = fcntl(tempsocket_fd, F_GETFL, 0);
    if(flags < 0 || fcntl(tempsocket_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        fmt::print("Set non-blocking error, fd: {}\n", tempsocket_fd);
        close(tempsocket_fd);
        return;
    } 

    int nodelay_opt = 1;
    setsockopt(tempsocket_fd, IPPROTO_TCP, TCP_NODELAY, (char *)&nodelay_opt, sizeof(nodelay_opt));

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = tempsocket_fd;
    if(epoll_ctl(eFd, EPOLL_CTL_ADD, tempsocket_fd, &ev) < 0) {
        perror("[SERVER] epoll_ctl add failed");
        close(tempsocket_fd);
        return;
    }

    fmt::print("Successfully connected, fd: {}\n", tempsocket_fd);
}

void Server::sendMsgToExisting(Connector& connect_fd, span<const string> messages){
    std::unique_lock<std::shared_mutex> lock(state_mutex);
    vector<string> resend_buffer;
    if(messages.empty()) {
        // resend
        auto it = archived_msg.find(connect_fd.getFd());
        if(it != archived_msg.end() && !it->second.empty()) {
            resend_buffer = std::move(it->second);
            archived_msg.erase(it);
            messages = resend_buffer;
        }
    } 
    for(size_t i = 0; i < messages.size(); i++){
        int bytes = sendMessage(connect_fd, messages[i].c_str());
        int retry = 5;
        while(bytes < 0 && retry-- > 0){
            bytes = sendMessage(connect_fd, messages[i].c_str());
        }
        // If still failed to send, archive the msg and send again afterwards
        if(bytes < 0) {
            archived_msg[connect_fd.getFd()].emplace_back(messages[i]);
            fmt::print("Message sent incomplete!\n");
        }
    }
}


tuple<vector<string>, Connector> Server::recvInputFromExisting(std::shared_ptr<db_user> cur_user, std::shared_ptr<question_bank> cur_question, Connector& connect_fd)
{
    vector<string> messages;
    char input_buffer[INPUT_BUFFER_SIZE]{0};
    int nbytesrecv = recvMessage(connect_fd, input_buffer);
    fmt::print("Received bytes: {}\n", nbytesrecv);
    if (nbytesrecv <= 0)
    {
        int fd = connect_fd.getFd();
        if (nbytesrecv < 0)
        {   
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return make_tuple<vector<string>, Connector>(std::move(messages), std::move(connect_fd));
            }
            perror("[SERVER] [ERROR] recv() failed");
        }
        epoll_ctl(eFd, EPOLL_CTL_DEL, fd, nullptr);
        close(fd);
        {
            std::unique_lock<std::shared_mutex> lock(state_mutex);
            auto it = bindUsername.find(fd);
            if (it != bindUsername.end()) {
                logined_users.erase(it->second);
                bindUsername.erase(it);
            }
            bindIdentity.erase(fd);
            archived_msg.erase(fd);
        }
        return make_tuple<vector<string>, Connector>(std::move(messages), std::move(connect_fd));
    }
    #ifdef SERVER_DEBUG
    printf("[SERVER] [RECV] Received '%s' from client!\n", input_buffer);
    #endif
    // authenticate the identity of the user
    s1 recv_struct{};
    if (auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(recv_struct, input_buffer)) {
        std::println(stderr, "[SERVER] JSON parse error: {}", glz::format_error(ec, input_buffer));
        return make_tuple<vector<string>, Connector>(std::move(messages), std::move(connect_fd));
    }
    
    string client_identity;
    string client_username;
    int teacher_target_fd = -1;
    {
        std::shared_lock<std::shared_mutex> lock(state_mutex);
        auto it_id = bindIdentity.find(connect_fd.getFd());
        if (it_id != bindIdentity.end()) client_identity = it_id->second;
        auto it_un = bindUsername.find(connect_fd.getFd());
        if (it_un != bindUsername.end()) client_username = it_un->second;
        if (recv_struct.command == "write bulletin") {
            auto it_teacher = logined_users.find(recv_struct.teacher_name);
            if (it_teacher != logined_users.end()) teacher_target_fd = it_teacher->second;
        }
    }

    auto task = processRequestAsync(recv_struct, cur_user, cur_question, connect_fd, client_identity, client_username);
    messages = task.run_sync();

    Connector target_connector;
    if(recv_struct.command == "write bulletin" && teacher_target_fd != -1) target_connector = Connector(teacher_target_fd);
    else target_connector = Connector(connect_fd);
    return make_tuple<vector<string>, Connector>(std::move(messages), std::move(target_connector));
}

async::Task<vector<string>> Server::processRequestAsync(
    s1 recv_struct,
    std::shared_ptr<db_user> cur_user,
    std::shared_ptr<question_bank> cur_question,
    Connector connect_fd,
    string client_identity,
    string client_username
) {
    auto command = recv_struct.command;
    string username = recv_struct.username;
    string password = recv_struct.password;
    string identity = recv_struct.identity;
    string subject_name = recv_struct.subject_name;
    string chapter_name = recv_struct.chapter_name;
    string question_id = recv_struct.question_id;
    auto question_text = recv_struct.question_text;
    string bulletin_name = recv_struct.bulletin_name;
    string teacher_name = recv_struct.teacher_name;
    string bulletin_text = recv_struct.bulletin_text;

    vector<string> messages;

    if(command == "login"){
        messages = authenticateUser(cur_user, connect_fd, username, password);
    }
    else if(command == "get users" && client_identity == "admin"){
        messages = getUser(cur_user, connect_fd);
    }
    else if(command == "register user"){
        messages = registerUser(cur_user, connect_fd, username, password, identity);
    }
    else if(command == "delete user" && !client_identity.empty()){
        if(client_identity == "admin") messages = deleteUser(cur_user, connect_fd, username);
        else messages = deleteUserSelf(cur_user, connect_fd, password);
    }
    else if(command == "logout" && !client_username.empty()) {
        messages = logout(cur_user, connect_fd);
    }
    else if(command == "get teachers" && client_identity == "rule maker") {
        messages = getTeachers(cur_user);
    }
    else if(command == "get subjects" && !client_username.empty()) {
        messages = getSubjects(cur_question);
    }
    else if(command == "get chapters" && !client_username.empty()) {
        messages = getChapters(cur_question, subject_name);
    }
    else if(command == "get questions" && !client_username.empty()) {
        messages = getQuestions(cur_question, subject_name, chapter_name);
    }
    else if(command == "read question" && !client_username.empty()) {
        messages = getQuestions(cur_question, subject_name, chapter_name, question_id);
    }
    else if(command == "write question" && !client_username.empty()) {
        messages = writeQuestion(cur_question, subject_name, chapter_name, question_id, question_text);
    }
    else if(command == "delete question" && !client_username.empty()) {
        messages = deleteQuestion(cur_question, subject_name, chapter_name, question_id);
    }
    else if(command == "write subject" && client_identity == "teacher") {
        messages = addSubject(cur_question, subject_name);
    }
    else if(command == "write chapter" && client_identity == "teacher") {
        messages = addChapter(cur_question, subject_name, chapter_name);
    }
    else if(command == "read bulletin") {
        messages = readBulletin(cur_question, bulletin_name);
    }
    else if(command == "write bulletin") {
        messages = writeBulletin(cur_question, bulletin_name, bulletin_text, teacher_name);
    }
    else if(command == "delete bulletin") {
        messages = deleteBulletin(cur_question, bulletin_name);
    }
    else{
        fmt::print("Invalid command or not enough permission.\n");
        const int status_code = 403;
        #ifdef __cpp_lib_format
        string message = std::vformat("{{\"code\": {}}}", std::make_format_args(status_code));
        #else
        string message = fmt::format("{{\"code\": {}}}", status_code);
        #endif
        messages.emplace_back(std::forward<string>(message));
    }

    co_return messages;
}

vector<string> Server::readBulletin(shared_ptr<question_bank> cur_question, string& bulletin_name) {
    // TODO
    vector<string> messages;
    return messages;
}

vector<string> Server::writeBulletin(shared_ptr<question_bank> cur_question, string& bulletin_name, string& bulletin_text, string& teacher_name) {
    // TODO
    vector<string> messages;
    return messages;
}

vector<string> Server::deleteBulletin(shared_ptr<question_bank> cur_question, string& bulletin_name) {
    // TODO
    vector<string> messages;
    return messages;
}

vector<string> Server::authenticateUser(std::shared_ptr<db_user> cur_user, Connector& connect_fd, string& username, auto password){
    int status_code;
    // with database logic
    // optional<pair<string, variant<string, int, double>>> constraint;
    password = encrypt_password(password);
    string key = "password";
    string target_attribute = "identity";
    pair<string, variant<string, int, double>> constraint = std::make_pair(key, password);
    string identity = cur_user->getUserAttribute(username, target_attribute, constraint);
    if(!identity.empty()){
        target_attribute = "activity";
        int activity = stoi(cur_user->getUserAttribute(username, target_attribute, constraint));
        if(activity){
            // cout<<"User already login! Logout from previous device and re-login!"<<endl;
            fmt::print("User already login! Logout from previous device and re-login!\n");
            int logout_status = logout(cur_user, username);
        }
        status_code = 200;
        activity = 1;
        const string primary_val = std::as_const(username);
        vector<pair<string, variant<string, int, double>>> changelist;
        changelist.emplace_back("activity", activity);
        cur_user->update(primary_val, changelist);

    }
    else{
        status_code = 404;
        // cout<<"Wrong authentication!"<<endl;
        fmt::print("Wrong authentication!\n");
    }
    vector<string> messages;
    #ifdef __cpp_lib_format
    string message = std::vformat("{{\"code\": {}, \"identity\": \"{}\"}}", std::make_format_args(status_code, identity));
    #else
    string message = fmt::format("{{\"code\": {}, \"identity\": \"{}\"}}", status_code, identity);
    #endif
    // cout<<"checkin message: "<<message<<endl;
    fmt::print("checkin message: {}\n", message);
    messages.emplace_back(std::forward<string>(message));
    
    if(status_code == 200) {
        std::unique_lock<std::shared_mutex> lock(state_mutex);
        bindIdentity[connect_fd.getFd()] = identity;
        bindUsername[connect_fd.getFd()] = username;
        logined_users[username] = connect_fd.getFd();
    }
    return messages;
}

vector<string> Server::registerUser(std::shared_ptr<db_user> cur_user, Connector& connect_fd, string& username, auto password, string& identity){
    int status_code;
    // with database logic
    const std::shared_ptr<UserInfo<string>> new_user = std::make_shared<UserInfo<string>>(username, static_cast<std::string>(password), identity, "valid");
    int result = cur_user->insert(new_user);
    // delete new_user;
    if(result == -1) status_code = 403;
    else {
        status_code = 200;
        if(user_count_cache >= 0) user_count_cache ++;
    }
    vector<string> messages;
    #ifdef __cpp_lib_format
    string message = std::vformat("{{\"code\": {}}}", std::make_format_args(status_code));
    #else
    string message = fmt::format("{{\"code\": {}}}", status_code);
    #endif
    // messages.push_back(message);
    // messages.push_back(std::move(message));
    messages.emplace_back(std::forward<string>(message));
    if(status_code == 200) {
        std::unique_lock<std::shared_mutex> lock(state_mutex);
        usernameSet.insert(username);
    }
    return messages;
}


vector<string> Server::logout(std::shared_ptr<db_user> cur_user, Connector& connect_fd){
    int status_code;
    int activity_updated = 0;
    string username;
    {
        std::shared_lock<std::shared_mutex> lock(state_mutex);
        auto it = bindUsername.find(connect_fd.getFd());
        if (it != bindUsername.end()) username = it->second;
    }
    vector<pair<string, variant<string, int, double>>> constraint;
    constraint.emplace_back("activity", activity_updated);
    int res = cur_user->update(std::as_const(username), constraint);
    if(res < 0){
        // cout<<"logout failed."<<endl;
        fmt::print("logout failed.\n");
        status_code = 403;
    }
    else {
        std::unique_lock<std::shared_mutex> lock(state_mutex);
        bindUsername.erase(connect_fd.getFd());
        bindIdentity.erase(connect_fd.getFd());
        if (!username.empty()) logined_users.erase(username);
        status_code = 200;
    }
    vector<string> messages;
    #ifdef __cpp_lib_format
    string message = std::vformat("{{\"code\": {}}}", std::make_format_args(status_code));
    #else
    string message = fmt::format("{{\"code\": {}}}", status_code);
    #endif

    messages.emplace_back(std::forward<string>(message));
    return messages;
}

int Server::logout(std::shared_ptr<db_user> cur_user, string& username){
    int source_fd = -1;
    {
        std::shared_lock<std::shared_mutex> lock(state_mutex);
        auto it = logined_users.find(username);
        if(it == logined_users.end()){
            return -1;
        }
        source_fd = it->second;
    }
    vector<pair<string, variant<string, int, double>>> constraint;
    constraint.emplace_back("activity", 0);
    int res = cur_user->update(std::as_const(username), constraint);
    if(res < 0){
        // cout<<"logout failed."<<endl;
        fmt::print("logout failed.\n");
    } else {
        std::unique_lock<std::shared_mutex> lock(state_mutex);
        logined_users.erase(username);
        if (source_fd != -1) {
            bindIdentity.erase(source_fd);
            bindUsername.erase(source_fd);
        }
    }
    // cout<<"Logout from other device successfully!"<<endl;
    fmt::print("Logout from other device successfully!\n");
    return res;
}

vector<string> Server::getUser(std::shared_ptr<db_user> cur_user, Connector& connect_fd){
    vector<string> usernames;
    int status_code;
    int numUsers;
    // with database logic
    if(user_count_cache < 0) {
        numUsers = user_count_cache = cur_user->count();
    } else {
        numUsers = user_count_cache;
    }
    
    if(numUsers < 0) status_code = 403;
    else status_code = 200;

    vector<string> messages;
    #ifdef __cpp_lib_format
    string message = std::vformat("{{\"code\": {}, \"counts\": {}}}", std::make_format_args(status_code, numUsers));
    #else
    string message = fmt::format("{{\"code\": {}, \"counts\": {}}}", status_code, numUsers);
    #endif

    messages.reserve(numUsers+1);

    messages.emplace_back(std::forward<string>(message));
    if(numUsers < 0) return messages;
    optional<pair<string, string>> constraint;
    usernames = cur_user->getUserAttributes(constraint, "USERNAME");

    // experimental
    usernames = helper(usernames, "username");
    messages.insert(messages.end(), make_move_iterator(usernames.begin()), make_move_iterator(usernames.end()));

    return messages;
}

vector<string> Server::deleteUser(std::shared_ptr<db_user> cur_user, Connector& connect_fd, string& username){
    int status_code = 200;

    {
        std::unique_lock<std::shared_mutex> lock(state_mutex);
        auto un = usernameSet.find(username);
        if(un != usernameSet.end()) {
            status_code = 200;
            usernameSet.erase(un);
            if(user_count_cache > 0) user_count_cache --;
        } else {
            status_code = 403;
        }
        auto login_it = logined_users.find(username);
        if (login_it != logined_users.end()) {
            int target_fd = login_it->second;
            logined_users.erase(login_it);
            bindIdentity.erase(target_fd);
            bindUsername.erase(target_fd);
        }
    }
    
    // with database logic
    string key = "status";
    pair<string, variant<string, int, double>> deleted_detail;
    deleted_detail = std::make_pair(key, "valid");
    int result = cur_user->delet(std::as_const(username), deleted_detail);
    if(result == -1 && status_code == 200) status_code = 404;
    else if(result >= 0) status_code = 200;

    vector<string> messages;
    #ifdef __cpp_lib_format
    string message = std::vformat("{{\"code\": {}}}", std::make_format_args(status_code));
    #else
    string message = fmt::format("{{\"code\": {}}}", status_code);
    #endif

    messages.emplace_back(std::forward<string>(message));
    return messages;
}

vector<string> Server::deleteUserSelf(std::shared_ptr<db_user> cur_user, Connector& connect_fd, auto password){
    string username;
    int status_code = 200;

    {
        std::unique_lock<std::shared_mutex> lock(state_mutex);
        int fd = connect_fd.getFd();
        auto identity_iter = bindIdentity.find(fd);
        if(identity_iter == bindIdentity.end()){
            status_code = 403;
            // cout<<"Identity not found!"<<endl;
            fmt::print("Identity not found!\n");
        } else {
            status_code = 200;
            // cout<<"Identity found!"<<endl;
            fmt::print("Identity found!\n");
            bindIdentity.erase(identity_iter);
            if(user_count_cache > 0) user_count_cache --;
        }

        auto username_iter = bindUsername.find(fd);
        if(username_iter == bindUsername.end()){
            status_code = 403;
            // cout<<"Username not found!"<<endl;
            fmt::print("Username not found!\n");
        } else {
            username = username_iter->second;
            status_code = 200;
            // cout<<"Username found!"<<endl;
            fmt::print("Username found!\n");
            bindUsername.erase(username_iter);
            logined_users.erase(username);
            usernameSet.erase(username);
        }
    }

    // with database logic
    string key = "password";
    pair<string, variant<string, int, double>> deleted_detail = std::make_pair(key, password);
    int result = cur_user->delet(std::as_const(username), deleted_detail);
    if(result == -1 && status_code == 200) status_code = 404;
    else if(result >= 0) status_code = 200;

    vector<string> messages;
    #ifdef __cpp_lib_format
    string message = std::vformat("{{\"code\": {}}}", std::make_format_args(status_code));
    #else
    string message = fmt::format("{{\"code\": {}}}", status_code);
    #endif

    messages.emplace_back(std::forward<string>(message));
    return messages;
}

vector<string> Server::getTeachers(std::shared_ptr<db_user> cur_user){
    int status_code;
    vector<pair<string, string>> constraint;
    // constraint.push_back(std::make_pair("ACTIVITY", "1"));
    constraint.emplace_back(std::make_pair("IDENTITY", "teacher"));
    vector<string> teachers = cur_user->getUserAttributes(constraint, "USERNAME");
    if(teachers.empty()) status_code = 403;
    else status_code = 200; 
    vector<string> messages;
    #ifdef __cpp_lib_format
    const int teacher_size = teachers.size();
    string message = std::vformat("{{\"code\": {}, \"counts\": {}}}", std::make_format_args(status_code, teacher_size));
    #else
    string message = fmt::format("{{\"code\": {}, \"counts\": {}}}", status_code, teachers.size());
    #endif

    messages.reserve(teachers.size()+1);

    messages.emplace_back(std::forward<string>(message));

    //experimental
    teachers = helper(teachers, "username");

    messages.insert(messages.end(), make_move_iterator(teachers.begin()), make_move_iterator(teachers.end()));
    return messages;
}

vector<string> Server::getSubjects(std::shared_ptr<question_bank> cur_question){
    int status_code;
    string message;
    vector<string> messages;
    const string target_attribute = "subject";
    optional<pair<string, variant<string, int, double>>> count_info;
    int subject_num;
    if(subject_count_cache < 0) {
        subject_num = subject_count_cache = cur_question->countDistinct(target_attribute, count_info);
    } else {
        subject_num = subject_count_cache;
    }
    
    if(subject_num < 0){
        status_code = 403;
        #ifdef __cpp_lib_format
        message = std::vformat("{{\"code\": {}, \"counts\": {}}}", std::make_format_args(status_code, subject_num));
        #else
        message = fmt::format("{{\"code\": {}, \"counts\": {}}}", status_code, subject_num);
        #endif
        // messages.push_back(message);
        // messages.push_back(std::move(message));
        messages.emplace_back(std::forward<string>(message));
        return messages;
    }
    else status_code = 200;
    optional<pair<string, string>> constraint;
    vector<string> subjects = cur_question->getQuestionAttributes(constraint, target_attribute);
    #ifdef __cpp_lib_format
    const int subjects_size = subjects.size();
    message = std::vformat("{{\"code\": {}, \"counts\": {}}}", std::make_format_args(status_code, subjects_size));
    #else
    message = fmt::format("{{\"code\": {}, \"counts\": {}}}", status_code, subjects.size());
    #endif

    messages.reserve(subjects.size()+1);

    messages.emplace_back(std::forward<string>(message));

    // experimental
    subjects = helper(subjects, "subject name");
    messages.insert(messages.end(), make_move_iterator(subjects.begin()), make_move_iterator(subjects.end()));

    return messages;
}

vector<string> Server::getChapters(std::shared_ptr<question_bank> cur_question, string& subject){
    int status_code;
    string message;
    vector<string> messages;
    const string target_attribute = "chapter";
    optional<pair<string, variant<string, int, double>>> count_info;
    count_info = std::make_pair("subject", subject);
    int chapter_num = cur_question->countDistinct(target_attribute, count_info);
    if(chapter_num < 0){
        status_code = 403;
        #ifdef __cpp_lib_format
        message = std::vformat("{{\"code\": {}, \"counts\": {}}}", std::make_format_args(status_code, chapter_num));
        #else
        message = fmt::format("{{\"code\": {}, \"counts\": {}}}", status_code, chapter_num);
        #endif

        messages.emplace_back(std::forward<string>(message));
        return messages;
    }
    else status_code = 200;
    optional<pair<string, string>> constraint;
    constraint = std::make_pair("subject", subject);
    vector<string> chapters = cur_question->getQuestionAttributes(constraint, target_attribute);
    #ifdef __cpp_lib_format
    message = std::vformat("{{\"code\": {}, \"counts\": {}}}", std::make_format_args(status_code, chapter_num));
    #else
    message = fmt::format("{{\"code\": {}, \"counts\": {}}}", status_code, chapter_num);
    #endif

    messages.reserve(chapter_num+1);

    messages.emplace_back(std::forward<string>(message));

    // vectorization transform on chapters
    chapters = helper(chapters, "chapter name");
    messages.insert(messages.end(), make_move_iterator(chapters.begin()), make_move_iterator(chapters.end()));

    return messages;
}

vector<string> Server::addSubject(std::shared_ptr<question_bank> cur_question, string& subject) {
    int status_code;
    vector<string> messages;
    bool existence = false;
    {
        std::shared_lock<std::shared_mutex> lock(state_mutex);
        if(subject_cache.contains(subject)) {
            existence = true;
        }
    }
    if(!existence) {
        // optional<pair<string, variant<string, int, double>>> count_info;
        // count_info = std::make_pair("subject", subject);
        // const string target_attribute = "subject";
        // existence = question->countDistinct(target_attribute, count_info);
        vector<pair<string, string>> constraint_info;
        constraint_info.emplace_back(std::make_pair("subject", subject));
        existence = cur_question->checkExistence(constraint_info);

    }
    
    int rc;
    if(!existence) {
        // cout<<"Add a new subject to the question bank."<<endl;
        fmt::print("Add a new subject to the question bank.\n");
        const std::shared_ptr<QuestionInfo<string>> new_question = std::make_shared<QuestionInfo<string>>("placeholder", "placeholder", "placeholder", subject);
        rc = cur_question->insert(new_question);
        // delete new_question;
        if(rc < 0) status_code = 403;
        else {
            status_code = 200;
            std::unique_lock<std::shared_mutex> lock(state_mutex);
            subject_cache.insert(subject);
            if(subject_count_cache >= 0) subject_count_cache ++;
        } 
    } else {
        // cout<<"Subject already exists!"<<endl;
        fmt::print("Subject already exists!\n");
        status_code = 403;
    }
    #ifdef __cpp_lib_format
    string message = std::vformat("{{\"code\": {}}}", std::make_format_args(status_code));
    #else
    string message = fmt::format("{{\"code\": {}}}", status_code);
    #endif

    messages.emplace_back(std::forward<string>(message));
    return messages;
}

vector<string> Server::addChapter(std::shared_ptr<question_bank> cur_question, string& subject, string& chapter) {
    int status_code;
    vector<string> messages;
    const string target_attribute = "chapter";
    bool existence = false;
    {
        std::shared_lock<std::shared_mutex> lock(state_mutex);
        if(subject_cache.contains(subject)) {
            existence = true;
        }
    }
    if (!existence) {
        // optional<pair<string, variant<string, int, double>>> count_info;
        // count_info = std::make_pair("subject", subject);
        // existence = question->countDistinct(target_attribute, count_info);
        vector<pair<string, string>> constraint_info;
        constraint_info.emplace_back(std::make_pair("subject", subject));
        existence = cur_question->checkExistence(constraint_info);
    }

    if(existence) {
        bool chapter_exists = false;
        {
            std::shared_lock<std::shared_mutex> lock(state_mutex);
            if(chapter_cache.contains(subject) && chapter_cache[subject].contains(chapter)) {
                chapter_exists = true;
            }
        }
        if (!chapter_exists) {
            vector<pair<string, string>> count_infos{std::make_pair("subject", subject), std::make_pair("chapter", chapter)};
            // existence = question->countDistinct(target_attribute, count_infos);
            chapter_exists = cur_question->checkExistence(count_infos);
        }
        int rc;
        if(!chapter_exists) {
            // cout<<"Add a new chapter to the question bank."<<endl;
            fmt::print("Add a new chapter to the question bank.\n");
            const std::shared_ptr<QuestionInfo<string>> new_question = std::make_shared<QuestionInfo<string>>("placeholder", "placeholder", chapter, subject);
            rc = cur_question->insert(new_question);
            if(rc < 0) status_code = 403;
            else {
                status_code = 200;
                std::unique_lock<std::shared_mutex> lock(state_mutex);
                chapter_cache[subject].insert(chapter);
            }
        } else {
            // cout<<"Chapter already exists!"<<endl;
            fmt::print("Chapter already exists!\n");
            status_code = 403;
        }
    } else {
        // cout<<"Subject has not been created yet!"<<endl;
        fmt::print("Subject has not been created yet!\n");
        status_code = 403;
    }

    #ifdef __cpp_lib_format
    string message = std::vformat("{{\"code\": {}}}", std::make_format_args(status_code));
    #else
    string message = fmt::format("{{\"code\": {}}}", status_code);
    #endif

    messages.emplace_back(std::forward<string>(message));
    return messages;
}

vector<string> Server::getQuestions(std::shared_ptr<question_bank> cur_question, string& subject, string& chapter){
    int status_code;
    string message;
    vector<string> messages;
    const string target_attribute = "path";
    vector<pair<string, string>> count_infos{std::make_pair("subject", subject), std::make_pair("chapter", chapter)};
    int question_num = cur_question->countDistinct(target_attribute, count_infos);
    if(question_num < 0){
        status_code = 403;
        #ifdef __cpp_lib_format
        message = std::vformat("{{\"code\": {}, \"counts\": {}}}", std::make_format_args(status_code, question_num));
        #else
        message = fmt::format("{{\"code\": {}, \"counts\": {}}}", status_code, question_num);
        #endif

        messages.emplace_back(std::forward<string>(message));
        return messages;
    }
    else status_code = 200;

    vector<string> question_ids = cur_question->getQuestionAttributes(count_infos, target_attribute);
    #ifdef __cpp_lib_format
    const int questions_size = question_ids.size();
    message = std::vformat("{{\"code\": {}, \"counts\": {}}}", std::make_format_args(status_code, questions_size));
    #else
    message = fmt::format("{{\"code\": {}, \"counts\": {}}}", status_code, question_ids.size());
    #endif

    messages.reserve(question_ids.size()+1);

    messages.emplace_back(std::forward<string>(message));

    //experimental
    question_ids = helper(question_ids, "question name");
    messages.insert(messages.end(), make_move_iterator(question_ids.begin()), make_move_iterator(question_ids.end()));

    return messages;
}

vector<string> Server::getQuestions(std::shared_ptr<question_bank> cur_question, string& subject, string& chapter, string& question_id){
    int status_code;
    vector<string> messages;
    const string target_attribute = "content";
    optional<pair<string, variant<string, int, double>>> constraint;
    std::array<pair<string, string>, 3> primary_pairs{std::make_pair("subject", subject), std::make_pair("chapter", chapter), std::make_pair("path", question_id)};
    string question_content = cur_question->getQuestionAttribute(constraint, primary_pairs, target_attribute);

    status_code = 200;
    #ifdef __cpp_lib_format
    string message = std::vformat("{{\"code\": {}, \"question text\": \"{}\"}}", std::make_format_args(status_code, question_content));
    #else
    string message = fmt::format("{{\"code\": {}, \"question text\": \"{}\"}}", status_code, question_content);
    #endif

    messages.emplace_back(std::forward<string>(message));
    return messages;
}

vector<string> Server::writeQuestion(std::shared_ptr<question_bank> cur_question, string& subject, string& chapter, string& question_id, auto content){
    int status_code;
    vector<string> messages;
    bool existence;
    // check if the path exists
    vector<pair<string, string>> count_infos;
    count_infos.emplace_back(std::make_pair("subject", subject));
    count_infos.emplace_back(std::make_pair("chapter", chapter));
    count_infos.emplace_back(std::make_pair("path", question_id));

    const string target_attribute = "content";
    // int existence = question->countDistinct(target_attribute, count_infos);
    existence = cur_question->checkExistence(count_infos);
    int rc;
    content = escapeJsonString(content);
    if(!existence) {
        // Check if the subject and chapter can accept a new question
        count_infos.pop_back();
        // existence = question->countDistinct(target_attribute, count_infos);
        existence = cur_question->checkExistence(count_infos);
        if(existence) {
            // cout<<"Write a new question into the question bank!"<<endl;
            fmt::print("Write a new question into the question bank!\n");
            const std::shared_ptr<QuestionInfo<string>> new_question = std::make_shared<QuestionInfo<string>>(question_id, content, chapter, subject);
            rc = cur_question->insert(new_question);
            // delete new_question;
            if(rc < 0) status_code = 403;
            else status_code = 200;
        } else {
            // cout<<"Either subject or chapter has not been created yet!"<<endl;
            fmt::print("Either subject or chapter has not been created yet!\n");
            status_code = 403;
        }
    }
    else {
        // cout<<"Update an existing question!"<<endl;
        fmt::print("Update an existing question!\n");
        vector<pair<string, variant<string, int, double>>> changelist;
        changelist.emplace_back(std::make_pair("content", content));
        rc = cur_question->update(count_infos, changelist);
        if(rc < 0) status_code = 403;
        else status_code = 200;
    }

    #ifdef __cpp_lib_format
    string message = std::vformat("{{\"code\": {}}}", std::make_format_args(status_code));
    #else
    string message = fmt::format("{{\"code\": {}}}", status_code);
    #endif

    messages.emplace_back(std::forward<string>(message));
    return messages;
}

vector<string> Server::deleteQuestion(std::shared_ptr<question_bank> cur_question, string& subject, string& chapter, string& question_id){
    int status_code;
    vector<string> messages;
    vector<pair<string, string>> primary_pairs{std::make_pair("subject", subject), std::make_pair("chapter", chapter), std::make_pair("path", question_id)};
    int rc = cur_question->delet(primary_pairs);
    if(rc >= 0) status_code = 200;
    else status_code = 404;
    #ifdef __cpp_lib_format
    string message = std::vformat("{{\"code\": {}}}", std::make_format_args(status_code));
    #else
    string message = fmt::format("{{\"code\": {}}}", status_code);
    #endif

    messages.emplace_back(std::forward<string>(message));
    return messages;
}

void Server::run(std::stop_token st)
{
    while(!st.stop_requested()) {
        loop(100);
    }
    shutdown();
}

std::jthread Server::start_in_thread()
{
    return std::jthread([this](std::stop_token st) {
        this->run(st);
    });
}

void Server::loop(int timeout_ms)
{
    //no problems, we're all set
    int eNum = epoll_wait(eFd, events, EVENTS_SIZE, timeout_ms);
    if(eNum == -1) {
        if(errno == EINTR) return;
        return;
    }
    if(eNum == 0) {
        return; // timeout reached, allows cooperative cancellation check
    }

    // Handle new incoming connections on master socket first sequentially
    for (int i = 0; i < eNum; i++) {
        if(events[i].data.fd == mastersocket_fd) {
            if(events[i].events & EPOLLIN) {
                handleNewConnection();
            }
        }
    }

    int num_threads = min(max_concurrency, eNum);

    #pragma omp parallel for schedule(auto) num_threads(num_threads) 
    for (int i = 0; i < eNum; i++) {
        if(events[i].data.fd == mastersocket_fd) {
            continue;
        }

        // check if there is a potential disconnection
        if(events[i].events & EPOLLERR || events[i].events & EPOLLHUP) {
            int closed_fd = events[i].data.fd;
            epoll_ctl(eFd, EPOLL_CTL_DEL, closed_fd, nullptr);
            close(closed_fd);
            {
                std::unique_lock<std::shared_mutex> lock(state_mutex);
                auto it = bindUsername.find(closed_fd);
                if (it != bindUsername.end()) {
                    logined_users.erase(it->second);
                    bindUsername.erase(it);
                }
                bindIdentity.erase(closed_fd);
                archived_msg.erase(closed_fd);
            }
            // cout<<"Connection "<<events[i].data.fd<<" has been closed."<<endl;
            fmt::print("Connection {} has been closed.\n", static_cast<int>(closed_fd));
        } else if (events[i].events & EPOLLIN) {
            //exisiting connection has new data
            Connector connect_fd = Connector(events[i].data.fd);
            int thread_idx = omp_get_thread_num();
            // connect_fd.source_fd = i;
            auto [messages, target_connector] = recvInputFromExisting(users[thread_idx], questions[thread_idx], connect_fd);
            if(!messages.empty()){
                messages.shrink_to_fit();
                bool user_safe = users[thread_idx]->check_threadsafe();
                bool question_safe = questions[thread_idx]->check_threadsafe();
                if(!user_safe || !question_safe) fmt::print("Warning: database not thread-safe!\n");
                if (target_connector.getFd() != -1) {
                    sendMsgToExisting(target_connector, messages);
                }
            }
        }
    }
}

void Server::init()
{
    initializeSocket();
    bindSocket();
    startListen();
    for(int i=0; i<max_concurrency; i++) {
        users[i]->create();
        questions[i]->create();
    }
}

void Server::onInput(void (*rc)(uint16_t fd, char *buffer))
{
    receiveCallback = rc;
}

void Server::onConnect(void(*ncc)(uint16_t))
{
    newConnectionCallback = ncc;
}

void Server::onDisconnect(void(*dc)(uint16_t))
{
    disconnectCallback = dc;
}

int Server::sendMessage(Connector conn, char *messageBuffer) {
    return sendMessage(conn, static_cast<const char*>(messageBuffer));
}

int Server::sendMessage(Connector conn, const char *messageBuffer) {
    int fd = conn.getFd();
    int len = static_cast<int>(strlen(messageBuffer));
    int total_sent = 0;
    while (total_sent < len) {
        int sent = send(fd, messageBuffer + total_sent, len - total_sent, 0);
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                struct pollfd pfd{fd, POLLOUT, 0};
                int pr = poll(&pfd, 1, 10);
                if (pr > 0 && (pfd.revents & POLLOUT)) continue;
                return -1;
            }
            return -1;
        }
        total_sent += sent;
    }
    return total_sent;
}

int Server::recvMessage(Connector conn, char *messageBuffer){
    int ret = recv(conn.getFd(), messageBuffer, INPUT_BUFFER_SIZE - 1, 0);
    if (ret > 0) {
        messageBuffer[ret] = '\0';
    }
    return ret;
}

shared_ptr<Server> Server::server_ = nullptr;
std::mutex Server::mutex_;

shared_ptr<Server> Server::getInstance(int port) {
    // double checked locking
    if(server_ == nullptr) {
        // std::lock_guard<std::mutex> lock(mutex_);
        std::scoped_lock lock(mutex_);
        if(server_ == nullptr) server_ = shared_ptr<Server>(new Server(port));
    }
    return server_;
}

shared_ptr<Server> Server::getInstance() {
    // double checked locking
    if(server_ == nullptr) {
        // std::lock_guard<std::mutex> lock(mutex_);
        std::scoped_lock lock(mutex_);
        if(server_ == nullptr) server_ = shared_ptr<Server>(new Server());
    }
    return server_;
}


#ifndef SERVER_NO_MAIN
static std::atomic<bool> g_stop_requested{false};
inline void handle_sigint(int) {
    g_stop_requested.store(true);
}

int main(int argc, char* argv[]){
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = handle_sigint;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);
    sigaction(SIGTERM, &sigIntHandler, NULL);

    int port = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;
    shared_ptr<Server> server_object = Server::getInstance(port);
    server_object->init();

    std::println("[SERVER] Starting server worker thread via std::jthread on port {}...", port);
    std::jthread server_thread = server_object->start_in_thread();

    while(!g_stop_requested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::println("\n[SERVER] Signal received. Requesting cooperative stop...");
    server_thread.request_stop();
    // server_thread automatically joins via RAII destructor
    std::println("[SERVER] Server worker thread joined cleanly. Exiting.");
    return 0;
}
#endif