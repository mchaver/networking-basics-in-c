#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>

#define BUFFSIZE 32
void diep(char *mess) { perror(mess); exit(1); }

//127.0.0.1
int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_in echoserver;
    char buffer[BUFFSIZE];
    unsigned int echolen;
    int received = 0;
    
    if (argc != 4) {
        fprintf(stderr, "USAGE: TCPecho <server_ip> <word> <port>\n");
        exit(1);
    }
    
    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        diep("Failed to create socket");
    }
    
    memset(&echoserver, 0, sizeof(echoserver));
    echoserver.sin_family = AF_INET;
    echoserver.sin_addr.s_addr = inet_addr(argv[1]);
    echoserver.sin_port = htons(atoi(argv[3]));
    
    if (connect (sock, (struct sockaddr *) &echoserver, sizeof(echoserver)) < 0) {
        diep("Failed to connect with server");
    }
    
    echolen = strlen(argv[2]);
    if (send(sock, argv[2], echolen, 0) != echolen) {
        diep("Mismatch in number of sent bytes");
    }
    
    fprintf(stdout, "Received: ");
    while (received < echolen) {
        int bytes = 0;
        if ((bytes = recv(sock, buffer, BUFFSIZE-1, 0)) < 1) {
            diep("Failed to receive bytes from server");
        }
        received += bytes;
        buffer[bytes] = '\0';
        fprintf(stdout, buffer);
    }
    
    fprintf(stdout, "\n");
    close(sock);
    exit(0);
}