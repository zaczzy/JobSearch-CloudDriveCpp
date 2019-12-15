#include <netdb.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include "data_types.h"

#define MAX 8192 
#define PORT 5000 
#define SA struct sockaddr 

void get_email_header_list(int sockfd)
{
    char buff[MAX]; 
        bzero(buff, sizeof(buff)); 
    get_mail_request req;
    strncpy(req.prefix, "getmail", strlen("getmail"));
    req.prefix[strlen(req.prefix)] = '\0';
    strncpy(req.username, "ritika", strlen("ritika"));
    req.username[strlen(req.username)] = '\0';

    int bytes = write(sockfd, (char*)(&req), sizeof(get_mail_request)); 
    printf("written %d bytes to server\n", bytes);

    /** Read the email from server */
    bzero(buff, sizeof(buff)); 
    bytes = read(sockfd, buff, sizeof(buff)); 
    printf("read %d bytes from Server : message: %s ", bytes, buff);
    get_mail_response* response;
    response = (get_mail_response*)buff;

    printf("prefix: %s\n", response->prefix);
    printf("username: %s\n", response->username);
    printf("num emails: %lu\n", response->num_emails);

    for(unsigned long i = 0; i < response->num_emails; i++)
    {
        printf("from: %s\n", response->email_headers[i].from);
        printf("to: %s\n", response->email_headers[i].to);
        printf("subject: %s\n", response->email_headers[i].subject);
        printf("date: %s\n", response->email_headers[i].date);
        printf("email_id: %lu\n", response->email_headers[i].email_id);
    }

}

void write_emails(int sockfd)
{
    char buff[MAX]; 
    bzero(buff, sizeof(buff)); 
    for (int i = 0; i < 3; i++) 
    { 
        bzero(buff, sizeof(buff)); 

        /** Send the email */
        printf("Sending data to the server\n");

        time_t t = time(NULL);
        struct tm *tm = localtime(&t);


        put_mail_request request;
        strncpy(request.prefix, "putmail", strlen("putmail"));
        request.prefix[strlen(request.prefix)] = '\0';
        strncpy(request.username, "ritika", strlen("ritika"));
        request.username[strlen(request.username)] = '\0';
        strncpy(request.header.from, "abc@gmail.com", strlen("abc@gmail.com"));
        request.header.from[strlen(request.header.from)] = '\0';
        strncpy(request.header.to, "ritika@gmail.com", strlen("ritika@gmail.com"));
        request.header.to[strlen(request.header.to)] = '\0';
        strncpy(request.header.subject, "test email", strlen("test email"));
        request.header.subject[strlen(request.header.subject)] = '\0';
        strftime(request.header.date, sizeof(request.header.date), "%c", tm);
        request.header.date[strlen(request.header.date)] = '\0';

        request.email_len = 12;
        strncpy(request.email_body, "Hi, everyone", strlen("Hi, everyone"));

        int bytes = write(sockfd, (char*)(&request), sizeof(put_mail_request)); 
        
        printf("written %d bytes to server\n", bytes);

        bzero(buff, sizeof(buff)); 
        bytes = read(sockfd, buff, sizeof(buff)); 
        printf("read %d bytes from Server : message: %s ", bytes, buff);

        sleep(1);
    }    
}

void func(int sockfd) 
{ 
    char buff[MAX]; 
    int bytes;
    bzero(buff, sizeof(buff)); 
    read(sockfd, buff, sizeof(buff)); 
    printf("From Server : %s", buff); 

    write_emails(sockfd);

    /** Get the email headers from server */
   printf("reading email headers from server\n");
   get_email_header_list(sockfd);

   /** Read email body */
    get_mail_body_request body_req;
    strncpy(body_req.prefix, "mailbody", strlen("mailbody"));
    body_req.prefix[strlen(body_req.prefix)] = '\0';
    strncpy(body_req.username, "ritika", strlen("ritika"));
    body_req.username[strlen(body_req.username)] = '\0';
    body_req.email_id = 2;

    bytes = write(sockfd, (char*)(&body_req), sizeof(get_mail_body_request)); 
    printf("written %d bytes to server\n", bytes);
    
    bytes = read(sockfd, buff, sizeof(buff)); 
    printf("read %d bytes from Server : message: %s \n", bytes, buff);
    get_mail_body_response* body_response;
    body_response = (get_mail_body_response*)buff;

    printf("prefix: %s\n", body_response->prefix);
    printf("username: %s\n", body_response->username);
    printf(" email id: %lu\n", body_response->email_id);
    printf(" email len: %lu\n", body_response->mail_body_len);

    printf("mail body: %s\n", body_response->mail_body);

    printf("\n\nDeleting email\n\n");
    /** Delete an email */
    delete_mail_request del_req;
    strncpy(del_req.prefix, "delmail", strlen("delmail"));
    del_req.prefix[strlen(del_req.prefix)] = '\0';
    strncpy(del_req.username, "ritika", strlen("ritika"));
    del_req.username[strlen(del_req.username)] = '\0';
    del_req.email_id = 1;

    bytes = write(sockfd, (char*)(&del_req), sizeof(delete_mail_request)); 
    printf("written %d bytes to server\n", bytes);
    bytes = read(sockfd, buff, sizeof(buff)); 
    printf("read %d bytes from Server : message: %s \n", bytes, buff);
   
    /** Get the email headers from server */
    printf("reading email headers from server\n");
    get_email_header_list(sockfd);


} 
  
int main() 
{ 
    int sockfd; 
    struct sockaddr_in servaddr; 
  
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
  
    while(true);
    // close the socket 
    close(sockfd); 
} 
