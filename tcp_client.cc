#include <netdb.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "data_types.h"

#define MAX 4096 
#define PORT 5000 
#define SA struct sockaddr 

void func(int sockfd) 
{ 
    char buff[MAX]; 
    int n; 
        bzero(buff, sizeof(buff)); 
        read(sockfd, buff, sizeof(buff)); 
        printf("From Server : %s", buff); 
    //for (;;) 
    { 
        bzero(buff, sizeof(buff)); 

        /** Send the email */
        printf("Sending data to the server\n");

        put_mail_request request;
        strncpy(request.prefix, "putmail", strlen("putmail"));
        request.prefix[strlen(request.prefix)] = '\0';
        strncpy(request.username, "ritika", strlen("ritika"));
        request.username[strlen(request.username)] = '\0';
        strncpy(request.email_id, "ritika@gmail.com", strlen("ritika@gmail.com"));
        request.email_id[strlen(request.email_id)] = '\0';
        strncpy(request.header.from, "abc@gmail.com", strlen("abc@gmail.com"));
        request.header.from[strlen(request.header.from)] = '\0';
        strncpy(request.header.to, "ritika@gmail.com", strlen("ritika@gmail.com"));
        request.header.to[strlen(request.header.to)] = '\0';
        strncpy(request.header.subject, "test email", strlen("test email"));
        request.header.subject[strlen(request.header.subject)] = '\0';
        strncpy(request.header.date, "Nov 21: 63542", strlen("Nov 21: 63542"));
        request.header.date[strlen(request.header.date)] = '\0';

        request.email_len = 12;
        strncpy(request.email_body, "Hi, everyone", strlen("Hi, everyone"));

        int bytes = write(sockfd, (char*)(&request), sizeof(put_mail_request)); 
        
        printf("written %d bytes to server\n", bytes);

        bzero(buff, sizeof(buff)); 
        bytes = read(sockfd, buff, sizeof(buff)); 
        printf("read %d bytes from Server : message: %s ", bytes, buff);
        

        printf("reading email from server\n");
        /** Get the email from server */
        get_mail_request req;
        strncpy(req.prefix, "getmail", strlen("getmail"));
        req.prefix[strlen(req.prefix)] = '\0';
        strncpy(req.username, "ritika", strlen("ritika"));
        req.username[strlen(req.username)] = '\0';
        strncpy(req.email_id, "ritika@gmail.com", strlen("ritika@gmail.com"));
        req.email_id[strlen(req.email_id)] = '\0';
        
        bytes = write(sockfd, (char*)(&req), sizeof(get_mail_request)); 
        printf("written %d bytes to server\n", bytes);
        
        /** Read the email from server */
        bzero(buff, sizeof(buff)); 
        bytes = read(sockfd, buff, sizeof(buff)); 
        printf("read %d bytes from Server : message: %s ", bytes, buff);
        get_mail_response* response;
        response = (get_mail_response*)buff;

        printf("prefix: %s\n", response->prefix);
        printf("username: %s\n", response->username);
        printf("email: %s\n", response->email_id);
        printf("num emails: %d\n", response->num_emails);

        for(int i = 0; i < response->num_emails; i++)
        {
            printf("from: %s\n", response->email_headers[i].from);
            printf("to: %s\n", response->email_headers[i].to);
            printf("subject: %s\n", response->email_headers[i].subject);
            printf("date: %s\n", response->email_headers[i].date);
        }
         
    } 
} 
  
int main() 
{ 
    int sockfd, connfd; 
    struct sockaddr_in servaddr, cli; 
  
    // socket create and varification 
    sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd == -1) { 
        printf("socket creation failed...\n"); 
        exit(0); 
    } 
    else
        printf("Socket successfully created..\n"); 
    bzero(&servaddr, sizeof(servaddr)); 
  
    // assign IP, PORT 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    servaddr.sin_port = htons(PORT); 
  
    // connect the client socket to server socket 
    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) { 
        printf("connection with the server failed...\n"); 
        exit(0); 
    } 
    else
        printf("connected to the server..\n"); 
  
    // function for chat 
    func(sockfd); 
  
    // close the socket 
    close(sockfd); 
} 
