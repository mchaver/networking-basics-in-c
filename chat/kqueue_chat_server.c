#include <sys/types.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>


#include <netinet/in.h>
#include <arpa/inet.h>

#define NUSERS 10
#define BUFFLEN 1024

struct uc {
    int uc_fd;
    char *uc_addr;
} users[NUSERS];


void diep(char *mess) { perror(mess); exit(1); }

int makesocket(char *addr, int port) {
    int option_value, sockfd;
    struct sockaddr_in server;
    
    if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        diep("Failed to create socket.");
    }
    
    option_value = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&option_value, (socklen_t)sizeof(option_value)) == -1) {
        diep("setsockopt");
    }
    
    memset(&server, 0, sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(addr);
    
    if(bind(sockfd, (struct sockaddr *)&server, (socklen_t)sizeof(server)) < 0) {
        diep("Failed to bind the server socket.");
    } 

    if (listen(sockfd, NUSERS) < 0) {
        diep("Failed to listen on server socket.");
    }
    
    return sockfd;
}

int main(int argc, char *argv[]) {
    int serversockfd, clientsockfd, kq, uidx, nev, i;
    unsigned int clientlen;
    struct sockaddr_in chatclient;
    socklen_t len;
    char buf[BUFFLEN];
    char *umsg = "too many users!\n";
    
    if (argc != 2) {
        fprintf(stderr, "USAGE: kqueue_chat_server <port> \n");
        exit(1);
    }
    
    serversockfd = makesocket("127.0.0.1", atoi(argv[1]));    
    
    struct kevent chlist[1+NUSERS];
    struct kevent evlist[1+NUSERS];
    if ((kq = kqueue()) < 0) {
        diep("kqueue() failed.");
    }
    
    memset(users, 0, sizeof(struct uc) * NUSERS);
    
    EV_SET(&chlist[0], serversockfd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
    clientlen = sizeof(chatclient);
            
    while(1) {
        //nev = kevent(kq, chlist, 1+NUSERS, evlist, 1+NUSERS, NULL); //maybe should count number of current users
        nev = kevent(kq, chlist, 1, evlist, 1, NULL);
        
        if (nev < 0) {
            diep("kevent() error, less than 0.");
        } else if (nev > 0) {     
            for (i = 0; i < nev; i++) {
                if (evlist[i].flags & EV_ERROR) {
                    printf("%d", nev);
                    fprintf(stderr, "EV_ERROR: %s\n", strerror(evlist[i].data));
                    exit(EXIT_FAILURE);
                }
                
                //accept a client
                if (evlist[i].ident == serversockfd) {
                    if((clientsockfd = accept(serversockfd, (struct sockaddr *)&chatclient, &clientlen)) < 0) {
                        diep("Failed to accept client socket");
                    }
                    
                    //get an empty uc_fd
                    for (uidx = 0; uidx < NUSERS; uidx++) { 
                        if (users[uidx].uc_fd == 0) {
                            break;
                        }
                    }
                    
                    if (uidx == NUSERS) {
                        warn("%s", umsg);
                        write(clientsockfd, umsg, strlen(umsg));
                        close(clientsockfd);
                        continue;
                    }
                    
                    users[uidx].uc_fd = clientsockfd; //this is the client's socketfd
                    users[uidx].uc_addr = strdup(inet_ntoa(chatclient.sin_addr)); //client's address and identifier
                    
                    if (users[uidx].uc_addr == NULL) {
                        err(1, "strdup error");
                    }
                    
                    //offset uidx by one
                    EV_SET(&chlist[uidx+1], clientsockfd, EVFILT_READ, EV_ADD, 0, 0, NULL);
                    //if (kevent(kq, chlist, 1, NULL, 0, NULL) < 0) {
                    if (kevent(kq, chlist, 1+NUSERS, evlist, 1+NUSERS, NULL) < 0) {
                        err(1, "kevent add users error.");
                    }
                    
                    printf("connection from %s added\n", users[uidx].uc_addr);
                    
                } else { //distribute message
                    for (uidx = 0; uidx < NUSERS; uidx++) {
                        if (users[uidx].uc_fd == evlist[i].ident) {
                            break; //sets uidx
                        }
                    }
                    
                    if (uidx == NUSERS) {
                        printf("Bogus message.");
                    }
                    
                    memset(buf, 0, sizeof(buf));
                    
                    int readresult;
                    readresult = read(users[uidx].uc_fd, buf, sizeof(buf));
                    
                    //if (read(users[uidx].uc_fd, buf, sizeof(buf)) < 0) {
                    if (readresult < 0) {
                        continue;
                    }
                    
                    if (readresult == 0) {
                    printf("Removing %s\n", users[uidx].uc_addr);
                    EV_SET(&chlist[i], users[uidx].uc_fd,EVFILT_READ, EV_DELETE, 0, 0, NULL);
                    if (kevent(kq, chlist, 1, 0, 0, NULL) < 0) {
                        diep("Error");
                    }
                    
                    close(users[uidx].uc_fd);
				    free(users[uidx].uc_addr);
                    
                    users[uidx].uc_fd = 0;
                    users[uidx].uc_addr = NULL;
                    
                    continue;
                    }
                    
                    for (uidx = 0; uidx < NUSERS; uidx++) {
                        //don't send message to the original sender
                        if (users[uidx].uc_fd == 0 || users[uidx].uc_fd == evlist[i].ident) {
                            continue;
                        }
                        
                        //write message to other users by writing to their socketfd
                        if (write(users[uidx].uc_fd, buf, sizeof(buf)) < 0) {
                            warn("writing to user %s failed", users[uidx].uc_addr);
                        }
                    }
                }
            }
        }
    }
    
    return 0;
}