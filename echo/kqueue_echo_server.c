#include <sys/event.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFSIZE 1024
#define MAXPENDING 5
void diep(char *mess) { perror(mess); exit(1); }

void HandleClient(int sock) {
    char buffer[BUFFSIZE];
    int received = -1;
    if ((received = recv(sock, buffer, BUFFSIZE, 0)) < 0) {
        diep("Failed to receive initial bytes from client");
    }
    
    while (received > 0) {
        if (send(sock, buffer, received, 0) != received) {
            diep("Failed to send bytes to client");
        }
        
        if ((received = recv(sock, buffer, BUFFSIZE, 0)) < 0) {
            diep("Failed to receive additional bytes from client");
        }
    }
    close(sock);
}

int main(int argc, char *argv[]) {
    int serversockfd, clientsockfd;
    struct sockaddr_in echoserver, echoclient;
    
    if (argc != 2) {
        fprintf(stderr, "USAGE: echoserver <port> \n");
        exit(1);
    }
    
    if((serversockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        diep("Failed to create socket.");
    }
    
    memset(&echoserver, 0, sizeof(echoserver));
    echoserver.sin_family = AF_INET;
    echoserver.sin_addr.s_addr = htonl(INADDR_ANY);
    echoserver.sin_port = htons(atoi(argv[1]));
    
    if (bind(serversockfd, (struct sockaddr *) &echoserver, sizeof(echoserver)) < 0) {
        diep("Failed to bind the server socket.");
    }
    
    if (listen(serversockfd, MAXPENDING) < 0) {
        diep("Failed to listen on server socket.");
    }
    
    struct kevent chlist[1];  //events server is monitoring
    struct kevent evlist[1];  //events that were triggered
    int kq, nev, i;
    
    if ((kq = kqueue()) == -1) {
        diep("kqueue() failed.");
    }
    
    EV_SET(&chlist[0], serversockfd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
    
    while(1) {
        nev = kevent(kq, chlist, 1, evlist, 1, NULL);
        if (nev < 0) {
            diep("kevent() error, less than 0.");
        }
        
        for (i = 0; i < nev; i++) {
            if (evlist[i].flags & EV_ERROR) {
                fprintf(stderr, "EV_ERROR: %s\n", strerror(evlist[i].data));
                exit(EXIT_FAILURE);
            }
            
            //handle server
            if (evlist[i].ident == serversockfd) {
                unsigned int clientlen = sizeof(echoclient);
                if((clientsockfd = accept(serversockfd, (struct sockaddr *) &echoclient, &clientlen)) < 0) {
                    diep("Failed to accept client socket");
                }
                HandleClient(clientsockfd);
                fprintf(stdout, "Client connected\n");
            }
        }
        
    }
    
    return 0;
}