/*******************************************************************************
* Name        : client.c
* Author      : Matt Bernardon
* Pledge      : I pledge my honor that I have abided by the Stevens Honor System.
******************************************************************************/

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>

char* IP = "127.0.0.1";
int PORT = 25555;
int   recvbytes = 0;

//------------------------------------------------------------------------------------------------------
//PARSE CONNECT
void parse_connect(int argc, char** argv, int* server_fd){
    int option = 0;
    while((option = getopt(argc, argv, ":i:p:h")) != -1){
        switch(option){
            case 'i':
                IP = optarg;
                break;
            case 'p':
                PORT = atoi(optarg);
                break;
            case 'h':
                printf("Usage: %s [-i IP_address] [-p port_number] [-h]\n\n", argv[0]);
                printf("  -i IP_address        Default to \"127.0.0.1\";\n");
                printf("  -p port_number       Default to 25555;\n");
                printf("  -h                   Display this help info.\n");
                close(*server_fd);
                exit(0);
            case ':':
                fprintf(stderr, "Error: Option '-%c' requires an argument.\n", optopt);
                close(*server_fd);
                exit(1);
            case '?':
                fprintf(stderr, "Error: Unknown option '-%c' received.\n", optopt);
                close(*server_fd);
                exit(1);
        }
    }
}

//------------------------------------------------------------------------------------------------------
int main(int argc, char* argv[]){

    /* STEP 1:
        Create a socket to talk to the server;
    */
    int    server_fd;
    struct sockaddr_in server_addr;
    socklen_t addr_size = sizeof(server_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&server_addr, 0, sizeof(server_addr));

    parse_connect(argc, argv, &server_fd);

    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(IP);

//------------------------------------------------------------------------------------------------------
    /* STEP 2:
        Try to connect to the server.
    */
    int c = connect(server_fd,
            (struct sockaddr *) &server_addr,
            addr_size);
    if(c == -1){
        perror("Connect");
        close(server_fd);
        return 1;
    }


//------------------------------------------------------------------------------------------------------
//SEND AND RECEIVE

    char buffer[1024];
    memset(buffer, 0, 1024);
    int cfds[2];
    cfds[0] = server_fd;
    cfds[1] = STDIN_FILENO;
    fd_set myset;
    FD_SET(server_fd, &myset);
    int maxfd = server_fd;
    while(1){

        maxfd = server_fd;
        FD_SET(cfds[0], &myset);
        if (cfds[0] > maxfd) maxfd = cfds[0];
        FD_SET(cfds[1], &myset);
        if (cfds[1] > maxfd) maxfd = cfds[1];

        int s = select(maxfd+1, &myset, NULL, NULL, NULL);
        if (s == -1){
            perror("Select");
            close(server_fd);
            return 1;
        }

        for (int i = 0; i < 2; i++){
            if (cfds[i] != -1 && FD_ISSET(cfds[i], &myset)){
                if (i == 0){
                    recvbytes = recv(server_fd, buffer, 1024, 0);
                    if (recvbytes == 0 || recvbytes == -1) {
                        close(server_fd);
                        return 0;
                    }
                    else{
                        buffer[recvbytes] = 0;
                        printf("%s\n", buffer); fflush(stdout);
                    }
                }
                else if (i == 1){
                    scanf("%s", buffer);
                    if(send(server_fd, buffer, strlen(buffer), 0) == -1){
                        perror("Send");
                        close(server_fd);
                        return 1;
                    }
                }
            }
        }
    }

    close(server_fd);

  return 0;
}