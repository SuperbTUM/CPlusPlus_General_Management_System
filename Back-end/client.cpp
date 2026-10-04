#include "client.hpp"

int Client::read_iterative(SSL* ssl, char* ptr, int size){
    int left_bytes, read_bytes;
    left_bytes = size;
    while(left_bytes > 0){
        read_bytes = SSL_read(ssl, ptr, left_bytes);
        if(read_bytes < 0) return (read_bytes);
        if(read_bytes == 0) break;
        left_bytes -= read_bytes;
        ptr += read_bytes;
    }
    // everything in the buffer zone has been read into the ptr
    return (size - left_bytes);
}

int Client::write_iterative(SSL* ssl, char* ptr, int size){
    int left_bytes, written_bytes;
    left_bytes = size;
    while(left_bytes > 0){
        written_bytes = SSL_write(ssl, ptr, left_bytes);
        if(written_bytes < 0) return (written_bytes);
        if(written_bytes == 0) break;
        left_bytes -= written_bytes;
        ptr += written_bytes;
    }
    return (size - left_bytes);
}

Client::Client(char* digital_certificate_path, char* privateKey_path){
    setup(PORT, digital_certificate_path, privateKey_path);
}

Client::~Client(){
    #ifdef SERVER_DEBUG
	std::cout << "[SERVER] [DESTRUCTOR] Destroying Server...\n";
	#endif
    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    close(socket_fd);
}

Client::Client(char* digital_certificate_path, char* privateKey_path, int port){
    setup(port, digital_certificate_path, privateKey_path);
}

void Client::setup(const int port, const char* digital_certificate_path, const char* privateKey_path){
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    ctx = SSL_CTX_new(TLS_client_method());
    if(ctx == NULL){
        ERR_print_errors_fp(stdout);
        exit(1);
    }
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);

    // root certificate location needs to be modified accordingly
    const char* home_env = getenv("HOME");
    string ca_path = (home_env ? home_env : ".") + string("/ssl_server_client/ca/ca.crt");
    if (SSL_CTX_load_verify_locations(ctx, ca_path.c_str(), NULL)<=0){
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
    int nodelay_opt = 1;
    setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, (char *)&nodelay_opt, sizeof(nodelay_opt));
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
}

int Client::sendMessage(Connector conn, const char *messageBuffer){
    return send(conn.source_fd, messageBuffer, strlen(messageBuffer), 0);
}

int Client::sendMessageSSL(SSL *ssl, char *messageBuffer){
    return SSL_write(ssl, messageBuffer, strlen(messageBuffer));
}

int Client::sendMessage(Connector conn, char *messageBuffer){
    return send(conn.source_fd, messageBuffer, strlen(messageBuffer), 0);
}

int Client::sendMessageSSL(SSL *ssl, const char *messageBuffer){
    return SSL_write(ssl, messageBuffer, strlen(messageBuffer));
}

int Client::recvMessage(Connector conn, char *messageBuffer){
    return recv(conn.source_fd, messageBuffer, INPUT_BUFFER_SIZE, 0);
}

int Client::recvMessageSSL(SSL *ssl, char *messageBuffer){
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
    int r;
    if ((r = connect(socket_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))) < 0)
        error("ERROR connecting");
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, socket_fd);
    SSL_set_connect_state(ssl);
    if(SSL_do_handshake(ssl) == -1) {
        ERR_print_errors_fp(stderr);
    } else {
        ShowCerts(ssl);
    }
}

void Client::loop(){
    
    struct Connector connect_fd = Connector();
    connect_fd.source_fd = socket_fd;
    
    // num_bytes = recvMessage(connect_fd, buffer);
    // num_bytes = recvMessageSSL(ssl, buffer);
    // printf("%s \n", buffer);
    // if (num_bytes < 0) 
    //      error("ERROR reading from socket");
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
    if (num_bytes >= 0 && num_bytes < 256) {
        buffer[num_bytes] = '\0';
    } else {
        buffer[255] = '\0';
    }
    cout<<"buffer: "<<buffer<<endl;

    glz::read<glz::opts{.error_on_unknown_keys = false}>(recv_struct, buffer);
    cout<<"Response code: "<<recv_struct.code<<"\t"<<"Identity: "<<recv_struct.identity<<endl;
    
}
int main(int argc, char *argv[])
{   
    if (argc < 4) {
        cerr << "Usage: " << argv[0] << " <server_host> <cert_path> <key_path>\n";
        return 1;
    }
    struct hostent *server = gethostbyname(argv[1]); // 34.139.226.174
    char* digital_certificate_path = argv[2];
    char* privateKey_path = argv[3];
    Client client = Client(digital_certificate_path, privateKey_path);
    client.initConnect(server);
    client.loop();
    return 0;
}