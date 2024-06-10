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
#include <string>
#include <iostream>
#include <string>
#include <chrono>
#include <regex>
//#include "date.h"
#include "nlohmann/json.hpp"
using json=nlohmann::json;
#define FMT_HEADER_ONLY
#include "fmt/format.h"
#include "fmt/chrono.h"
     
#define TRUE   1 
#define FALSE  0 
#define PORT 9999
#define BUFF_LENGTH 4096

inline std::string escapeJsonString(std::string input){
    regex re1("\\n");
    regex re2("\\\\");
    input = regex_replace(regex_replace(input, re2, "\\\\"), re1, "\\n");
    return input;
}
     
int main(int argc , char *argv[])  
{  
    int opt = TRUE;  
    int master_socket , addrlen , new_socket , client_socket[30] , 
          max_clients = 30 , activity, i , valread , sd;  
    int max_sd;  
    struct sockaddr_in address;  
         
    char buffer[BUFF_LENGTH+1];  //data buffer of 1K 
         
    //set of socket descriptors 
    fd_set readfds; 
     
    //initialise all client_socket[] to 0 so not checked 
    for (i = 0; i < max_clients; i++)  
    {  
        client_socket[i] = 0;  
    }  
         
    //create a master socket 
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
    address.sin_addr.s_addr = INADDR_ANY;  
    address.sin_port = htons( PORT );  
         
    //bind the socket to localhost port 8888 
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
    std::string bulletinText="";
    std::string teacherName="";
    std::string questionText="";
         
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
        activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);  
       
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
            printf("New connection , socket fd is %d , ip is : %s , port : %d "
                  "\n" , new_socket , inet_ntoa(address.sin_addr) , ntohs
                  (address.sin_port));  
                 
            puts("Welcome message sent successfully");  
                 
            //add new socket to array of sockets 
            for (i = 0; i < max_clients; i++)  
            {  
                //if position is empty 
                if( client_socket[i] == 0 )  
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
                 
            if (FD_ISSET( sd , &readfds))  
            {  
                //Check if it was for closing , and also read the 
                //incoming message 
                if ( (valread = read( sd , buffer, BUFF_LENGTH)) == 0)  
                {  
                    //Somebody disconnected , get his details and print 
                    getpeername(sd , (struct sockaddr*)&address , \
                        (socklen_t*)&addrlen);  
                    printf("Host disconnected , ip %s , port %d \n" , 
                          inet_ntoa(address.sin_addr) , ntohs(address.sin_port));  
                         
                    //Close the socket and mark as 0 in list for reuse 
                    close( sd );  
                    client_socket[i] = 0;  
                }  
                     
                //Echo back the message that came in 
                else 
                {  
                    //set the string terminating NULL byte on the end 
                    //of the data read 
                    buffer[valread] = '\0';
                    json recvPacket=json::parse(buffer);
                    std::cout<<"recvPacket:"<<recvPacket<<std::endl;
                    
                    //packet=R"({"code": 200, "identity": "admin"})"_json;
                    //packet=R"({"code": 200, "identity": "teacher"})"_json;
                    //packet=R"({"code": 200, "identity": "rulemaker"})"_json;
                    if(recvPacket["command"]=="login"){
                        json sendPacket=json::parse(fmt::format("{{\"code\": 200, \"identity\": \"{}\"}}",recvPacket["username"]));
                        std::string ret=sendPacket.dump();
                        send(sd , ret.c_str(), strlen(ret.c_str()) , 0 );  
                    }else if(recvPacket["command"]=="get users"){
                        json sendPacket=json::parse(fmt::format("{{\"code\": 200, \"counts\": {}}}",5));
                        std::string ret=sendPacket.dump();
                        send(sd , ret.c_str(), strlen(ret.c_str()) , 0 );
                        for(int i=0;i<5;i++){
                            sendPacket=json::parse(fmt::format("{{\"username\": \"{}\"}}","admin"+std::to_string(i)));
                            std::string ret=sendPacket.dump();
                            send(sd , ret.c_str(), strlen(ret.c_str()) , 0 );
                            usleep(100);
                        }
                    }else if(recvPacket["command"]=="delete user"){
                        json sendPacket=json::parse(fmt::format("{{\"code\": 200}}"));
                        std::string ret=sendPacket.dump();
                        send(sd , ret.c_str(), strlen(ret.c_str()) , 0 );
                    }else if(recvPacket["command"]=="register user"){
                        json sendPacket=json::parse(fmt::format("{{\"code\": 403}}"));
                        std::string ret=sendPacket.dump();
                        send(sd , ret.c_str(), strlen(ret.c_str()) , 0 );
                    }else if(recvPacket["command"]=="get bulletins"){
                        json sendPacket=json::parse(fmt::format("{{\"code\": 200, \"counts\": {}}}",5));
                        std::string ret=sendPacket.dump();
                        send(sd , ret.c_str(), strlen(ret.c_str()) , 0 );
                        usleep(100);
                        for(int i=0;i<5;i++){
                            auto t=std::chrono::system_clock::now();
                            sendPacket=json::parse(fmt::format("{{\"bulletin name\": \"{}\"}}",
                            "teachers"+std::to_string(i)+'+'+fmt::format("{:%Y-%m-%d_%H-%M-%S}",std::chrono::floor<std::chrono::seconds>(t))));
                            std::cout<<"sendPackets: "<<sendPacket<<std::endl;
                            std::string ret=sendPacket.dump();
                            send(sd , ret.c_str(), strlen(ret.c_str()) , 0 );
                            usleep(100);
                        }
                    }else if(recvPacket["command"]=="get teachers"){
                        json sendPacket=json::parse(fmt::format("{{\"code\": 200, \"counts\": {}}}",5));
                        std::string ret=sendPacket.dump();
                        send(sd , ret.c_str(), strlen(ret.c_str()) , 0 );
                        usleep(100);
                        for(int i=0;i<5;i++){
                            sendPacket=json::parse(fmt::format("{{\"username\": \"{}\"}}","teacher"+std::to_string(i)));
                            std::string ret=sendPacket.dump();
                            send(sd , ret.c_str(), strlen(ret.c_str()) , 0 );
                            usleep(100);
                        }
                    }else if(recvPacket["command"]=="write bulletin"){
                        std::cout<<recvPacket["bulletin text"]<<std::endl;
                        bulletinText=recvPacket["bulletin text"];
                        teacherName=recvPacket["teacher name"];
                        std::cout<<bulletinText<<std::endl;
                        json sendPacket=json::parse(fmt::format("{{\"code\": 200}}"));
                        std::string ret=sendPacket.dump();
                        send(sd , ret.c_str(), strlen(ret.c_str()) , 0 );
                    }else if(recvPacket["command"]=="read bulletin"){
                        std::cout<<bulletinText<<std::endl;
                        std::string escapeBulletin=escapeJsonString(bulletinText);
                        auto escapeTeacher=escapeJsonString(teacherName);
                        json sendPacket=json::parse(fmt::format("{{\"code\": 200,\"bulletin text\":\"{}\"}}",escapeBulletin));
                        std::string ret=sendPacket.dump();
                        send(sd , ret.c_str(), strlen(ret.c_str()) , 0 );
                    }else if(recvPacket["command"]=="delete bulletin"){
                        json sendPacket=json::parse(fmt::format("{{\"code\": 200}}"));
                        std::string ret=sendPacket.dump();
                        send(sd , ret.c_str(), strlen(ret.c_str()) , 0 );
                    }else if(recvPacket["command"]=="get subjects"){
                        json sendPacket=json::parse(fmt::format("{{\"code\": 200, \"counts\": {}}}",5));
                        std::string ret=sendPacket.dump();
                        send(sd , ret.c_str(), strlen(ret.c_str()) , 0 );
                        usleep(100);
                        for(int i=0;i<5;i++){
                            sendPacket=json::parse(fmt::format("{{\"subject name\": \"{}\"}}","subject"+std::to_string(i)));
                            std::string ret=sendPacket.dump();
                            send(sd , ret.c_str(), strlen(ret.c_str()) , 0 );
                            usleep(100);
                        }
                    }else if(recvPacket["command"]=="get chapters"){
                        json sendPacket=json::parse(fmt::format("{{\"code\": 200, \"counts\": {}}}",5));
                        std::string ret=sendPacket.dump();
                        send(sd , ret.c_str(), strlen(ret.c_str()) , 0 );
                        usleep(100);
                        for(int i=0;i<5;i++){
                            sendPacket=json::parse(fmt::format("{{\"chapter name\": \"{}\"}}","chapter"+std::to_string(i)));
                            std::string ret=sendPacket.dump();
                            send(sd , ret.c_str(), strlen(ret.c_str()) , 0 );
                            usleep(100);
                        }
                    }else if(recvPacket["command"]=="get questions"){
                        json sendPacket=json::parse(fmt::format("{{\"code\": 200, \"counts\": {}}}",5));
                        std::string ret=sendPacket.dump();
                        send(sd , ret.c_str(), strlen(ret.c_str()) , 0 );
                        usleep(100);
                        for(int i=0;i<5;i++){
                            auto t=std::chrono::system_clock::now();
                            sendPacket=json::parse(fmt::format("{{\"question name\": \"{}\"}}",
                            fmt::format("{:%Y-%m-%d_%H-%M-%S}",std::chrono::floor<std::chrono::seconds>(t))));
                            std::string ret=sendPacket.dump();
                            send(sd , ret.c_str(), strlen(ret.c_str()), 0);
                            usleep(100);
                        }
                    }else if(recvPacket["command"]=="read question"){
                        std::cout<<"question Text: "<<questionText<<std::endl;
                        std::string escapeQuestion=escapeJsonString(questionText);
                        json sendPacket=json::parse(fmt::format("{{\"code\": 200,\"question text\":\"{}\"}}",escapeQuestion));
                        std::string ret=sendPacket.dump();
                        send(sd , ret.c_str(), strlen(ret.c_str()) , 0 );
                    }else if(recvPacket["command"]=="write question"){
                        std::cout<<"question Text: "<<recvPacket["question text"]<<std::endl;
                        questionText=recvPacket["question text"];
                        json sendPacket=json::parse(fmt::format("{{\"code\": 200}}"));
                        std::string ret=sendPacket.dump();
                        send(sd , ret.c_str(), strlen(ret.c_str()) , 0 );
                    }else if(recvPacket["command"]=="delete question"){
                        json sendPacket=json::parse(fmt::format("{{\"code\": 200}}"));
                        std::string ret=sendPacket.dump();
                        send(sd , ret.c_str(), strlen(ret.c_str()) , 0 );
                    }else if(recvPacket["command"]=="delete chapter"){
                        json sendPacket=json::parse(fmt::format("{{\"code\": 200}}"));
                        std::string ret=sendPacket.dump();
                        send(sd , ret.c_str(), strlen(ret.c_str()) , 0 );
                    }else if(recvPacket["command"]=="delete subject"){
                        json sendPacket=json::parse(fmt::format("{{\"code\": 200}}"));
                        std::string ret=sendPacket.dump();
                        send(sd , ret.c_str(), strlen(ret.c_str()) , 0 );
                    }else if(recvPacket["command"]=="logout"){
                        json sendPacket=json::parse(fmt::format("{{\"code\": 200}}"));
                        std::string ret=sendPacket.dump();
                        send(sd , ret.c_str(), strlen(ret.c_str()) , 0 );
                    }
                    
                }  
            }  
        }  
    }  
         
    return 0;  
}  