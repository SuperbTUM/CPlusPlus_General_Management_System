#include "server_unencrypted.hpp"
#include "user_info.cpp"
#include "question_bank.cpp"
#include <omp.h>
#include <utility>
using namespace std;

Server::Server()
{
    setup(DEFAULT_PORT);
}

Server::Server(int port)
{   
    setup(port);
}

Server::Server(const Server& orig)
{
    // masterfds = orig.masterfds;
    // tempfds = orig.tempfds;
    // maxfd = orig.maxfd;
    mastersocket_fd = orig.mastersocket_fd;
    tempsocket_fd = orig.tempsocket_fd;

    char input_buffer[INPUT_BUFFER_SIZE];
    strcpy(input_buffer, orig.input_buffer);
    char remote_ip[INET6_ADDRSTRLEN];
    strcpy(remote_ip, orig.remote_ip);
}

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

void Server::startListen()
{
	#ifdef SERVER_DEBUG
	std::cout << "[SERVER] listen starting...\n";
	#endif
	int listen_ret = listen(mastersocket_fd, 3);
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
}

void Server::handleNewConnection()
{
	#ifdef SERVER_DEBUG
  	std::cout << "[SERVER] [CONNECTION] handling new connection\n";
    #endif
    socklen_t addrlen = sizeof (client_addr);
    tempsocket_fd = accept(mastersocket_fd, (struct sockaddr*) &client_addr, &addrlen);
    	
	if (tempsocket_fd < 0) {
        	perror("[SERVER] [ERROR] accept() failed");
	} else {
        	// FD_SET(tempsocket_fd, &masterfds);
            epev.events = EPOLLIN | EPOLLET;
            epev.data.fd = tempsocket_fd;
            int flags = fcntl(tempsocket_fd, F_GETFL, 0);
            if(flags < 0 || fcntl(tempsocket_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
                cout<<"Set non-blocking error, fd: "<<tempsocket_fd<<endl;
                return;
            } 

            epoll_ctl(eFd, EPOLL_CTL_ADD, tempsocket_fd, &epev);
		//     //increment the maximum known file descriptor (select() needs it)
        // 	if (tempsocket_fd > maxfd) {
        //     		maxfd = tempsocket_fd;
		// 	#ifdef SERVER_DEBUG
        //     		std::cout << "[SERVER] incrementing maxfd to " << maxfd << std::endl;
		// 	#endif
        // 	}
        // 	#ifdef SERVER_DEBUG
        // 	printf("[SERVER] [CONNECTION] New connection on socket fd '%d'.\n",tempsocket_fd);
		// #endif
    }
    cout<<"Successfully connected!"<<endl;
    // // newConnectionCallback(tempsocket_fd); //call the callback
    // string message = "Successfully connected!";
    // struct Connector connect_fd = Connector();
    // connect_fd.source_fd = tempsocket_fd;
    // sendMessage(connect_fd, message.c_str());
}

void Server::sendMsgToExisting(Connector& connect_fd, vector<string>& messages){
    for(int i=0; i<messages.size(); i++){
        int bytes = sendMessage(connect_fd, messages[i].c_str());
        while(bytes < 0){
            bytes = sendMessage(connect_fd, messages[i].c_str());
        }
        usleep(100000);
    }

}

vector<string> Server::recvInputFromExisting(Connector& connect_fd)
{
    vector<string> messages;
    int nbytesrecv = recvMessage(connect_fd, input_buffer);
    // int nbytesrecv = recv(fd, input_buffer, INPUT_BUFFER_SIZE, 0);
    cout<<"Received bytes: "<<nbytesrecv<<endl;
    if (nbytesrecv <= 0)
    {
        //problem
        if (nbytesrecv < 0)
	    {   
            perror("[SERVER] [ERROR] recv() failed");
        	//disconnectCallback((uint16_t)fd);
        }
        close(connect_fd.source_fd); //close connection to client
        // FD_CLR(connect_fd.source_fd, &masterfds); //clear the client fd from fd set
        return messages;
    }
    #ifdef SERVER_DEBUG
    printf("[SERVER] [RECV] Received '%s' from client!\n", input_buffer);
    #endif
    // receiveCallback(fd,input_buffer);
    // authenticate the identity of the user
    json message = json::parse(input_buffer);
    cout<<message.dump()<<endl;
    auto command = message["command"].get<std::string>();
    string username = "";
    auto password = std::string();
    string identity = "";
    // string status = "";
    string subject_name = "";
    string chapter_name = "";
    string question_id = "";
    auto question_content = std::string();

    if(message.contains("username")) username = message["username"].get<std::string>();
    else cout<<"No account.\n";

    if(message.contains("password")) password = message["password"].get<std::string>();
    else cout<<"No password.\n";
    if(message.contains("subject name")) subject_name = message["subject name"].get<std::string>();
    if(message.contains("chapter name")) chapter_name = message["chapter name"].get<std::string>();
    if(message.contains("question name")) question_id = message["question name"].get<std::string>();
    if(message.contains("question text")) question_content = message["question text"].get<std::string>();

    if(command == "login"){
        messages = authenticateUser(connect_fd, username, password);
    }
    else if(command == "get users" && 
            bindIdentity.find(connect_fd.source_fd) != bindIdentity.end() && 
            bindIdentity[connect_fd.source_fd] == "admin"){
        messages = getUser(connect_fd);
    }
    else if(command == "register user"){
        if(message.contains("identity")) identity = message["identity"].get<std::string>();
        else{
            perror("No identity.\n");
            // exit(1);
        }
        messages = registerUser(connect_fd, username, password, identity);
    }
    else if(command == "delete user" && bindIdentity.find(connect_fd.source_fd) != bindIdentity.end()){
        if(bindIdentity[connect_fd.source_fd] == "admin") messages = deleteUser(connect_fd, username);
        else messages = deleteUserSelf(connect_fd, password);
    }
    else if(command == "logout" && bindUsername.find(connect_fd.source_fd) != bindUsername.end()) {
        messages = logout(connect_fd);
    }
    else if(command == "get teachers" && bindIdentity.find(connect_fd.source_fd) != bindIdentity.end() && bindIdentity[connect_fd.source_fd] == "rule maker") {
        messages = getTeachers();
    }
    else if(command == "get subjects" && bindUsername.find(connect_fd.source_fd) != bindUsername.end()) {
        messages = getSubjects();
    }
    else if(command == "get chapters" && bindUsername.find(connect_fd.source_fd) != bindUsername.end()) {
        messages = getChapters(subject_name);
    }
    else if(command == "get questions" && bindUsername.find(connect_fd.source_fd) != bindUsername.end()) {
        messages = getQuestions(subject_name, chapter_name);
    }
    else if(command == "read question" && bindUsername.find(connect_fd.source_fd) != bindUsername.end()) {
        messages = getQuestions(subject_name, chapter_name, question_id);
    }
    else if(command == "write question" && bindUsername.find(connect_fd.source_fd) != bindUsername.end()) {
        messages = writeQuestion(subject_name, chapter_name, question_id, question_content);
    }
    else if(command == "delete question" && bindUsername.find(connect_fd.source_fd) != bindUsername.end()) {
        messages = deleteQuestion(subject_name, chapter_name, question_id);
    }
    else if(command == "write subject" && bindIdentity.find(connect_fd.source_fd) != bindIdentity.end() && bindIdentity[connect_fd.source_fd] == "teacher") {
        messages = addSubject(subject_name);
    }
    else if(command == "write chapter" && bindIdentity.find(connect_fd.source_fd) != bindIdentity.end() && bindIdentity[connect_fd.source_fd] == "teacher") {
        messages = addChapter(subject_name, chapter_name);
    }
    else{
        cout<<"Invalid command or not enough permission."<<endl;
        #ifdef __cpp_lib_format
        message = std::format("{\"code\": {}}", 403);
        #else
        message = fmt::format("{{\"code\": {}}}", 403);
        #endif
        // messages.push_back(message);
        messages.push_back(std::move(message));
    } 
    //memset(&input_buffer, 0, INPUT_BUFFER_SIZE); //zero buffer //bzero
    // bzero(&input_buffer,INPUT_BUFFER_SIZE); //clear input buffer
    return messages;
}

vector<string> Server::authenticateUser(Connector& connect_fd, string username, auto password){
    int status_code;
    // with database logic
    optional<pair<string, variant<string, int, double>>> constraint;
    string key = "password";
    string target_attribute = "identity";
    constraint = std::make_pair(key, password);
    string identity = user->getUserAttribute(constraint, username, target_attribute);
    if(!identity.empty()){
        target_attribute = "activity";
        int activity = stoi(user->getUserAttribute(constraint, username, target_attribute));
        if(activity){
            cout<<"User already login! Logout from previous device and re-login!"<<endl;
            int logout_status = logout(username);
        }
        status_code = 200;
        activity = 1;
        const string primary_val = std::as_const(username);
        vector<pair<string, variant<string, int, double>>> changelist;
        changelist.emplace_back("activity", activity);
        user->update(primary_val, changelist);

    }
    else{
        status_code = 404;
        cout<<"Wrong authentication!"<<endl;
    }
    vector<string> messages;
    #ifdef __cpp_lib_format
    message = std::format("{\"code\": {}, \"identity\": \"{}\"}", status_code, identity);
    #else
    message = fmt::format("{{\"code\": {}, \"identity\": \"{}\"}}", status_code, identity);
    #endif
    cout<<"checkin message: "<<message<<endl;
    // messages.push_back(message);
    messages.push_back(std::move(message));
    
    bindIdentity[connect_fd.source_fd] = identity;
    bindUsername[connect_fd.source_fd] = username;
    logined_users[username] = connect_fd.source_fd;
    return messages;
}

vector<string> Server::registerUser(Connector& connect_fd, string username, auto password, string identity){
    int status_code;
    // with database logic
    std::shared_ptr<UserInfo<string>> new_user = std::make_shared<UserInfo<string>>(username, static_cast<std::string>(password), identity, "valid");
    int result = user->insert(new_user);
    // delete new_user;
    if(result == -1) status_code = 403;
    else status_code = 200;
    vector<string> messages;
    #ifdef __cpp_lib_format
    message = std::format("{\"code\": {}}", status_code);
    #else
    message = fmt::format("{{\"code\": {}}}", status_code);
    #endif
    // messages.push_back(message);
    messages.push_back(std::move(message));
    usernameSet.insert(username);
    return messages;
}


vector<string> Server::logout(Connector& connect_fd){
    int status_code;
    int activity_updated = 0;
    string username = bindUsername[connect_fd.source_fd];
    vector<pair<string, variant<string, int, double>>> constraint;
    constraint.emplace_back("activity", activity_updated);
    int res = user->update(std::as_const(username), constraint);
    if(res < 0){
        cout<<"logout failed."<<endl;
        status_code = 403;
    }
    else {
        bindUsername.erase(connect_fd.source_fd);
        bindIdentity.erase(connect_fd.source_fd);
        logined_users.erase(username);
        status_code = 200;
    }
    vector<string> messages;
    #ifdef __cpp_lib_format
    message = std::format("{\"code\": {}}", status_code);
    #else
    message = fmt::format("{{\"code\": {}}}", status_code);
    #endif
    // messages.push_back(message);
    messages.push_back(std::move(message));
    return messages;
}

int Server::logout(string username){
    int source_fd = logined_users[username];
    vector<pair<string, variant<string, int, double>>> constraint;
    constraint.emplace_back("activity", 0);
    int res = user->update(std::as_const(username), constraint);
    if(res < 0){
        cout<<"logout failed."<<endl;
    } else {
        auto it = logined_users.find(username);
        if(it != logined_users.end()){
            logined_users.erase(it);
        }
    }
    cout<<"Logout from other device successfully!"<<endl;
    bindIdentity.erase(source_fd);
    bindUsername.erase(source_fd);
    return res;
}

vector<string> Server::getUser(Connector& connect_fd){
    vector<string> usernames;
    int status_code;
    // with database logic
    int numUsers = user->count();
    if(numUsers < 0) status_code = 403;
    else status_code = 200;

    vector<string> messages;
    #ifdef __cpp_lib_format
    message = std::format("{\"code\": {}, \"counts\": {}}", status_code, numUsers);
    #else
    message = fmt::format("{{\"code\": {}, \"counts\": {}}}", status_code, numUsers);
    #endif

    messages.reserve(numUsers+1);
    // messages.push_back(message);
    messages.push_back(std::move(message));
    if(numUsers < 0) return messages;
    optional<pair<string, string>> constraint;
    usernames = user->getUserAttributes(constraint, "USERNAME");

    for(int i=0; i<usernames.size(); i++){
        message = fmt::format("{{\"username\": \"{}\"}}", usernames[i]);
        // messages.push_back(message);
        messages.push_back(std::move(message));
    }
    return messages;
}

vector<string> Server::deleteUser(Connector& connect_fd, string username){
    int status_code = 200;

    auto un = usernameSet.find(username);
    if(un != usernameSet.end()) {
        status_code = 200;
        usernameSet.erase(un);
    } else {
        status_code = 403;
    }
    
    // with database logic
    string key = "status";
    pair<string, variant<string, int, double>> deleted_detail;
    deleted_detail = std::make_pair(key, "valid");
    int result = user->delet(std::as_const(username), deleted_detail);
    if(result == -1 && status_code == 200) status_code = 404;
    else if(result >= 0) status_code = 200;

    vector<string> messages;
    #ifdef __cpp_lib_format
    message = std::format("{\"code\": {}}", status_code);
    #else
    message = fmt::format("{{\"code\": {}}}", status_code);
    #endif
    // messages.push_back(message);
    messages.push_back(std::move(message));
    return messages;
}

vector<string> Server::deleteUserSelf(Connector& connect_fd, auto password){
    string username = bindUsername[connect_fd.source_fd];
    int status_code;

    auto identity_iter = bindIdentity.find(connect_fd.source_fd);
    if(identity_iter == bindIdentity.end()){
        status_code = 403;
        cout<<"Identity not found!"<<endl;
    } else {
        status_code = 200;
        cout<<"Identity found!"<<endl;
        bindIdentity.erase(identity_iter);
        // bindUsername.erase(it);
    }

    auto username_iter = bindUsername.find(connect_fd.source_fd);
    if(username_iter == bindUsername.end()){
        status_code = 403;
        cout<<"Username not found!"<<endl;
    } else {
        status_code = 200;
        cout<<"Username found!"<<endl;
        // bindIdentity.erase(it);
        bindUsername.erase(username_iter);
    }

    // with database logic
    string key = "password";
    pair<string, variant<string, int, double>> deleted_detail = std::make_pair(key, password);
    int result = user->delet(std::as_const(username), deleted_detail);
    if(result == -1 && status_code == 200) status_code = 404;
    else if(result >= 0) status_code = 200;

    vector<string> messages;
    #ifdef __cpp_lib_format
    message = std::format("{\"code\": {}}", status_code);
    #else
    message = fmt::format("{{\"code\": {}}}", status_code);
    #endif
    // messages.push_back(message);
    messages.push_back(std::move(message));
    return messages;
}

vector<string> Server::getTeachers(){
    int status_code;
    vector<pair<string, string>> constraint;
    // constraint.push_back(std::make_pair("ACTIVITY", "1"));
    constraint.push_back(std::make_pair("IDENTITY", "teacher"));
    vector<string> teachers = user->getUserAttributes(constraint, "USERNAME");
    if(teachers.empty()) status_code = 403;
    else status_code = 200; 
    vector<string> messages;
    #ifdef __cpp_lib_format
    message = std::format("{\"code\": {}, \"counts\": {}}", status_code, teachers.size());
    #else
    message = fmt::format("{{\"code\": {}, \"counts\": {}}}", status_code, teachers.size());
    #endif

    messages.reserve(teachers.size()+1);
    // messages.push_back(message);
    messages.push_back(std::move(message));

    //experimental
    #pragma omp parallel for num_threads(4)
    for(int i=0; i<teachers.size(); i++) {
        #pragma omp critical
        messages.push_back(fmt::format("{{\"username\": \"{}\"}}", teachers[i]));
    }
    return messages;
}

vector<string> Server::getSubjects(){
    int status_code;
    vector<string> messages;
    const string target_attribute = "subject";
    optional<pair<string, variant<string, int, double>>> count_info;
    int subject_num = question->countDistinct(target_attribute, count_info);
    if(subject_num < 0){
        status_code = 403;
        #ifdef __cpp_lib_format
        message = std::format("{\"code\": {}, \"counts\": {}}", status_code, subject_num);
        #else
        message = fmt::format("{{\"code\": {}, \"counts\": {}}}", status_code, subject_num);
        #endif
        // messages.push_back(message);
        messages.push_back(std::move(message));
        return messages;
    }
    else status_code = 200;
    optional<pair<string, string>> constraint;
    vector<string> subjects = question->getQuestionAttributes(constraint, target_attribute);
    #ifdef __cpp_lib_format
    message = std::format("{\"code\": {}, \"counts\": {}}", status_code, subjects.size());
    #else
    message = fmt::format("{{\"code\": {}, \"counts\": {}}}", status_code, subjects.size());
    #endif

    messages.reserve(subjects.size()+1);
    // messages.push_back(message);
    messages.push_back(std::move(message));

    for(int i=0; i<subject_num; i++){
        #ifdef __cpp_lib_format
        message = std::format("{\"subject name\": \"{}\"}", subjects[i]);
        #else
        message = fmt::format("{{\"subject name\": \"{}\"}}", subjects[i]);
        #endif
        // messages.push_back(message);
        messages.push_back(std::move(message));
    }
    return messages;
}

vector<string> Server::getChapters(string subject){
    int status_code;
    vector<string> messages;
    const string target_attribute = "chapter";
    optional<pair<string, variant<string, int, double>>> count_info;
    count_info = std::make_pair("subject", subject);
    int chapter_num = question->countDistinct(target_attribute, count_info);
    if(chapter_num < 0){
        status_code = 403;
        #ifdef __cpp_lib_format
        message = std::format("{\"code\": {}, \"counts\": {}}", status_code, chapter_num);
        #else
        message = fmt::format("{{\"code\": {}, \"counts\": {}}}", status_code, chapter_num);
        #endif
        // messages.push_back(message);
        messages.push_back(std::move(message));
        return messages;
    }
    else status_code = 200;
    optional<pair<string, string>> constraint;
    constraint = std::make_pair("subject", subject);
    vector<string> chapters = question->getQuestionAttributes(constraint, target_attribute);
    #ifdef __cpp_lib_format
    message = std::format("{\"code\": {}, \"counts\": {}}", status_code, chapter_num);
    #else
    message = fmt::format("{{\"code\": {}, \"counts\": {}}}", status_code, chapter_num);
    #endif

    messages.reserve(chapter_num+1);
    // messages.push_back(message);
    messages.push_back(std::move(message));

    for(int i=0; i<chapter_num; i++){
        #ifdef __cpp_lib_format
        message = std::format("{\"chapter name\": \"{}\"}", chapters[i]);
        #else
        message = fmt::format("{{\"chapter name\": \"{}\"}}", chapters[i]);
        #endif
        // messages.push_back(message);
        messages.push_back(std::move(message));
    }
    return messages;
}

vector<string> Server::addSubject(string subject) {
    int status_code;
    vector<string> messages;
    optional<pair<string, variant<string, int, double>>> count_info;
    count_info = std::make_pair("subject", subject);

    const string target_attribute = "subject";
    int existence = question->countDistinct(target_attribute, count_info);
    int rc;
    if(existence < 0) status_code = 404;
    else if(existence == 0) {
        cout<<"Add a new subject to the question bank."<<endl;
        std::shared_ptr<QuestionInfo<string>> new_question = std::make_shared<QuestionInfo<string>>("placeholder", "placeholder", "placeholder", subject);
        rc = question->insert(new_question);
        // delete new_question;
        if(rc < 0) status_code = 403;
        else status_code = 200;
    } else {
        cout<<"Subject already exists!"<<endl;
        status_code = 403;
    }
    #ifdef __cpp_lib_format
    message = std::format("{\"code\": {}}", status_code);
    #else
    message = fmt::format("{{\"code\": {}}}", status_code);
    #endif
    // messages.push_back(message);
    messages.push_back(std::move(message));
    return messages;
}

vector<string> Server::addChapter(string subject, string chapter) {
    int status_code;
    vector<string> messages;
    const string target_attribute = "chapter";

    optional<pair<string, variant<string, int, double>>> count_info;
    count_info = std::make_pair("subject", subject);
    int existence = question->countDistinct(target_attribute, count_info);
    if(existence > 0) {
        vector<pair<string, string>> count_infos{std::make_pair("subject", subject), std::make_pair("chapter", chapter)};
        existence = question->countDistinct(target_attribute, count_infos);
        int rc;
        if(existence < 0) status_code = 404;
        else if(existence == 0) {
            cout<<"Add a new chapter to the question bank."<<endl;
            std::shared_ptr<QuestionInfo<string>> new_question = std::make_shared<QuestionInfo<string>>("placeholder", "placeholder", chapter, subject);
            rc = question->insert(new_question);
            // delete new_question;
            if(rc < 0) status_code = 403;
            else status_code = 200;
        } else {
            cout<<"Chapter already exists!"<<endl;
            status_code = 403;
        }
    } else {
        cout<<"Subject has not been created yet!"<<endl;
        status_code = 403;
    }

    #ifdef __cpp_lib_format
    message = std::format("{\"code\": {}}", status_code);
    #else
    message = fmt::format("{{\"code\": {}}}", status_code);
    #endif
    // messages.push_back(message);
    messages.push_back(std::move(message));
    return messages;
}

vector<string> Server::getQuestions(string subject, string chapter){
    int status_code;
    vector<string> messages;
    const string target_attribute = "path";
    vector<pair<string, string>> count_infos{std::make_pair("subject", subject), std::make_pair("chapter", chapter)};
    int question_num = question->countDistinct(target_attribute, count_infos);
    if(question_num < 0){
        status_code = 403;
        #ifdef __cpp_lib_format
        message = std::format("{\"code\": {}, \"counts\": {}}", status_code, question_num);
        #else
        message = fmt::format("{{\"code\": {}, \"counts\": {}}}", status_code, question_num);
        #endif
        // messages.push_back(message);
        messages.push_back(std::move(message));
        return messages;
    }
    else status_code = 200;
    // vector<pair<string, string>> constraints;
    // constraints.push_back(std::make_pair("subject", subject));
    // constraints.push_back(std::make_pair("chapter", chapter));
    vector<string> question_ids = question->getQuestionAttributes(count_infos, target_attribute);
    #ifdef __cpp_lib_format
    message = std::format("{\"code\": {}, \"counts\": {}}", status_code, question_ids.size());
    #else
    message = fmt::format("{{\"code\": {}, \"counts\": {}}}", status_code, question_ids.size());
    #endif

    messages.reserve(question_ids.size()+1);
    // messages.push_back(message);
    messages.push_back(std::move(message));

    for(int i=0; i<question_num; i++){
        
        #ifdef __cpp_lib_format
        message = std::format("{\"question name\": \"{}\"}", question_ids[i]);
        #else
        message = fmt::format("{{\"question name\": \"{}\"}}", question_ids[i]);
        #endif
        // messages.push_back(message);
        messages.push_back(std::move(message));
        
    }
    return messages;
}

vector<string> Server::getQuestions(string subject, string chapter, string question_id){
    int status_code;
    vector<string> messages;
    string target_attribute = "content";
    optional<pair<string, variant<string, int, double>>> constraint;
    vector<pair<string, string>> primary_pairs{std::make_pair("subject", subject), std::make_pair("chapter", chapter), std::make_pair("path", question_id)};
    string question_content = question->getQuestionAttribute(constraint, primary_pairs, target_attribute);

    status_code = 200;
    #ifdef __cpp_lib_format
    message = std::format("{\"code\": {}, \"question text\": \"{}\"}", status_code, question_content);
    #else
    message = fmt::format("{{\"code\": {}, \"question text\": \"{}\"}}", status_code, question_content);
    #endif

    // messages.push_back(message);
    messages.push_back(std::move(message));
    return messages;
}

vector<string> Server::writeQuestion(string subject, string chapter, string question_id, auto content){
    int status_code;
    vector<string> messages;
    // check if the path exists
    vector<pair<string, string>> count_infos;
    count_infos.push_back(std::make_pair("subject", subject));
    count_infos.push_back(std::make_pair("chapter", chapter));
    count_infos.push_back(std::make_pair("path", question_id));

    const string target_attribute = "content";
    int existence = question->countDistinct(target_attribute, count_infos);
    int rc;
    content = escapeJsonString(content);
    if(existence < 0) status_code = 404;
    else if(existence == 0) {
        // Check if the subject and chapter can accept a new question
        count_infos.pop_back();
        existence = question->countDistinct(target_attribute, count_infos);
        if(existence > 0) {
            cout<<"Write a new question into the question bank!"<<endl;
            std::shared_ptr<QuestionInfo<string>> new_question = std::make_shared<QuestionInfo<string>>(question_id, content, chapter, subject);
            rc = question->insert(new_question);
            // delete new_question;
            if(rc < 0) status_code = 403;
            else status_code = 200;
        } else {
            cout<<"Either subject or chapter has not been created yet!"<<endl;
            status_code = 403;
        }
    }
    else {
        cout<<"Update an existing question!"<<endl;
        vector<pair<string, variant<string, int, double>>> changelist;
        changelist.push_back(std::make_pair("content", content));
        rc = question->update(count_infos, changelist);
        if(rc < 0) status_code = 403;
        else status_code = 200;
    }

    #ifdef __cpp_lib_format
    message = std::format("{\"code\": {}}", status_code);
    #else
    message = fmt::format("{{\"code\": {}}}", status_code);
    #endif
    // messages.push_back(message);
    messages.push_back(std::move(message));
    return messages;
}

vector<string> Server::deleteQuestion(string subject, string chapter, string question_id){
    int status_code;
    vector<string> messages;
    vector<pair<string, string>> primary_pairs{std::make_pair("subject", subject), std::make_pair("chapter", chapter), std::make_pair("path", question_id)};
    int rc = question->delet(primary_pairs);
    if(rc >= 0) status_code = 200;
    else status_code = 404;
    #ifdef __cpp_lib_format
    message = std::format("{\"code\": {}}", status_code);
    #else
    message = fmt::format("{{\"code\": {}}}", status_code);
    #endif

    // messages.push_back(message);
    messages.push_back(std::move(message));
    return messages;
}

void Server::loop()
{
    // tempfds = masterfds; //copy fd_set for select()
    // #ifdef SERVER_DEBUG
    // printf("[SERVER] [MISC] max fd = '%hu' \n", maxfd);
    // std::cout << "[SERVER] [MISC] calling select()\n";
    // #endif
    // int sel = select(maxfd + 1, &tempfds, NULL, NULL, NULL); //blocks until activity
    // //printf("[SERVER] [MISC] select() ret %d, processing...\n", sel);
    // if (sel < 0) {
    //     perror("[SERVER] [ERROR] select() failed");
    //     shutdown();
    // }
    // cout<<"sel: "<<sel<<endl;

    //no problems, we're all set
    int eNum = epoll_wait(eFd, events, EVENTS_SIZE, -1);
    if(eNum == -1) {cout<<"epoll wait"<<endl; return;}

    //loop the fd_set and check which socket has interactions available
    for (int i = 0; i <= eNum; i++) {
        //if (FD_ISSET(i, &tempfds)) { //if the socket has activity pending
        if(events[i].data.fd == mastersocket_fd) {
            //if (mastersocket_fd == i) {
            if(events[i].events & EPOLLIN) {
                //new connection on master socket
                handleNewConnection();
            }
        }
        else {
            // check if there is a potential disconnection
            if(events[i].events & EPOLLERR || events[i].events & EPOLLHUP) {
                epoll_ctl(eFd, EPOLL_CTL_DEL, events[i].data.fd, nullptr);
                close(events[i].data.fd);
            } else if (events[i].events & EPOLLIN) {
                //exisiting connection has new data
                Connector connect_fd = Connector();
                // connect_fd.source_fd = i;
                connect_fd.source_fd = events[i].data.fd;
                vector<string> messages = recvInputFromExisting(connect_fd);
                if(!messages.empty()){
                    messages.shrink_to_fit();
                    sendMsgToExisting(connect_fd, messages);
                    bzero(&input_buffer,INPUT_BUFFER_SIZE); //clear input buffer
                }
            }
                
        } //loop on to see if there is more
    }
}

void Server::init()
{
    // create/open databases
    user->create();
    question->create();
    initializeSocket();
    bindSocket();
    startListen();
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

uint16_t Server::sendMessage(Connector conn, char *messageBuffer) {
    return send(conn.source_fd, messageBuffer, strlen(messageBuffer), 0);
}

uint16_t Server::sendMessage(Connector conn, const char *messageBuffer) {
    return send(conn.source_fd, messageBuffer, strlen(messageBuffer), 0);
}

uint16_t Server::recvMessage(Connector conn, char *messageBuffer){
    return recv(conn.source_fd, messageBuffer, INPUT_BUFFER_SIZE, 0);
}



int main(int argc, char* argv[]){
    Server server_object = Server();
    // db_user user = db_user();
    // user.create(false);
    server_object.init();
    while(true){
        server_object.loop();
    }
    return 0;
}