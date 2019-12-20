#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h> 
#include <sys/socket.h> 
#include <unistd.h>
#include <string.h>
#include <string>
#include <chrono>
#include <thread>

using namespace std;
// ./test_client [port]
int main(int argc, char const *argv[]) 
{ 
    if (argc != 4) {
        fprintf(stderr, "expect 1 argument: port\n");
        return -1;
    }
    int port = stoi(argv[1]);
    int sock = 0, valread; 
    struct sockaddr_in serv_addr;
    const char *request = "01primary"; // First request
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
    //    identify as backend
    int blah1 = stoi(argv[2]);
    int blah2 = stoi(argv[3]);
    unsigned short blah[2];
    blah[0] = (unsigned short) blah1;
    blah[1] = (unsigned short) blah2;
    write(sock, blah, sizeof(blah));

    //test real request
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    write(sock , request , strlen(request));
    printf("Primary inquiry sent\n"); 
    int result;
    valread = read(sock, &result, sizeof(result));
    close(sock);

    //ROUND 2

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

    //    identify as backend (again)
    write(sock, blah, sizeof(blah));

    //test real request (again)
    request = "02primary"; // SECOND request
    send(sock , request , strlen(request) , 0 ); 
    printf("Primary inquiry sent\n"); 
    valread = read(sock, &result, 1024);
    close(sock);
    return 0; 
} 
