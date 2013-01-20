#include <sys/event.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

#define BUFSIZE 1024

void diep(char *mess) { perror(mess); exit(1); }
int makesocket(char *addr, int port);
void sendbuftosck(int sckfd, const char *buf, int len);

int main(int argc, char *argv[]) {
    int sockfd, kq, nev, i;
    
    if(argc != 3) {
        fprintf(stderr, "usage: %s host port\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    sockfd = makeclientsocket(argv[1], atoi(argv[2]));
    struct kevent chlist[2];
    struct kevent evlist[2];
    char buf[BUFSIZE];
    
    if ((kq = kqueue()) < 0) {
        diep("kqueue() initialization error.");
    }
    
    EV_SET(&chlist[0], sockfd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
    EV_SET(&chlist[1], fileno(stdin), EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
    
    while(1) {
        nev = kevent(kq, chlist, 2, evlist, 2, NULL);
        
        if (nev < 0) {
            diep("kevent() error.");
        } else if (nev > 0) {
            if  (evlist[0].flags & EV_EOF) {
                exit(EXIT_FAILURE);
            }
            
            for (i = 0; i < nev; i++) {
                if (evlist[i].flags & EV_ERROR) {
                    fprintf(stderr, "EV_ERROR: %s\n", strerror(evlist[i].data));
                    exit(EXIT_FAILURE);
                }
                
                if (evlist[i].ident == sockfd) { //data from the host
                    memset(buf, 0, BUFSIZE);
                    if (read(sockfd, buf, BUFSIZE) < 0) {
                        diep("read()");
                    }
                    fputs(buf, stdout);
                } else if (evlist[i].ident == fileno(stdin)) { //data from stdin
                    memset(buf, 0, BUFSIZE);
                    fgets(buf, BUFSIZE, stdin);
                    sendbuftosck(sockfd, buf, strlen(buf));
                }   
            }
        }
    }
    
    close(kq);
    return 0;
}

int makeclientsocket(char *addr, int port) {
    int option_value, sockfd;
    struct sockaddr_in server;
    /*
    struct hostent *hp;
    
    if ((hp = gethostbyname(host)) == NULL) {
        diep("gethostbyname()");
    }
    server.sin_addr = *((struct in_addr *)hp->h_addr);
    */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        diep("Failed to create socket.");
    }
    
    memset(&server, 0, sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(addr);
    
    if (connect(sockfd, (struct sockaddr *)&server, sizeof(struct sockaddr)) < 0) {
        diep("connect()");
    }
    
    return sockfd;
}

void sendbuftosck(int sckfd, const char *buf, int len) {
   int bytessent, pos;

   pos = 0;
   do {
      if ((bytessent = send(sckfd, buf + pos, len - pos, 0)) < 0)
         diep("send()");
      pos += bytessent;
   } while (bytessent > 0);
}