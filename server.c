/*******************************************************************************
* Name        : server.c
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

#define MAX_CONN 3


//------------------------------------------------------------------------------------------------------
//STRUCTS

struct Entry {
    char prompt[1024];
    char options[3][50];
    int answer_idx;
};

struct Player {
    int fd;
    int score;
    char name[128];
    bool ready;
};

//------------------------------------------------------------------------------------------------------
//READ QUESTIONS

int read_questions(struct Entry* arr, char* filename){
    FILE* file = fopen(filename, "r");
    if(file == NULL){
        perror("fopen");
        return -1;
    }

    char *readPrompt = NULL;
    char *temp = NULL;
    char *temp2 = NULL;

    size_t size = 0;

    int i = 0;
    while(getline(&readPrompt, &size, file) != -1){

        if(strcmp(readPrompt, "\n") == 0){
            continue;
        }

        strcpy(arr[i].prompt, readPrompt);
 
        if(getline(&temp, &size, file) == -1){
            perror("getline");
            free(readPrompt);
            free(temp);
            free(temp2);
            return -1;
        }

        if(getline(&temp2, &size, file) == -1){
            perror("getline");
            free(readPrompt);
            free(temp);
            free(temp2);
            return -1;
        }

        if (temp2[strlen(temp2) - 1] == '\n'){
            temp2[strlen(temp2) - 1] = '\0';
        }

        char* split = strtok(temp, " ");
        if (split[strlen(split) - 1] == '\n'){
            split[strlen(split) - 1] = '\0';
        }
        for(int j = 0; j < 3; j++){
            strcpy(arr[i].options[j], split);

            if(strcmp(split, temp2) == 0){
                arr[i].answer_idx = j;
            }
            split = strtok(NULL, " ");
            if (j < 2 && split[strlen(split) - 1] == '\n'){
                split[strlen(split) - 1] = '\0';
            }
        }
        i++;
        free(readPrompt);
        free(temp);
        free(temp2);
        readPrompt = NULL;
        temp = NULL;
        temp2 = NULL;
    }
    free(readPrompt);
    free(temp);
    free(temp2);
    return i;
}

//------------------------------------------------------------------------------------------------------
    /* STEP 0
        Parse command line arguments
    */

