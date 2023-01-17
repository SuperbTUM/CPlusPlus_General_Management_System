//Example code: A simple server side code, which echos back the received message.
//Handle multiple socket connections with select and fd_set on Linux 
#include <stdio.h> 
#include <string.h>   //strlen 
#include <stdlib.h> 
#include <errno.h> 
#include <unistd.h>   //close 
#include <arpa/inet.h>    //close 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros 
#include <nlohmann/json.hpp>
using json = nlohmann::json;
using namespace std;
     
#define TRUE   1 
#define FALSE  0 
#define PORT 9999


int read_iterative(int sd, char* ptr, int size){
    int left_bytes, read_bytes;
    left_bytes = size;
    while(left_bytes > 0){
        read_bytes = read(sd, ptr, left_bytes);
        if(read_bytes < 0) return (read_bytes);
        if(read_bytes == 0) break;
        left_bytes -= read_bytes;
        ptr += read_bytes;
    }
    // everything in the buffer zone has been read into the ptr
    return (size - left_bytes);
}

int write_iterative(int sd, char* ptr, int size){
    int left_bytes, written_bytes;
    left_bytes = size;
    while(left_bytes > 0){
        written_bytes = write(sd, ptr, left_bytes);
        if(written_bytes < 0) return (written_bytes);
        if(written_bytes == 0) break;
        left_bytes -= written_bytes;
        ptr += written_bytes;
    }
    return (size - left_bytes);
}

int recv_iterative(int sd, char* ptr, int size, int flag){
    int left_bytes, recv_bytes;
    left_bytes = size;
    while(left_bytes > 0){
        recv_bytes = recv(sd, ptr, left_bytes, flag);
        if(recv_bytes <= 0) break;
        left_bytes -= recv_bytes;
        ptr += recv_bytes;
    }
    // everything in the buffer zone has been read into the ptr
    return (size - left_bytes + recv_bytes);
}

int send_iterative(int sd, char* ptr, int size, int flag){
    int left_bytes, sent_bytes;
    left_bytes = size;
    while(left_bytes > 0){
        sent_bytes = send(sd, ptr, left_bytes, flag);
        printf("in send %s", ptr);
        if(sent_bytes < 0) return (sent_bytes);
        if(sent_bytes == 0) break;
        left_bytes -= sent_bytes;
        ptr += sent_bytes;
    }
    return (size - left_bytes);
}


int main(int argc , char *argv[])  
{  
    int opt = TRUE;  
    int master_socket , addrlen , new_socket , client_socket[30] , 
          max_clients = 30 , activity, i , valread , sd;  
    int max_sd;  
    struct sockaddr_in address;  
         
    char buffer[1025];  //data buffer of 1K 
         
    //set of socket descriptors 
    fd_set readfds;  
         
    //a message 
    char message[] = "ECHO Daemon v1.0 \r\n";  
     
    // initialise all client_socket[] to 0 so not checked 
    // file descriptor -> identify client
    for (i = 0; i < max_clients; i++)  
    {  
        client_socket[i] = 0;  
    }  
         
    // create a master socket 
    // AF_INET: for internet
    // SOCK_STREAM: TCP connection
    // master_socket: remember the server
    if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0)  
    {  
        perror("socket failed");  
        exit(EXIT_FAILURE);  
    }  
     
    //set master socket to allow multiple connections , 
    //this is just a good habit, it will work without this 
    if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, 
          sizeof(opt)) < 0 )  
    {  
        perror("setsockopt");  
        exit(EXIT_FAILURE);  
    }  
     
    //type of socket created 
    address.sin_family = AF_INET;  
    address.sin_addr.s_addr = INADDR_ANY;  //localhost: 127.0.0.1
    address.sin_port = htons(PORT);  
         
    //bind the socket to localhost port 9999 
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0)  
    {  
        perror("bind failed");  
        exit(EXIT_FAILURE);  
    }  
    printf("Listener on port %d \n", PORT);  
         
    //try to specify maximum of 3 pending connections for the master socket 
    if (listen(master_socket, 3) < 0)  
    {  
        perror("listen");  
        exit(EXIT_FAILURE);  
    }  
         
    //accept the incoming connection 
    addrlen = sizeof(address);  
    puts("Waiting for connections ...");  

    while(TRUE)  
    {  
        //clear the socket set 
        FD_ZERO(&readfds);  
        //add master socket to set 
        FD_SET(master_socket, &readfds);  
        max_sd = master_socket;  
             
        //add child sockets to set 
        for ( i = 0 ; i < max_clients ; i++)  
        {  
            //socket descriptor 
            sd = client_socket[i];  
                 
            //if valid socket descriptor then add to read list 
            if(sd > 0)  
                FD_SET( sd , &readfds);  
                 
            //highest file descriptor number, need it for the select function 
            if(sd > max_sd)  
                max_sd = sd;  
        }  
     
        //wait for an activity on one of the sockets , timeout is NULL , 
        //so wait indefinitely 
        activity = select(max_sd + 1 , &readfds , NULL , NULL , NULL);  
       
        if ((activity < 0) && (errno!=EINTR))  
        {  
            printf("select error");  
        }  
             
        //If something happened on the master socket , 
        //then its an incoming connection 
        if (FD_ISSET(master_socket, &readfds))  
        {  
            if ((new_socket = accept(master_socket, 
                    (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)  
            {  
                perror("accept");  
                exit(EXIT_FAILURE);  
            }  
             
            //inform user of socket number - used in send and receive commands 
            printf("New connection , socket fd is %d , ip is : %s , port : %d \n" ,new_socket , inet_ntoa(address.sin_addr), ntohs(address.sin_port));
           
            //send new connection greeting message 
            if(send_iterative(new_socket, message, strlen(message), 0) != static_cast<int>(strlen(message))) 
            {  
                perror("send");  
            }  
            
            puts("Welcome message sent successfully");  
                 
            //add new socket to array of sockets 
            for (i = 0; i < max_clients; i++)  
            {  
                //if position is empty 
                if(client_socket[i] == 0)  
                {  
                    client_socket[i] = new_socket;  
                    printf("Adding to list of sockets as %d\n" , i);  
                         
                    break;  
                }  
            }  
        }  
             
        //else its some IO operation on some other socket
        for (i = 0; i < max_clients; i++)  
        {  
            sd = client_socket[i];
            if (FD_ISSET(sd , &readfds))  
            {   
                printf("It's in fd set\n");
                //Check if it was for closing , and also read the 
                //incoming message 
                if ((valread = recv_iterative(sd, buffer, 1024, MSG_CONFIRM | MSG_DONTWAIT)) == 0)  
                {  
                    //Somebody disconnected , get his details and print 
                    getpeername(sd , (struct sockaddr*)&address, \
                        (socklen_t*)&addrlen);  
                    printf("Host disconnected , ip %s, port %d \n" , 
                          inet_ntoa(address.sin_addr), ntohs(address.sin_port));  
                         
                    //Close the socket and mark as 0 in list for reuse 
                    close(sd);
                    client_socket[i] = 0;  
                }  
                //Process the message that came in 

                if(valread > 0){
                    printf("Got request!\n");
                    json login_msg = json::parse(buffer);
                    auto command = login_msg["command"];
                    auto account = login_msg["account"];
                    auto password = login_msg["password"];
                    bzero(buffer, strlen(buffer));
                    char response[] = "200\0";
                    for(int i=0; i<static_cast<int>(strlen(response)); i++){
                        buffer[i] = response[i];
                    }
                    send_iterative(sd, buffer, strlen(response), 0);
                }
                
                
                    
            }  
        }  
    }  
         
    return 0;  
}  