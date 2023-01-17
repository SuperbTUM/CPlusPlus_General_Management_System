#include "client.hpp"

int Client::read_iterative(char* ptr, int size){
    int left_bytes, read_bytes;
    left_bytes = size;
    while(left_bytes > 0){
        read_bytes = read(socket_fd, ptr, left_bytes);
        if(read_bytes < 0) return (read_bytes);
        if(read_bytes == 0) break;
        left_bytes -= read_bytes;
        ptr += read_bytes;
    }
    // everything in the buffer zone has been read into the ptr
    return (size - left_bytes);
}

int Client::write_iterative(char* ptr, int size){
    int left_bytes, written_bytes;
    left_bytes = size;
    while(left_bytes > 0){
        written_bytes = write(socket_fd, ptr, left_bytes);
        if(written_bytes < 0) return (written_bytes);
        if(written_bytes == 0) break;
        left_bytes -= written_bytes;
        ptr += written_bytes;
    }
    return (size - left_bytes);
}

int Client::recv_iterative(char* ptr, int size, int flag){
    int left_bytes, recv_bytes;
    left_bytes = size;
    while(left_bytes > 0){
        recv_bytes = recv(socket_fd, ptr, left_bytes, flag);
        if(recv_bytes < 0) return (recv_bytes);
        if(recv_bytes == 0) break;
        left_bytes -= recv_bytes;
        ptr += recv_bytes;
    }
    // everything in the buffer zone has been read into the ptr
    return (size - left_bytes);
}

int Client::send_iterative(char* ptr, int size, int flag){
    int left_bytes, sent_bytes;
    left_bytes = size;
    while(left_bytes > 0){
        sent_bytes = send(socket_fd, ptr, left_bytes, flag);
        printf("in send %s", ptr);
        if(sent_bytes < 0) return (sent_bytes);
        if(sent_bytes == 0) break;
        left_bytes -= sent_bytes;
        ptr += sent_bytes;
    }
    return (size - left_bytes);
}

Client::Client(string digital_certificate_path, string privateKey_path){
    setup(PORT, digital_certificate_path, privateKey_path);
}

Client::~Client(){
    #ifdef SERVER_DEBUG
	std::cout << "[SERVER] [DESTRUCTOR] Destroying Server...\n";
	#endif
    SSL_shutdown(ssl);
    SSL_free(ssl);
	close(socket_fd);
    SSL_CTX_free(ctx);
}

Client::Client(string digital_certificate_path, string privateKey_path, int port){
    setup(port, digital_certificate_path, privateKey_path);
}

Client::Client(const Client& orig){
    socket_fd = orig.socket_fd;
    num_bytes = orig.num_bytes;
    char buffer[256];
    strcpy(buffer, orig.buffer);
}

void Client::setup(int port, string digital_certificate_path, string privateKey_path){
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    ctx = SSL_CTX_new(SSLv23_client_method());
    if(ctx == NULL){
        ERR_print_errors_fp(stdout);
        exit(1);
    }
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);

    // root certificate location needs to be modified accordingly
    if (SSL_CTX_load_verify_locations(ctx, strcat(getenv("HOME"), "/ssl_server_client/ca/ca.crt"), NULL)<=0){
        ERR_print_errors_fp(stdout);
        exit(1);
    }
 
    if (SSL_CTX_use_certificate_file(ctx, digital_certificate_path, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stdout);
        exit(1);
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, privateKey_path, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stdout);
        exit(1);
    }

    if (!SSL_CTX_check_private_key(ctx)) {
        ERR_print_errors_fp(stdout);
        exit(1);
    }

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) 
        error("ERROR opening socket");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
}

uint16_t Client::sendMessage(Connector conn, const char *messageBuffer){
    return send(conn.source_fd, messageBuffer, strlen(messageBuffer), 0);
}

uint16_t Client::sendMessageSSL(SSL *ssl, char *messageBuffer){
    return SSL_write(ssl, messageBuffer, strlen(messageBuffer));
}

uint16_t Client::sendMessage(Connector conn, char *messageBuffer){
    return send(conn.source_fd, messageBuffer, strlen(messageBuffer), 0);
}

uint16_t Client::sendMessageSSL(SSL *ssl, const char *messageBuffer){
    return SSL_write(ssl, messageBuffer, strlen(messageBuffer));
}

uint16_t Client::recvMessage(Connector conn, char *messageBuffer){
    return recv(conn.source_fd, messageBuffer, INPUT_BUFFER_SIZE, 0);
}

uint16_t Client::recvMessageSSL(SSL *ssl, char *messageBuffer){
    return SSL_read(ssl, messageBuffer, INPUT_BUFFER_SIZE);
}


void Client::error(const char *msg)
{
    perror(msg);
    exit(0);
}

void Client::initConnect(hostent* server){
    if (server == NULL) {
    fprintf(stderr,"ERROR, no such host\n");
    exit(0);
    }
    cout<<"Start connecting!"<<endl;

    bcopy((char *)server->h_addr, 
        (char *)&serv_addr.sin_addr.s_addr,
        server->h_length);
    serv_addr.sin_port = htons(PORT);
    if (connect(socket_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR connecting");
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, socket_fd);
    ShowCerts(ssl);
}

void Client::loop(){
    
    struct Connector connect_fd = Connector();
    connect_fd.source_fd = socket_fd;
    
    // num_bytes = recvMessage(connect_fd, buffer);
    num_bytes = recvMessageSSL(ssl, buffer);
    printf("%s \n", buffer);
    if (num_bytes < 0) 
         error("ERROR reading from socket");
    bzero(buffer,256);

    string username = "\"admin\"";
    string password = "\"123456\"";
    string identity = "\"admin\"";
    string status = "\"valid\"";
    struct UserInfo user = {username, password, identity, status};

    string login_msg = fmt::format("{{\"command\": \"login\",\"username\": {},\"password\": {}}}", 
    user.username, user.password);
    cout<<"login msg"<<login_msg<<endl;
    // login_msg[strlen(login_msg)] = '\0';

    // num_bytes = sendMessage(connect_fd, login_msg.c_str());
    num_bytes = sendMessageSSL(ssl, login_msg.c_str());
    if (num_bytes < 0) 
         error("ERROR writing to socket");
    cout<<"Sent a login command!"<<endl;
    
    bzero(buffer, 256);
    // num_bytes = recv(socket_fd, buffer, 255, 0);
    num_bytes = recvMessageSSL(ssl, buffer);
    buffer[num_bytes] = '\0';
    cout<<"buffer: "<<buffer<<endl;

    json response = json::parse(buffer);
    auto response_code = response["code"];
    auto response_identity = response["identity"];
    cout<<"Response code: "<<response_code<<"\t"<<"Identity: "<<response_identity<<endl;
    
}
int main(int argc, char *argv[])
{   
    struct hostent *server = gethostbyname(argv[1]); // 34.139.226.174
    string digital_certificate_path = argv[2];
    string privateKey_path = argv[3];
    Client client = Client(digital_certificate_path, privateKey_path);
    client.initConnect(server);
    client.loop();
    return 0;
}