#include "common.h"
#include "msg_struct.h"
#define TIMEOUT_DURATION 10 * 60 * 1000
char client_nickname[NICK_LEN] = "";

// Fonction de connexion au serveur
int handle_connect(const char *server_name, const char *server_port) {
    struct addrinfo hints, *server_info, *current;
    int connection_fd;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(server_name, server_port, &hints, &server_info) != 0) {
        perror("Error resolving server name");
        exit(EXIT_FAILURE);
    }

    for (current = server_info; current != NULL; current = current->ai_next) {
        connection_fd = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        if (connection_fd == -1) continue;

        if (connect(connection_fd, current->ai_addr, current->ai_addrlen) != -1) break;
        close(connection_fd);
    }

    if (current == NULL) {
        fprintf(stderr, "Failed to connect to the server\n");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(server_info);
    return connection_fd;
}

void communication_handler(int sock_fd) {
    char buffer[MSG_LEN];
    struct pollfd pfds[2];
    
    // Configuration des descripteurs pour poll
    pfds[0].fd = sock_fd;
    pfds[0].events = POLLIN;
    pfds[1].fd = STDIN_FILENO;
    pfds[1].events = POLLIN;

    char current_channel[INFOS_LEN] = "";  // Stocke le nom du salon actuel
    int running = 1;

    while (running) {
        int poll_result = poll(pfds, 2, TIMEOUT_DURATION);
        
        if (poll_result == -1) {
            perror("Poll error"); 
            exit(EXIT_FAILURE);
        }
        if (poll_result == 0) {
            printf("Inactivity timeout, disconnecting...\n");
            break;
        }

        // Réception de messages du serveur
        if (pfds[0].revents & POLLIN) {
            memset(buffer, 0, MSG_LEN);
            ssize_t received_bytes = recv(pfds[0].fd, buffer, MSG_LEN, 0);
            if (received_bytes <= 0) {
                if (received_bytes == 0) {
                    printf("Server closed the connection\n");
                } else {
                    perror("Error receiving data");
                }
                break;
            }
            printf("%s", buffer);
        }

        // Envoi de messages depuis le client
        if (pfds[1].revents & POLLIN) {
            memset(buffer, 0, MSG_LEN);
            if (fgets(buffer, MSG_LEN, stdin) == NULL) {
                continue;
            }
            buffer[strcspn(buffer, "\n")] = 0;  // Supprime le \n final

            if (buffer[0] == '/') {
                struct message msg;
                memset(&msg, 0, sizeof(struct message));
                strncpy(msg.nick_sender, client_nickname, NICK_LEN - 1);

                char *command = strtok(buffer, " ");
                char *arg = strtok(NULL, "\n");

                if (strcmp(command, "/quit") == 0) {
                    if (strlen(current_channel) > 0) {
                        msg.type = MULTICAST_QUIT;
                        strncpy(msg.nick_sender, client_nickname, NICK_LEN - 1);
                        strncpy(msg.infos, current_channel, INFOS_LEN - 1);
                        send(sock_fd, &msg, sizeof(msg), 0);
                        current_channel[0] = '\0';
                    } else {
                        printf("Disconnecting from server...\n");
                        running = 0;
                        break;
                    }
                }
                else if (strcmp(command, "/nick") == 0) {
                    if (arg == NULL || strlen(arg) == 0) {
                        printf("Usage: /nick <nickname>\n");
                        continue;
                    }
                    msg.type = NICKNAME_NEW;
                    strncpy(msg.infos, arg, INFOS_LEN - 1);
                    send(sock_fd, &msg, sizeof(msg), 0);
                }
                else if (strcmp(command, "/who") == 0) {
                    msg.type = NICKNAME_LIST;
                    send(sock_fd, &msg, sizeof(msg), 0);
                }
                else if (strcmp(command, "/whois") == 0) {
                    if (arg == NULL || strlen(arg) == 0) {
                        printf("Usage: /whois <nickname>\n");
                        continue;
                    }
                    msg.type = NICKNAME_INFOS;
                    strncpy(msg.infos, arg, INFOS_LEN - 1);
                    send(sock_fd, &msg, sizeof(msg), 0);
                }
                else if (strcmp(command, "/create") == 0) {
                    if (arg == NULL || strlen(arg) == 0) {
                        printf("Usage: /create <channel_name>\n");
                        continue;
                    }
                    msg.type = MULTICAST_CREATE;
                    strncpy(msg.infos, arg, INFOS_LEN - 1);
                    send(sock_fd, &msg, sizeof(msg), 0);
                    strncpy(current_channel, arg, INFOS_LEN - 1);
                }
                else if (strcmp(command, "/channel_list") == 0) {
                    msg.type = MULTICAST_LIST;
                    send(sock_fd, &msg, sizeof(msg), 0);
                }
                else if (strcmp(command, "/join") == 0) {
                    if (arg == NULL || strlen(arg) == 0) {
                        printf("Usage: /join <channel_name>\n");
                        continue;
                    }
                    msg.type = MULTICAST_JOIN;
                    
                    strncpy(msg.infos, arg, INFOS_LEN - 1);
                    send(sock_fd, &msg, sizeof(msg), 0);
                    strncpy(current_channel, arg, INFOS_LEN - 1);
                }
                else if (strcmp(command, "/msgall") == 0) {
                    if (arg == NULL || strlen(arg) == 0) {
                        printf("Usage: /msgall <message>\n");
                        continue;
                    }
                    
                    msg.type = BROADCAST_SEND;
                    msg.pld_len = strlen(arg);
                    
                    // Envoi de la structure du message
                    if (send(sock_fd, &msg, sizeof(struct message), 0) < 0) {
                        perror("Error sending broadcast message structure");
                        continue;
                    }
                    
                    // Envoi du contenu du message
                    if (send(sock_fd, arg, msg.pld_len, 0) < 0) {
                        perror("Error sending broadcast message content");
                        continue;
                    }
                }
                else if (strcmp(command, "/msg") == 0) {
                    char *target = strtok(arg, " ");
                    char *message = strtok(NULL, "\n");
                    
                    if (target == NULL || message == NULL) {
                        printf("Usage: /msg <nickname> <message>\n");
                        continue;
                    }

                    msg.type = UNICAST_SEND;
                    msg.pld_len = strlen(message);
                    strncpy(msg.infos, target, INFOS_LEN - 1);
                    
                    // Envoi de la structure du message
                    if (send(sock_fd, &msg, sizeof(struct message), 0) < 0) {
                        perror("Error sending private message structure");
                        continue;
                    }
                    
                    // Envoi du contenu du message
                    if (send(sock_fd, message, msg.pld_len, 0) < 0) {
                        perror("Error sending private message content");
                        continue;
                    }
                    printf("Private message sent to %s\n", target);
                }
                else {
                    printf("Unknown command: %s\n", command);
                }
            }
            else {
                // Message normal (dans le salon ou echo)
                struct message msg;
                memset(&msg, 0, sizeof(struct message));
                strncpy(msg.nick_sender, client_nickname, NICK_LEN - 1);
                msg.pld_len = strlen(buffer);

                if (strlen(current_channel) > 0) {
                    // L'utilisateur est dans un salon -> message multicast
                    msg.type = MULTICAST_SEND;
                    strncpy(msg.infos, current_channel, INFOS_LEN - 1);
                } else {
                    // L'utilisateur n'est pas dans un salon -> echo
                    msg.type = ECHO_SEND;
                }

                // Envoi de la structure du message
                if (send(sock_fd, &msg, sizeof(struct message), 0) < 0) {
                    perror("Error sending message structure");
                    continue;
                }

                // Envoi du contenu du message
                if (send(sock_fd, buffer, msg.pld_len, 0) < 0) {
                    perror("Error sending message content");
                    continue;
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <server_name> <server_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sock_fd = handle_connect(argv[1], argv[2]);
    printf("Successfully connected to the server\n");
    communication_handler(sock_fd);
    close(sock_fd);
    return EXIT_SUCCESS;
}