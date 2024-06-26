#include<stdio.h>
#include<string.h>
#include<stdbool.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<sys/select.h>
#include<unistd.h>
#include<stdlib.h>
#include <sys/stat.h>
#include <dirent.h>

#define BUFFER_SIZE 256

#define S_CONTROLPORT 7001 
#define S_DATAPORT 7000

char root[BUFFER_SIZE];

typedef struct
{
	char path[BUFFER_SIZE];
	char username[BUFFER_SIZE];
} UserState;

// to handle multiple clients
UserState user_state[BUFFER_SIZE];

typedef struct {
	char IP[BUFFER_SIZE];
	int port;
} data_connection_port;

// to handle multiple clients
data_connection_port dcp[BUFFER_SIZE];

// globals for user authentication
int users_logged_in[BUFFER_SIZE] = {0};
int username_logged_in[BUFFER_SIZE] = {0};
int password_logged_in[BUFFER_SIZE] = {0};

int data_sockets[BUFFER_SIZE] = {0};

// creating socket for control connection
int createServerSocket(){
    // create a socket using the Internet Protocol version 4 (IPv4) and the TCP protocol
	int server_sd = socket(AF_INET,SOCK_STREAM,0);
	printf("Server fd = %d \n",server_sd);
    // check if socket was created successfully
	if(server_sd<0) {
		perror("socket:");
		exit(-1);
	}

    // set socket option to allow reuse of local addresses
	int value  = 1;
	setsockopt(server_sd,SOL_SOCKET,SO_REUSEADDR,&value,sizeof(value)); //&(int){1},sizeof(int)

	struct sockaddr_in server_addr;

    // set the server's address and port for control channel
	bzero(&server_addr,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(S_CONTROLPORT);
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); //INADDR_ANY, INADDR_LOOP

    // bind the socket to the specified address and port
	if(bind(server_sd, (struct sockaddr*)&server_addr,sizeof(server_addr))<0)
	{
		perror("bind failed");
		exit(-1);
	}
    // listen for incoming connections
	if(listen(server_sd,5)<0)
	{
		perror("listen failed");
		close(server_sd);
		exit(-1);
	}

    return server_sd;
}

