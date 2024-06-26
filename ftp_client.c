#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <dirent.h>
#include <sys/stat.h>

#include <unistd.h>

// lenght of all buffers
#define BUFFER_SIZE 256

// port numbers for the server
#define S_CONTROLPORT 7001
#define S_DATAPORT 7000

// count is for the PORT N + I functionality where, count represents I
int count = 0;
// root is the root directory of the client
char root[BUFFER_SIZE];
// current_dir is the current directory of the client
char current_dir[BUFFER_SIZE];

int port_ip_argument[6]; // 0-3 for IP, 4-5 for port (h1, h2, h3, h4, p1, p2)

char username[BUFFER_SIZE];
int user_logged_in = 0;

// sturcture to store the current address of the client
typedef struct {
    char IP[16];
    unsigned int port;
} current_addr;

// connects to the Control channel of the server
int connectToServer(){
    // create a socket using the Internet Protocol version 4 (IPv4) and the TCP protocol
    int server_sd = socket(AF_INET, SOCK_STREAM, 0);

    // check if socket was created successfully
    if (server_sd < 0) {
        perror("socket: ");
        exit(-1);
    }

    // set socket option to allow reuse of local addresses
    int value = 1;
    setsockopt(server_sd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));

    struct sockaddr_in server_addr;

    // zero-initialize the structure
    bzero(&server_addr, sizeof(server_addr));

    // set the server's address and port for control channel
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(S_CONTROLPORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");   //INADDR_ANY, INADDR_LOOP

    // connect to the server at the specified address and port
    if (connect(server_sd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect: ");
        exit(-1);
    } else {
        // print the 220 welcome
        char welcome[BUFFER_SIZE];
        recv(server_sd, welcome, BUFFER_SIZE, 0);
        printf("%s\n", welcome);
    }
    
    return server_sd;
}

// creates a data socket for the client to connect to the server and retriece files.
// client binds and listens to any connection the server makes
int create_data_socket(char IP[16], unsigned int port){
    // create a socket using the Internet Protocol version 4 (IPv4) and the TCP protocol
    int server_data_sd = socket(AF_INET, SOCK_STREAM, 0);

    // check if socket was created successfully
    if (server_data_sd < 0) {
        perror("socket: ");
        exit(-1);
    }

    // set socket option to allow reuse of local addresses
    int value = 1;
    setsockopt(server_data_sd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));

    // create a structure to hold the server's address
    struct sockaddr_in server_addr;

    // zero-initialize the structure
    bzero(&server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(IP);   //INADDR_ANY, INADDR_LOOP

    // bind the socket to the specified address and port
    if (bind(server_data_sd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind: ");
        exit(-1);
    }

    // listen for incoming connections
    if (listen(server_data_sd, 5) < 0) {
        perror("listen: ");
        exit(-1);
    }
    
    return server_data_sd;
}

// gets the current address of the client and returns it
current_addr get_addr(int server_sd){
    current_addr curr_addr;
    struct sockaddr_in my_addr;

    bzero(&my_addr, sizeof(my_addr));
    socklen_t len = sizeof(my_addr);
    getsockname(server_sd, (struct sockaddr *) &my_addr, &len);
    inet_ntop(AF_INET, &my_addr.sin_addr, curr_addr.IP, sizeof(curr_addr.IP));
    curr_addr.port = ntohs(my_addr.sin_port);

    return curr_addr;
}

// gets the IP address from the argument and stores it in port_ip_argument which seperates the IP into h1, h2, h3, h4
void get_ip_argument(char ip[]) {
    uint32_t ip_addr;
    ip_addr = inet_addr(ip);

    // getting the h1, h2, h3, h4
    port_ip_argument[0] = ip_addr / 1000000;
    port_ip_argument[1] = (ip_addr - port_ip_argument[0]*1000000) / 10000;
    port_ip_argument[3] = (ip_addr)%100;
    port_ip_argument[2] = ip_addr%10000 / 100;
}

// this function is called when the user enters the LIST command
void send_list_command(char msg_to_send[], char client_argument[], int server_sd) {
    // creating the message to send to the server (we are concatenating the command and the argument bacause we had to split them beforehands)
    char list_msg[BUFFER_SIZE];
    strcpy(list_msg, msg_to_send);
    strcat(list_msg, " ");
    strcat(list_msg, client_argument);

     // creating a data socket to retreive files
    current_addr curr_addr = get_addr(server_sd);
    curr_addr.port += count; // implementing the port number incrementation
    int data_socket = create_data_socket(curr_addr.IP, curr_addr.port);

    // send the LIST command to the server
    if (send(server_sd, list_msg, strlen(list_msg), 0) < 0) {
        perror("Error Sending message.\n");
        exit(-1);
    }

    // get the 150 response and print it
    char list_received[BUFFER_SIZE];
    if (recv(server_sd, list_received, sizeof(list_received), 0)==0){
        printf("Error receiving message from the server\n");
        exit(-1);
    }
    printf("%s", list_received);
    bzero(list_received, sizeof(list_received));

    // accept new server connection to get the list of files
    struct sockaddr_in server_addr;  // create a struct for the client address
    bzero(&server_addr, sizeof(server_addr));
    unsigned int addrlen = sizeof(server_addr);  // get the size of the client address struct
    int client_sd = accept(data_socket, (struct sockaddr*)&server_addr, (socklen_t*)&addrlen);  // accept a client connection and get a new socket descriptor for the client
    if(client_sd < 0)  // check if accepting the client connection failed
    {
        perror("accept failed");  
        exit(-1);
    }
    
    // receive the list of files from the server until the server closes the connection
    while (1) {
        int bytes = recv(client_sd, list_received, sizeof(list_received), 0);
        printf("%s", list_received);
        bzero(list_received, sizeof(list_received));

        if (bytes == 0) {
            break;
        }
    }

    // close all sockets
    close(data_socket);
    close(client_sd);

    // get the 226 response and print it
    if (recv(server_sd, list_received, sizeof(list_received), 0)==0){
        printf("Error receiving message from the server\n");
        exit(-1);
    }
    printf("%s\n", list_received);
}

void send_retr_command(char msg_to_send[], char client_argument[], int server_sd) {
    // creating the message to send to the server (we are concatenating the command and the argument bacause we had to split them beforehands)
    char retr_msg[BUFFER_SIZE];
    strcpy(retr_msg, msg_to_send);
    strcat(retr_msg, " ");
    strcat(retr_msg, client_argument);

    // creating a data socket to retreive files
    current_addr curr_addr = get_addr(server_sd);
    curr_addr.port += count;
    int data_socket = create_data_socket(curr_addr.IP, curr_addr.port);

    // send the RETR command to the server
    if (send(server_sd, retr_msg, strlen(retr_msg), 0) < 0) {
        perror("Error Sending message.\n");
        exit(-1);
    }

    // get the 150 response
    char retr_received[BUFFER_SIZE];
    if (recv(server_sd, retr_received, sizeof(retr_received), 0)==0){
        printf("Error receiving message from the server\n");
        exit(-1);
    }
    printf("%s", retr_received);

    // if the file does not exist, close the data socket and return
    if (strcmp(retr_received, "550 No such file or directory\n") == 0) {
        close(data_socket);
        return;
    }
    // to reset the retr_received buffer
    bzero(retr_received, sizeof(retr_received));

    // accept new server connection
    struct sockaddr_in server_addr;  // create a struct for the client address
    bzero(&server_addr, sizeof(server_addr));
    unsigned int addrlen = sizeof(server_addr);  // get the size of the client address struct
    int client_sd = accept(data_socket, (struct sockaddr*)&server_addr, (socklen_t*)&addrlen);  // accept a client connection and get a new socket descriptor for the client
    if(client_sd < 0)  // check if accepting the client connection failed
    {
        perror("accept failed");  
        exit(-1);
    }

    // creating a file to write the data to
    char file_name[BUFFER_SIZE];
    strcpy(file_name, current_dir);
    strcat(file_name, "/");
    strcat(file_name, client_argument);

    FILE* file = fopen(file_name, "wb");

    // receive the file and its content from the server until the server closes the connection
    while (1) {
        int bytes = recv(client_sd, retr_received, sizeof(retr_received), 0);
        if (bytes == 0) {
            break;
        }
        fwrite(retr_received, 1, bytes, file);
        bzero(retr_received, sizeof(retr_received));
    }

    // closing all sockets and files
    close(data_socket);
    close(client_sd);
    fclose(file);

    // get the 226 response and print it
    if (recv(server_sd, retr_received, sizeof(retr_received), 0)==0){
        printf("Error receiving message from the server\n");
        exit(-1);
    }
    printf("%s\n", retr_received);
}

void send_stor_command(char msg_to_send[], char client_argument[], int server_sd) {
    // creating the message to send to the server (we are concatenating the command and the argument bacause we had to split them beforehands)
    char stor_msg[BUFFER_SIZE];
    strcpy(stor_msg, msg_to_send);
    strcat(stor_msg, " ");
    strcat(stor_msg, client_argument);

    chdir(current_dir);

    // creating a data socket to store files
    current_addr curr_addr = get_addr(server_sd);
    curr_addr.port += count;
    int data_socket = create_data_socket(curr_addr.IP, curr_addr.port);

    // send the STOR command to the server
    if (send(server_sd, stor_msg, strlen(stor_msg), 0) < 0) {
        perror("Error Sending message.\n");
        exit(-1);
    }

    // get the 150 response
    char stor_received[BUFFER_SIZE];
    if (recv(server_sd, stor_received, sizeof(stor_received), 0)==0){
        printf("Error receiving message from the server\n");
        exit(-1);
    }
    printf("%s", stor_received);
    bzero(stor_received, sizeof(stor_received));

    // accept new server connection
    struct sockaddr_in server_addr;  // create a struct for the client address
    bzero(&server_addr, sizeof(server_addr));
    unsigned int addrlen = sizeof(server_addr);  // get the size of the client address struct
    int client_sd = accept(data_socket, (struct sockaddr*)&server_addr, (socklen_t*)&addrlen);  // accept a client connection and get a new socket descriptor for the client
    if(client_sd < 0)  // check if accepting the client connection failed
    {
        perror("accept failed");  
        exit(-1);
    }

    // changing the current directory to the one where the file is located
    chdir(current_dir);

    // opening the file to send
    // rb is used to read the file in binary mode
    FILE *fp = fopen(client_argument, "rb");
    if (fp == NULL) {
        printf("Error opening file.\n");
        exit(1);
    }

    long bytes_sent = 0;
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // sending the file to the server
    char lines[BUFFER_SIZE];
    int n;
    while ((n = fread(lines, 1, BUFFER_SIZE, fp)) > 0) {
        if (send(client_sd, lines, n, 0) < 0) {
            perror("Error sending file");
            exit(1);
        }
        bytes_sent += n;

        // to reset the lines buffer
        bzero(lines, BUFFER_SIZE);
    }

    // checking if all bytes were sent
    if (bytes_sent < file_size) {
        printf("Error not all bytes sent.\n");
        return;
    }

    // closing all sockets and files
    close(data_socket);
    close(client_sd);
    fclose(fp);

    // get the 226 response and print it
    if (recv(server_sd, stor_received, sizeof(stor_received), 0)==0){
        printf("Error receiving message from the server\n");
        exit(-1);
    }
    printf("%s\n", stor_received);
}

int main() 
{   
    // getting the current directory
    getcwd(root, sizeof(root));
    strcpy(current_dir, root);

    // connecting to the server through control channel
    int server_sd = connectToServer();

    char msg_to_send[BUFFER_SIZE];
    while (1)
    {
        // take command input
        printf("ftp> ");
        fgets(msg_to_send, BUFFER_SIZE, stdin);

        // remove trailing newline char from buffer, fgets doesn't do it
        msg_to_send[strcspn(msg_to_send, "\n")] = 0; // review 0 or '0'

        // parsing user input: gets the command and the argument into two different strings
        char* parse = strtok(msg_to_send, " ");
        char client_command[BUFFER_SIZE];
        strcpy(client_command, parse);

        char client_argument[BUFFER_SIZE];
        while (parse != NULL) {
            strcpy(client_argument, parse);
            parse = strtok(NULL, " ");
        }

        // saving the username
        if (strcmp(client_command, "USER") == 0) {
            strcpy(username, client_argument);
        }

        // creating a directory for the user when logged in
        if (user_logged_in == 1) {
            strcat(current_dir, "/Client/");
            strcat(current_dir, username);
            if (chdir(current_dir) == -1) {
                mkdir(current_dir, 0777);
            }

            // resetting the user_logged_in variable so that this directory change is only done once
            user_logged_in = 0;
            chdir(current_dir);
        }

        if (strcmp(client_command, "!CWD") == 0) {
            // changing the current directory to the directory the user wants to go to
            char path[BUFFER_SIZE];
            strcpy(path, current_dir);
            strcat(path, "/");
            strcat(path, client_argument);

            // checking if the directory exists
            if (chdir(path) != -1) {
                strcpy(current_dir, path);
                printf("200 directory changed to %s\n", current_dir);

                // skips all code below and goes to the next iteration of the loop
                continue;
            }

            printf("550 No such file or directory.\n");

            // because you do not want !CWD to go to the server
            continue;
        } else if (strcmp(client_command, "!PWD") == 0) {
            // getting the current directory
            if (chdir(current_dir) != -1) {
                printf("257 %s\n", current_dir);
                // skips all code below and goes to the next iteration of the loop
                continue;
            }

            printf("550 No such file or directory.\n");

            // because you do not want !PWD to go to the server
            continue;
        } else if (strcmp(client_command, "!LIST") == 0) {
            // this is where the client is
            chdir(current_dir);

            // printing the files in the current directory
            DIR *dir;
            struct dirent *ent;
			struct stat st;
            dir = opendir (".");

            if (dir != NULL) {
                /* print all the files and directories within directory */
                while ((ent = readdir (dir)) != NULL) {

                    // to skip the first two files since they default to "." and ".."
                    if (ent->d_name[0] == '.') {
                        continue; // skip hidden files
                    }

                    // to skip over any directories
                    stat(ent->d_name, &st);
                    if (!S_ISREG(st.st_mode)) {
                        continue;
                    }

                    // printing the file name
                    printf("%s\n", ent->d_name);
                }
                closedir (dir);
            } else {
                /* could not open directory */
                perror ("Failed to open directory");
                return EXIT_FAILURE;
            }

            // because you do not want !LIST to go to the server
            continue;
        }

        // if the command is LIST, STOR, or RETR, then we need to create a data socket and send the PORT command
        if (!strcmp(client_command, "LIST") || !strcmp(client_command, "STOR") || !strcmp(client_command, "RETR")) {
            // getting the current address of the client
            current_addr curr_addr = get_addr(server_sd);

            // we increase the port by 1 for data connection when the command is LIST, STOR, or RETR
            count++;

            curr_addr.port += count; // implementing the port number incrementation

            // parsing the IP address and populates the port_ip_argument array
            get_ip_argument(curr_addr.IP);
            port_ip_argument[4] = curr_addr.port/BUFFER_SIZE;
            port_ip_argument[5] = curr_addr.port%BUFFER_SIZE;

            // make a string for argument i.e. h1,h2,h3,h4,p1,p2 as a single string
            char arg[BUFFER_SIZE];
            char temp[BUFFER_SIZE];
            sprintf(temp, "%d", port_ip_argument[0]);
            strcpy(arg, temp);
            strcat(arg, ",");
            for (int i = 1; i < 5; i++) {
                sprintf(temp, "%d", port_ip_argument[i]);
                strcat(arg, temp);
                strcat(arg, ",");
            }

            // adding the last element of the array
            sprintf(temp, "%d", port_ip_argument[5]);
            strcat(arg, temp);

            // creating the PORT command to send to server
            char msg[BUFFER_SIZE];
            strcpy(msg, "PORT ");
            strcat(msg, arg);

            // sending the server the PORT command
            if (send(server_sd, msg, strlen(msg), 0) < 0) {
                perror("Error Sending message.\n");
                exit(-1);
            }
            
            // receiving the server's reply
            char port_reply[BUFFER_SIZE];
            bzero(port_reply, sizeof(port_reply));
            if (recv(server_sd, port_reply, sizeof(port_reply), 0)==0){
                printf("Error receiving message from the server\n");
                break;
            }
            printf("%s", port_reply);

            // if the server did not accept the PORT command, continue
            if (strcmp(port_reply, "200 PORT command successful.\n") != 0) {
                continue;
            }
        }

        // if the command is LIST
        if (strcmp(client_command, "LIST") == 0) {
            // forking the process so that the client can continue to send commands
            int pid = fork();
            if (pid == 0) {
                send_list_command(msg_to_send, client_argument, server_sd);
                exit(0);
            }
        } else if  (strcmp(client_command, "RETR") == 0) {
            int pid = fork();
            if (pid == 0) {
                chdir(current_dir);
                send_retr_command(msg_to_send, client_argument, server_sd);
                exit(0);
            }
        } else if (strcmp(client_command, "STOR") == 0) {
            int pid = fork();
            if (pid == 0) {
                chdir(current_dir);
                // checking if the file exists in the client's directory
                FILE* check = fopen(client_argument, "r");
                if (check == NULL) {
                    printf("550 No such file or directory\n");
                    exit(0);
                }
                fclose(check);

                send_stor_command(msg_to_send, client_argument, server_sd);
                exit(0);
            }
        } else {
            // if the command is not LIST, RETR, or STOR, then we just send the command to the server
            strcat(msg_to_send, " ");
            strcat(msg_to_send, client_argument);

            // sending the message to the server
            if (send(server_sd, msg_to_send, strlen(msg_to_send), 0) < 0) {
                perror("Error Sending message.\n");
                exit(-1);
            }

            // receiving the server's reply
            char msg_received[BUFFER_SIZE];
            bzero(msg_received, sizeof(msg_received));

            if (recv(server_sd, msg_received, sizeof(msg_received), 0)==0){
                printf("Error receiving message from the server\n");
                break;
            }
            printf("%s\n", msg_received);


            // user is logged in
            if (strcmp(msg_received, "230 User logged in, proceed.\n") == 0) {
                user_logged_in = 1;
            } 

            // if the server is closing the connection, then we close the client's connection
            if (strcmp(msg_received, "221 Service closing control connection.\n") == 0) {
                close(server_sd);
                break;
            }
            bzero(msg_to_send, sizeof(msg_to_send));
        }
    }

    return 0;
}