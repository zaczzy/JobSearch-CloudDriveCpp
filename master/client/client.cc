#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h> 
#include <sys/socket.h> 
#include <unistd.h>
#include <string.h>
#include <string>
using namespace std;
// ./test_client [port]
int main(int argc, char const *argv[]) 
{ 
    if (argc != 2) {
        fprintf(stderr, "expect 1 argument: port\n");
        return -1;
    }
    int port = stoi(argv[1]);
    int sock = 0, valread; 
    struct sockaddr_in serv_addr;
    const char *request = "01primary"; 
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    { 
        printf("\n Socket creation error \n"); 
        return -1; 
    } 
   
    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(port); 
       
    // Convert IPv4 and IPv6 addresses from text to binary form 
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0)  
    { 
        printf("\nInvalid address/ Address not supported \n"); 
        return -1; 
    } 
   
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
    { 
        printf("\nConnection Failed \n"); 
        return -1; 
    } 
    send(sock , request , strlen(request) , 0 ); 
    printf("Primary inquiry sent\n"); 
    struct sockaddr_in result;
    valread = read(sock, &result, 1024);
    if (valread != (int) sizeof(result)) {
        fprintf(stderr, "Result size incorrect!\n");
        return -1;
    }
    printf("IP: %s\n", inet_ntoa(result.sin_addr));
    printf("PORT: %hd\n", ntohs(result.sin_port));
    close(sock);
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    { 
        printf("\n Socket creation error \n"); 
        return -1; 
    }
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
    { 
        printf("\nConnection Failed \n"); 
        return -1; 
    } 
    // again
    request = "00primary";
    send(sock , request , strlen(request) , 0 ); 
    printf("Primary inquiry sent\n"); 
    valread = read(sock, &result, 1024);
    if (valread != (int) sizeof(result)) {
        fprintf(stderr, "Result size incorrect!\n");
        return -1;
    }
    printf("IP: %s\n", inet_ntoa(result.sin_addr));
    printf("PORT: %hd\n", ntohs(result.sin_port));
    close(sock);
    return 0; 
} 