int main(int argc, char* argv[]){

    bool playing = false;

    char* questionFile = "questions.txt";
    char* IP = "127.0.0.1";
    int PORT = 25555;

    int option = 0;
    while((option = getopt(argc, argv, ":f:i:p:h")) != -1){
        switch(option){
            case 'f':
                questionFile = optarg;
                break;
            case 'i':
                IP = optarg;
                break;
            case 'p':
                PORT = atoi(optarg);
                break;
            case 'h':
                printf("Usage: %s [-f question_file] [-i IP_address] [-p port_number] [-h]\n\n", argv[0]);
                printf("  -f question_file     Default to \"question.txt\";\n");
                printf("  -i IP_address        Default to \"127.0.0.1\";\n");
                printf("  -p port_number       Default to 25555;\n");
                printf("  -h                   Display this help info.\n");
                return 0;
            case ':':
                fprintf(stderr, "Error: Option '-%c' requires an argument.\n", optopt);
                return 1;
            case '?':
                fprintf(stderr, "Error: Unknown option '-%c' received.\n", optopt);
                return 1;
        }
    }

    int    server_fd;
    int    client_fd;
    struct sockaddr_in server_addr;
    struct sockaddr_in in_addr;
    socklen_t addr_size = sizeof(in_addr);

//------------------------------------------------------------------------------------------------------
    /* STEP 1
        Create and set up a socket
    */

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(IP);

//------------------------------------------------------------------------------------------------------
    /* STEP 2
        Bind the file descriptor with address structure
        so that clients can find the address
    */

   int i =
    bind(server_fd,
            (struct sockaddr *) &server_addr,
            sizeof(server_addr));
    if (i < 0) {
        perror("");
        exit(1);
        }

//------------------------------------------------------------------------------------------------------
    /* STEP 3
        Listen to at most 2 incoming connections
    */

    if (listen(server_fd, MAX_CONN) == 0) printf("Welcome to 392 Trivia!\n");
    else {perror("listen"); exit(1); }

//------------------------------------------------------------------------------------------------------
    /* STEP 3.5
        Set up the questions
    */

    struct Entry questions[50];
    int num_questions = read_questions(questions, questionFile);
    if(num_questions == -1){
        perror("read_questions");
        return 1;
    }

//------------------------------------------------------------------------------------------------------
    /* STEP 4
        Accept connections from clients
        to enable communication
    */

    fd_set myset;
    FD_SET(server_fd, &myset);
    int maxfd = server_fd;
    int n_conn = 0;

    int cfds[MAX_CONN];
    for (int i = 0; i < MAX_CONN; i++) cfds[i] = -1;

    int   recvbytes = 0;
    char  buffer[1024];
    char  buffer2[1024];
    memset(buffer, 0, 1024);
    memset(buffer2, 0, 1024);
    int question_number = 1;
    struct Player players[MAX_CONN];
    for (int i = 0; i < MAX_CONN; i++) {
        players[i].fd = -1;
        players[i].score = 0;
        players[i].name[0] = '\0';
        players[i].ready = false;
    }
    while(playing == false){

        maxfd = server_fd;
        FD_ZERO(&myset);
        FD_SET(server_fd, &myset);
        for(int i = 0; i < MAX_CONN; i++){
            if (cfds[i] != -1){
                FD_SET(cfds[i], &myset);
                if (cfds[i] > maxfd) maxfd = cfds[i];
            }
        }

        select(maxfd+1, &myset, NULL, NULL, NULL);

        // See if new connections detected
        if (FD_ISSET(server_fd, &myset)){
            client_fd = accept(server_fd, (struct sockaddr*)&in_addr, &addr_size);
            if(client_fd == -1){
                perror("accept");
                return 1;
            }
            
            if (n_conn < MAX_CONN) {
                printf("New connection detected!\n");
                write(client_fd, "Please type your name: ", 23);
                n_conn++;
                for(int i = 0; i < MAX_CONN; i++){
                    if (cfds[i] == -1){
                        cfds[i] = client_fd;
                        break;
                    }
                }
            }
            else{
                printf("Max connections reached!\n");
                write(client_fd, "Max connections reached!\n", 25);
                close(client_fd);
            }
        }
//------------------------------------------------------------------------------------------------------
//GET PLAYER NAMES
        for (int i = 0; i < MAX_CONN; i++){
            if (cfds[i] != -1 && FD_ISSET(cfds[i], &myset)){

                recvbytes = recv(cfds[i], buffer, 1024, 0);
                if (recvbytes == 0) {
                    printf("Connection lost!\n");
                    close(cfds[i]);
                    n_conn--;
                    cfds[i] = -1;
                    players[i].fd = -1;
                    players[i].name[0] = '\0';
                    players[i].ready = false;
                }
                else {
                    buffer[recvbytes] = 0;
                    strcpy(players[i].name, buffer);
                    players[i].fd = cfds[i];
                    snprintf(buffer2, sizeof(buffer2), "Hi %s!\n", players[i].name);
                    write(players[i].fd, buffer2, strlen(buffer2));
                    memset(buffer2, 0, 1024);
                    players[i].ready = true;
                }
            }
        }

//------------------------------------------------------------------------------------------------------
//CHECK IF GAME STARTS

        bool allIn = true;
        for(int i = 0; i < MAX_CONN; i++){
            if(players[i].ready == false){
                allIn = false;
                break;
            }
        }

        if(!playing && allIn){
            playing = true;
            printf("The game starts now!\n");
            for(int i = 0; i < MAX_CONN; i++){
                write(players[i].fd, "The game starts now!\n", 21);
            }
        }
    }

//END OF FIRST WHILE LOOP
//------------------------------------------------------------------------------------------------------
//DISPLAY QUESTIONS
    while(playing && question_number <= num_questions){

        printf("Question %d: %s\n", question_number, questions[question_number - 1].prompt);
        for(int i = 0; i < 3; i++){
            printf("%d: %s\n", i + 1, questions[question_number - 1].options[i]);
        }
        char formattedQuestion[1224];
        memset(formattedQuestion, 0, 1224);
        snprintf(formattedQuestion, sizeof(formattedQuestion), "Question %d: %s\nPress 1: %s\nPress 2: %s\nPress 3: %s\n", 
                    question_number, questions[question_number - 1].prompt, questions[question_number - 1].options[0], 
                    questions[question_number - 1].options[1], questions[question_number - 1].options[2]);
        for(int i = 0; i < MAX_CONN; i++){
            write(players[i].fd, formattedQuestion, strlen(formattedQuestion));
        }

//------------------------------------------------------------------------------------------------------
//GET ANSWERS

        maxfd = server_fd;
        FD_SET(server_fd, &myset);
        for(int i = 0; i < MAX_CONN; i++){
            if (cfds[i] != -1){
                FD_SET(cfds[i], &myset);
                if (cfds[i] > maxfd) maxfd = cfds[i];
            }
        }

        select(maxfd+1, &myset, NULL, NULL, NULL);

        for (int i = 0; i < MAX_CONN; i++){
            memset(buffer, 0, 1024);
            if (cfds[i] != -1 && FD_ISSET(cfds[i], &myset)){

                recvbytes = recv(cfds[i], buffer, 1024, 0);
                if (recvbytes == 0){
                    printf("Lost connection!\n");
                    for(int z = 0; z < MAX_CONN; z++){
                        close(players[z].fd);
                    }
                    return 0;
                }
                else{
                    buffer[recvbytes] = 0;
                    int answer = atoi(buffer);
                    if(answer == questions[question_number - 1].answer_idx + 1){
                        players[i].score++;    
                    }
                    else{
                        players[i].score--;
                    }
                    char formatCorrect[1024];
                    snprintf(formatCorrect, sizeof(formatCorrect), "Correct answer: %d: %s\n\n",
                                        questions[question_number - 1].answer_idx + 1, 
                            questions[question_number - 1].options[questions[question_number - 1].answer_idx]);
                    printf("%s", formatCorrect);
                    for(int y = 0; y < MAX_CONN; y++){
                        write(players[y].fd, formatCorrect, strlen(formatCorrect));
                    }
                    question_number++;
                    i = MAX_CONN;
                    break;
                }
            }
        }

//------------------------------------------------------------------------------------------------------
//END GAME

        if(question_number > num_questions){
            playing = false;
            char congrats[1024];
            int numWinners = 1;
            int highestScore = players[0].score;
            memset(congrats, 0, 1024);

            for(int i = 1; i < MAX_CONN; i++){
                if(players[i].score > highestScore){
                    highestScore = players[i].score;
                    numWinners = 1;
                }
                else if(players[i].score == highestScore){
                    numWinners++;
                }
            }
            for(int j = 0; j < MAX_CONN; j++){
                if(players[j].score == highestScore){
                    memset(congrats, 0, 1024);
                    snprintf(congrats, sizeof(congrats), "Congrats %s!\n", players[j].name);
                    printf("%s", congrats);
                    for(int k = 0; k < MAX_CONN; k++){
                        write(players[k].fd, congrats, strlen(congrats));
                    }
                }
            }

            char scores[1024];
            memset(scores, 0, 1024);
            int length = 0;

            for(int j = 0; j < MAX_CONN; j++){
                length += snprintf(scores + length, sizeof(scores) - length, "%s: %d\n", players[j].name, players[j].score);
            }
            printf("%s", scores);
            for(int i = 0; i < MAX_CONN; i++){
                write(players[i].fd, scores, strlen(scores));
                
            }

            for(int i = 0; i < MAX_CONN; i++){
                close(players[i].fd);
            }
            return 0;
        }
    }
}