int create_data_socket(char IP[BUFFER_SIZE], unsigned int port){
    // create a socket using the Internet Protocol version 4 (IPv4) and the TCP protocol
	int server_sd = socket(AF_INET,SOCK_STREAM,0);
	printf("Server fd = %d \n",server_sd);
    // check if socket was created successfully
	if(server_sd<0)
	{
		perror("socket:");
		exit(-1);
	}

    // set socket option to allow reuse of local addresses
	int value  = 1;
	setsockopt(server_sd,SOL_SOCKET,SO_REUSEADDR,&value,sizeof(value)); //&(int){1},sizeof(int)
	struct sockaddr_in server_addr, server_bind_addr;
	bzero(&server_addr,sizeof(server_addr));

	// set the server's address and port for data channel
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = inet_addr(IP); //INADDR_ANY, INADDR_LOOP

	// we bind the socket to the specified address and port for server side
	server_bind_addr.sin_family = AF_INET;
	server_bind_addr.sin_port = htons(S_DATAPORT);
	server_bind_addr.sin_addr.s_addr = inet_addr(IP); //INADDR_ANY, INADDR_LOOP

	// bind the socket to the the specified address and port for data connection
    if (bind(server_sd, (struct sockaddr*)&server_bind_addr, sizeof(server_bind_addr)) < 0) {
        perror("bind: ");
        exit(-1);
    }

	// connect to the client data socket to receive data
	if (connect(server_sd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		perror("connect failed");
		exit(-1);
	}


    return server_sd;
}

int verify_username(int fd, char client_username[BUFFER_SIZE]){
    // opening the login credentials file
	char path[BUFFER_SIZE];
	strcpy(path, root);
	strcat(path, "/");
	char file_name[] = "users.txt";
	strcat(path, file_name);

    FILE* fp = fopen(path, "r");

	// checking if the file exists
	if (fp == NULL) {
        printf("-----------------------------------\n");
        printf("Error Opening File: No such file found!\n");
        printf("-----------------------------------\n");
        
        return 1;
    }

	// reading the file line by line
    char * line = NULL;
    size_t len = 0;
    ssize_t line_length;

	// checking if the username entered by the client is valid
    while ((line_length = getline(&line, &len, fp)) != -1) {
        char user_name[BUFFER_SIZE];
		// username is the string before the comma
		char* username = strtok(line, ",");
		strcpy(user_name, username);

		// checking if the username entered by the client is valid
        if (strcmp(user_name, client_username) == 0) {
            char reply[BUFFER_SIZE];
            strcpy(reply, "331 Username OK, need password.\n");
            if (send(fd, reply, strlen(reply), 0) < 0) {
                perror("Error sending message");
                exit(1);
            }

            fclose(fp);
            return 1;
        }
    }

	// if the username entered by the client is invalid
	char reply[BUFFER_SIZE];
	strcpy(reply, "530 Not logged in.\n");
	if (send(fd, reply, strlen(reply), 0) < 0) {
		perror("Error sending message");
		exit(1);
	}

    fclose(fp);
    return 0;
}

int verify_password(int fd, char client_password[BUFFER_SIZE]) {
	// opening the login credentials file
	char path[BUFFER_SIZE];
	strcpy(path, root);
	strcat(path, "/");
	char file_name[] = "users.txt";
	strcat(path, file_name);

    FILE* fp = fopen(path, "r");
	if (fp == NULL) {
		printf("-----------------------------------\n");
		printf("Error Opening File: No such file found!\n");
		printf("-----------------------------------\n");

		return 1;
	}

	// reading the file line by line
	char * line = NULL;
	size_t len = 0;
	ssize_t line_length;

	// checking if the password entered by the client is valid
	while ((line_length = getline(&line, &len, fp)) != -1) {
		// also check if the username is the same for the password
		char password[BUFFER_SIZE];
		char username[BUFFER_SIZE];
		bzero(password, sizeof(password));
		bzero(username, sizeof(username));

		// get the position of the comma
		int comma_pos = 0;
		while (line[comma_pos] != ',') {
			// getting username to verity the password
			strncat(username, &line[comma_pos], 1);
			comma_pos++;
		}

		// get the actual password from the line
		for (int i = comma_pos + 1; i < line_length; i++) {
			strncat(password, &line[i], 1);
		}
		
		// remove the newline character from the password
		password[strlen(password) - 1] = '\0';
		
		// check if the password is correct for the username
		if (strcmp(password, client_password) == 0 && strcmp(username, user_state[fd].username) == 0) {
			char reply[BUFFER_SIZE];
			strcpy(reply, "230 User logged in, proceed.\n");
			if (send(fd, reply, strlen(reply), 0) < 0) {
				perror("Error sending message");
				exit(1);
			}

			fclose(fp);
			return 1;
		}
	}

	// if the password entered by the client is invalid
	char reply[BUFFER_SIZE];
	strcpy(reply, "530 Not logged in.\n");
	if (send(fd, reply, strlen(reply), 0) < 0) {
		perror("Error sending message");
		exit(1);
	}

	fclose(fp);
	return 0;
}

void resetting_session(int fd, int* max_fd, fd_set* full_fdset) {
	// reset user session when the client disconnects
	users_logged_in[fd] = 0;
	username_logged_in[fd] = 0;
	password_logged_in[fd] = 0;
	close(fd);
	FD_CLR(fd,full_fdset);
	if(fd==(*max_fd))
	{
		for(int i=(*max_fd); i>=3; i--) {
			if(FD_ISSET(i,full_fdset))
			{
				(*max_fd) =  i;
				// continue;
				break;
			}
		}
	}
}

// get ip and port
// called when PORT command is entered
struct in_addr getIP(char client_argument[BUFFER_SIZE]) {
	// parsing port argument to retrieve ip address
	char IP[BUFFER_SIZE];
	strcpy(IP, "");
	char* parse = strtok(client_argument, ",");

	// gets h1, h2, h3, h4
	for (int i = 0; i < 4; i++) {
		strcat(IP, parse);
		parse = strtok(NULL, ",");
	}

	struct in_addr ip_addr;
	inet_aton(IP, &ip_addr);

	// converting network byte order to host byte order
	ip_addr.s_addr = ntohl(ip_addr.s_addr);

    return ip_addr;
}

int getPort(char client_argument[BUFFER_SIZE]) {
	// PARSING port number
	char port_str[50];
	strcpy(port_str, "");
	
	// exhausting the first 4 arguments
	char* parse = strtok(client_argument, ",");
	for (int i = 0; i < 4; i++) {
		parse = strtok(NULL, ",");
	}

	// getting the last 2 arguments, p1 and p2
	int port = 0;
	port = atoi(parse) * 256;
	parse = strtok(NULL, ",");
	port += atoi(parse);

	return port;
}


int main()
{	
	// get the root directory
	getcwd(root, sizeof(root));

	// create a socket for control connection
    int server_sd = createServerSocket();
	fd_set full_fdset;
	fd_set read_fdset;
	FD_ZERO(&full_fdset);

	int max_fd = server_sd;

	FD_SET(server_sd,&full_fdset);

	printf("Server is listening...\n");

	while(1)
	{
		read_fdset = full_fdset;

		// select function to handle multiple clients
		if(select(max_fd+1,&read_fdset,NULL,NULL,NULL)<0)
		{
			perror("select");
			exit (-1);
		}

	    FD_SET(server_sd,&full_fdset);

		for(int fd = 3 ; fd<=max_fd; fd++)
		{
			if(FD_ISSET(fd,&read_fdset))
			{	
				// check if the socket is the server socket to accept new client connection
				if(fd==server_sd)
				{
                    // accept new client connection
                    struct sockaddr_in client_addr;  // create a struct for the client address
                    bzero(&client_addr, sizeof(client_addr));
    
                    unsigned int addrlen = sizeof(client_addr);  // get the size of the client address struct
                    int client_sd = accept(server_sd, (struct sockaddr*)&client_addr, (socklen_t*)&addrlen);  // accept a client connection and get a new socket descriptor for the client

                    if(client_sd < 0)  // check if accepting the client connection failed
                    {
                        perror("accept failed");  
                        continue; 
                    }

					// credentials of the client
					printf("Connected fd = %d IP Address:%s Port:%d \n",client_sd, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
					FD_SET(client_sd,&full_fdset);

					// send welcome message to the client
					send(client_sd, "220 Service ready for new user.\n", strlen("220 Service ready for new user.\n"), 0);
					
					if(client_sd>max_fd)	
						max_fd = client_sd;
				}
				else
				{
					char client_request[BUFFER_SIZE];
					bzero(client_request,sizeof(client_request));

					// receive client request
					int bytes = recv(fd,client_request,sizeof(client_request),0);
					if(bytes==0)   //client has closed the connection#
					{
						// reset user session when the client disconnects
						printf("connection closed from client side \n");
						resetting_session(fd, &max_fd, &full_fdset);
					}
                    else{
                        printf("Client: %s \n",client_request);

						// parsing user input: gets the command and the argument
						char* parse = strtok(client_request, " ");
						char client_command[BUFFER_SIZE];
						strcpy(client_command, parse);

						char client_argument[BUFFER_SIZE];
						while (parse != NULL) {
							strcpy(client_argument, parse);
							parse = strtok(NULL, " ");
						}

						// this is to filter out any commands that our FTP implementation doees not do
						if (strcmp(client_command, "PORT") && strcmp(client_command, "USER") && strcmp(client_command, "PASS") && strcmp(client_command, "CWD") && strcmp(client_command, "PWD") && strcmp(client_command, "LIST") && strcmp(client_command, "STOR") && strcmp(client_command, "RETR") && strcmp(client_command, "QUIT")) {
							char reply[BUFFER_SIZE];
							strcpy(reply, "202 Command not implemented.\n");
							if (send(fd, reply, strlen(reply), 0) < 0) {
								perror("Error sending message");
								exit(1);
							}
							
							// continue skips any code below it
							continue;
						}

						// QUIT command
						if (strcmp(client_command, "QUIT") == 0) {
							// TODO: implement the data transfer quitting
							char reply[BUFFER_SIZE];
							strcpy(reply, "221 Service closing control connection.\n");
							if (send(fd, reply, strlen(reply), 0) < 0) {
								perror("Error sending message");
								exit(1);
							}

							// reset user session when the client disconnects
							resetting_session(fd, &max_fd, &full_fdset);

							// continue skips any code below it
							continue;
						}
						
						// we need to check if the user is logged in before executing any commands
						// users_logged_in[fd] is 0 if the user is not logged in
						if (users_logged_in[fd] == 0) {
							// login procedure
							if (strcmp(client_command, "USER") == 0) {
								// verify the username
								username_logged_in[fd] = verify_username(fd, client_argument);

								// store the username in the user_state struct for further use
								strcpy(user_state[fd].username, client_argument);
							} else if (strcmp(client_command, "PASS") == 0) {
								// if username is not verified before password, then we need to reject the password
								if (username_logged_in[fd] == 0) {
									char reply[BUFFER_SIZE];
									strcpy(reply, "503 Bad sequence of command.\n");
									if (send(fd, reply, strlen(reply), 0) < 0) {
										perror("Error sending message");
										exit(1);
									}

									// continue skips any code below it
									continue;
								}

								// verify the password otherwise
								password_logged_in[fd] = verify_password(fd, client_argument);
							} else {
								// if the user is not logged in, then we need to reject any other commands
								char reply[BUFFER_SIZE];
								strcpy(reply, "530 Not logged in.\n");
								if (send(fd, reply, strlen(reply), 0) < 0) {
									perror("Error sending message");
									exit(1);
								}

								continue;
							}

							// user is logged in if both username and password are verified
							if (username_logged_in[fd] == 1 && password_logged_in[fd] == 1) {
								// we have now authenticated the user
								users_logged_in[fd] = 1;

								// create a directory for the client if it does not exist
								// the directory name is the username
								char path[BUFFER_SIZE];
								strcpy(path, root);
								strcat(path, "/Users/");
								strcat(path, user_state[fd].username);

								if (chdir(path) == -1) {
									mkdir(path, 0777);
								}

								// change the current working directory to the user's directory
								chdir(path);

								// we store the user state
								UserState us;
								strcpy(us.path, path);
								user_state[fd] = us;

								// Note: user_state[fd] contains path and the username for the user
							}
						} else {
							// if the user is logged in, then we need to check if the user is in the correct sequence of commands
							// if the user is not in the correct sequence of commands, then we need to reject the command
							// one wrong sequence is login after login. We need to reject the second login
							if (!strcmp(client_command, "USER") || !strcmp(client_command, "PASS")) {
								char reply[BUFFER_SIZE];
								strcpy(reply, "503 Bad sequence of command.\n");
								if (send(fd, reply, strlen(reply), 0) < 0) {
									perror("Error sending message");
									exit(1);
								}

								continue;
							}

							// if the user is in the correct sequence of commands, then we need to execute the command
							chdir(user_state[fd].path);

							// CWD command
							if (strcmp(client_command, "CWD") == 0) {
								char path[BUFFER_SIZE];
								strcpy(path, user_state[fd].path);
								strcat(path, "/");
								strcat(path, client_argument);

								// check if the path exists
								if (chdir(path) != -1) {
									char reply[BUFFER_SIZE];
									strcpy(reply, "200 directory change to ");
									strcat(reply, path);
									strcat(reply, "\n");

									// we store the new path in the user_state struct which changes the current working directory
									strcpy(user_state[fd].path, path);
									
									// send the reply to the client
									if (send(fd, reply, strlen(reply), 0) < 0) {
										perror("Error sending message");
										exit(1);
									}

									continue;
								}

								// if the path does not exist, then we need to reject the command
								char reply[BUFFER_SIZE];
								strcpy(reply, "550 No such file or directory.\n");
								if (send(fd, reply, strlen(reply), 0) < 0) {
									perror("Error sending message");
									exit(1);
								}
								
							} else if (strcmp(client_command, "PWD") == 0) {
								// we need to send the current working directory to the client
								if (chdir(user_state[fd].path) != -1) {

									char reply[BUFFER_SIZE];
									strcpy(reply, "257 ");
									// receiving the current working directory
									strcat(reply, user_state[fd].path);
									strcat(reply, "\n");

									// send the reply to the client
									if (send(fd, reply, strlen(reply), 0) < 0) {
										perror("Error sending message");
										exit(1);
									}

									continue;
								}

								// if the path does not exist, then we need to reject the command
								char reply[BUFFER_SIZE];
								strcpy(reply, "550 No such file or directory.\n");
								if (send(fd, reply, strlen(reply), 0) < 0) {
									perror("Error sending message");
									exit(1);
								}
							} else if (strcmp(client_command, "PORT") == 0) {
								// we need to parse the argument for the port and IP
								char reply[BUFFER_SIZE];
								// send the reply to the client
								strcpy(reply, "200 PORT command successful.\n");
								if (send(fd, reply, strlen(reply), 0) < 0) {
									perror("Error sending message");
									exit(1);
								}

								// parsing for port and IP (data connection)
								char client_argument_copy[BUFFER_SIZE];
								strcpy(client_argument_copy, client_argument);

								// get the IP address
								struct in_addr ip_int = getIP(client_argument);
								char IP[BUFFER_SIZE];
								strcpy(IP, inet_ntoa(ip_int));
								
								// get the port
								int port = getPort(client_argument_copy);

								// store the IP and port in the data connection struct
								strcpy(dcp[fd].IP, IP);
								dcp[fd].port = port;
							} else if (strcmp(client_command, "LIST") == 0) {
								int pid = fork();

								if (pid == 0) {
									// child process
									// we need to send the list of files in the current working directory to the client
									char reply[BUFFER_SIZE];
									strcpy(reply, "150 File status okay; about to open. data connection.\n");
									send(fd, reply, sizeof(reply), 0);

									// change the current working directory to the user's directory
									chdir(user_state[fd].path);

									// creating a data socket to send the list of files
									data_sockets[fd] = create_data_socket(dcp[fd].IP, dcp[fd].port);

									// send the file
									DIR *dir;
									struct dirent *ent;
									struct stat st;
									dir = opendir (".");
									if (dir != NULL) {

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

											// send the file name to the client
											char reply[BUFFER_SIZE];
											strcpy(reply, ent->d_name);
											strcat(reply, "\n");
											send(data_sockets[fd], reply, sizeof(reply), 0);
										}
										closedir (dir);
									} else {
										/* could not open directory */
										perror ("Failed to open directory");
										return EXIT_FAILURE;
									}

									// close the data socket
									close(data_sockets[fd]);
									// set the data socket to 0 so that we know it is closed
									data_sockets[fd] = 0;

									// sending the 226 reply
									strcpy(reply, "226 Transfer completed.\n");
									send(fd, reply, sizeof(reply), 0);

									// exit the child process
									exit(EXIT_SUCCESS);

								} 
							} else if (strcmp(client_command, "RETR") == 0) {
								int pid = fork();

								if (pid == 0) {

									// checking if the file exists
									FILE* check = fopen(client_argument, "r");
									if (check == NULL) {
										// if the file does not exist, then we need to reject the command
										char reply[BUFFER_SIZE];
										strcpy(reply, "550 No such file or directory.\n");
										send(fd, reply, sizeof(reply), 0);
										fclose(check);
										exit(0);
									}
									fclose(check);

									// replying with 150 status okay
									char reply[BUFFER_SIZE];
									strcpy(reply, "150 File status okay; about to open. data connection.\n");
									send(fd, reply, sizeof(reply), 0);
									bzero(reply, BUFFER_SIZE);
									
									// creating a data socket to send the file
									data_sockets[fd] = create_data_socket(dcp[fd].IP, dcp[fd].port);

									chdir(user_state[fd].path);

									// send the file
									// rb is for reading in binary mode
									FILE *fp = fopen(client_argument, "rb");
									// if the file does not exist, then we need to reject the command (for some unforseen reason)
									if (fp == NULL) {
										char reply[BUFFER_SIZE];
										strcpy(reply, "550 No such file or directory.\n");
										send(fd, reply, sizeof(reply), 0);
										close(data_sockets[fd]);
										exit(1);
									}

									long bytes_sent = 0;
									fseek(fp, 0, SEEK_END);
									long file_size = ftell(fp);
									fseek(fp, 0, SEEK_SET);

									// reading the file and sending it to the client
									char lines[BUFFER_SIZE];
									int n;
									while ((n = fread(lines, 1, BUFFER_SIZE, fp)) > 0) {
										if (send(data_sockets[fd], lines, n, 0) < 0) {
											perror("Error sending file");
											exit(1);
										}
										bytes_sent += n;

										bzero(lines, BUFFER_SIZE);
									}

									// checking if all bytes were sent
									if (bytes_sent < file_size) {
										printf("Error not all bytes sent.\n");
										exit(1);
									}

									// closing the data socket
									close(data_sockets[fd]);
									data_sockets[fd] = 0;

									// closing the file
									fclose(fp);

									// sending the 226 reply
									strcpy(reply, "226 Transfer completed.\n");
									send(fd, reply, sizeof(reply), 0);

									// exiting the child process
									exit(EXIT_SUCCESS);
								} 
							} else if (strcmp(client_command, "STOR") == 0) {
								int pid = fork();

								if (pid == 0) {
									// replying with 150 status okay
									char reply[BUFFER_SIZE];
									strcpy(reply, "150 File status okay; about to open. data connection.\n");
									send(fd, reply, sizeof(reply), 0);
									bzero(reply, BUFFER_SIZE);
									
									// creating a data socket to send the file
									data_sockets[fd] = create_data_socket(dcp[fd].IP, dcp[fd].port);

									chdir(user_state[fd].path);

									// receive file from client, and write it to the server
									FILE *fp = fopen(client_argument, "wb");
									// if the file does not exist, then we need to reject the command (for some unforseen reason)
									if (fp == NULL) {
										char reply[BUFFER_SIZE];
										strcpy(reply, "550 No such file or directory.\n");
										send(fd, reply, sizeof(reply), 0);
										close(data_sockets[fd]);
										exit(1);
									}


									// receiving file content
									char stor_received[BUFFER_SIZE];
									bzero(stor_received, sizeof(stor_received));
									while (1) {
										// receiving the file
										int bytes = recv(data_sockets[fd], stor_received, sizeof(stor_received), 0);
										fwrite(stor_received, 1, bytes, fp);
										bzero(stor_received, sizeof(stor_received));

										if (bytes == 0) {
											break;
										}
									}

									// closing the data socket and the file
									close(data_sockets[fd]);
									data_sockets[fd] = 0;
									fclose(fp);

									// sending the 226 reply
									strcpy(reply, "226 Transfer completed.\n");
									send(fd, reply, sizeof(reply), 0);

									// exiting the child process
									exit(EXIT_SUCCESS);
								}
							}
						}
                    }

				}
			}
		}

	}

	// closing the server socket when the server is done
	close(server_sd);
	return 0;
}