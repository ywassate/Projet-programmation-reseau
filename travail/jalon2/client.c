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

// Gestion de la communication client-serveur
void communication_handler(int sock_fd) {
    char buffer[MSG_LEN];
    struct pollfd pfds[2];
    pfds[0].fd = sock_fd;
    pfds[0].events = POLLIN;
    pfds[1].fd = STDIN_FILENO;
    pfds[1].events = POLLIN;

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

        // Gestion des messages du serveur
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

if (pfds[1].revents & POLLIN) {
    memset(buffer, 0, MSG_LEN);
    fgets(buffer, MSG_LEN, stdin);

    // Si c'est un message qui commence par '/'
    if (buffer[0] == '/') {
        struct message msg;
        msg.pld_len = 0;
        msg.nick_sender[0] = '\0';
        msg.infos[0] = '\0';

        // Extraction de la commande et des arguments
        char *command = strtok(buffer, " \n");
        char *arg = strtok(NULL, "\n");

        if (strcmp(command, "/quit") == 0) {
            printf("Disconnecting from server...\n");
            running = 0;
            break;
        }
        else if (strcmp(command, "/nick") == 0) {
            msg.type = NICKNAME_NEW;
            if (arg == NULL || strlen(arg) == 0) {
                printf("Usage: /nick <nickname>\n");
                continue;
            }
            strncpy(msg.infos, arg, NICK_LEN);
            send(sock_fd, &msg, sizeof(msg), 0);
        }
        else if (strcmp(command, "/who") == 0) {
            msg.type = NICKNAME_LIST;
            send(sock_fd, &msg, sizeof(msg), 0);
        }
        else if (strcmp(command, "/whois") == 0) {
            msg.type = NICKNAME_INFOS;
            if (arg == NULL || strlen(arg) == 0) {
                printf("Usage: /whois <nickname>\n");
                continue;
            }
            strncpy(msg.infos, arg, NICK_LEN);
            send(sock_fd, &msg, sizeof(msg), 0);
        }
        else if (strcmp(command, "/msgall") == 0) {
            if (arg == NULL || strlen(arg) == 0) {
                printf("Usage: /msgall <message>\n");
                continue;
            }
            
            // Initialiser la structure message
            memset(&msg, 0, sizeof(struct message));
            msg.type = BROADCAST_SEND;
            msg.pld_len = strlen(arg);
            
            // S'assurer que le nickname est copié dans le message
            strncpy(msg.nick_sender, client_nickname, NICK_LEN - 1);
            
            // Envoyer d'abord la structure du message
            if (send(sock_fd, &msg, sizeof(struct message), 0) < 0) {
                perror("Error sending message structure");
                continue;
            }
            
            // Envoyer ensuite le contenu du message
            if (send(sock_fd, arg, msg.pld_len, 0) < 0) {
                perror("Error sending message content");
                continue;
            }
        }
        else if (strcmp(command, "/msg") == 0) {
            msg.type = UNICAST_SEND;
            char *target = strtok(arg, " ");
            char *message = strtok(NULL, "\n");
            if (target == NULL || message == NULL) {
                printf("Usage: /msg <nickname> <message>\n");
                continue;
            }
            
            // Initialiser proprement la structure message
            memset(&msg, 0, sizeof(struct message));
            msg.type = UNICAST_SEND;
            msg.pld_len = strlen(message);
            
            // Copier le nickname de l'émetteur et du destinataire
            strncpy(msg.nick_sender, client_nickname, NICK_LEN - 1);
            strncpy(msg.infos, target, INFOS_LEN - 1);
            
            // Envoyer la structure du message
            if (send(sock_fd, &msg, sizeof(struct message), 0) < 0) {
                perror("Error sending message structure");
                continue;
            }
            
            // Envoyer le contenu du message
            if (send(sock_fd, message, msg.pld_len, 0) < 0) {
                perror("Error sending message content");
                continue;
            }
        }
        else {
            // Commande inconnue, on l'envoie comme un message echo
            msg.type = ECHO_SEND;
            msg.pld_len = strlen(buffer);
            send(sock_fd, &msg, sizeof(msg), 0);
            send(sock_fd, buffer, msg.pld_len, 0);
        }
    }
    else {
        // Message normal (sans '/')
        struct message msg;
        msg.type = ECHO_SEND;
        msg.pld_len = strlen(buffer);
        msg.nick_sender[0] = '\0';
        msg.infos[0] = '\0';
        
        send(sock_fd, &msg, sizeof(msg), 0);
        send(sock_fd, buffer, msg.pld_len, 0);
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