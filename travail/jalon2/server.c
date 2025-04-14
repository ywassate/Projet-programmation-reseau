#include "common.h"
#include "msg_struct.h"
#include <time.h>  // Ajout pour la fonction time() et ctime()
#define MAX_CLIENTS 256

// Structure pour stocker les informations des client
struct ClientNode {
    int fd;
    struct sockaddr_in addr;
    char nickname[NICK_LEN];
    time_t connection_time;  // Pour stocker la date de connexion
    struct ClientNode* next;
};

// Fonction pour créer un nouveau nœud client
struct ClientNode* create_client_node(int fd, struct sockaddr_in addr) {
    struct ClientNode* new_node = malloc(sizeof(struct ClientNode));
    if (new_node == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    new_node->fd = fd;
    new_node->addr = addr;
    new_node->nickname[0] = '\0';  // Pas de pseudo au départ
    new_node->connection_time = time(NULL); // Enregistrer l'heure de connexion
    new_node->next = NULL;
    return new_node;
}

// Fonction pour ajouter un client à la liste
void add_client(struct ClientNode** head, int fd, struct sockaddr_in addr) {
    struct ClientNode* new_node = create_client_node(fd, addr);
    new_node->next = *head;
    *head = new_node;
}

// Fonction pour trouver un client par FD
struct ClientNode* find_client_by_fd(struct ClientNode* head, int fd) {
    while (head != NULL) {
        if (head->fd == fd) return head;
        head = head->next;
    }
    return NULL;
}

// Fonction pour trouver un client par pseudo
struct ClientNode* find_client_by_nickname(struct ClientNode* head, const char* nickname) {
    while (head != NULL) {
        if (strcmp(head->nickname, nickname) == 0) return head;
        head = head->next;
    }
    return NULL;
}

// Fonction pour vérifier si un pseudo existe
int nickname_exists(struct ClientNode* head, const char* nickname) {
    return find_client_by_nickname(head, nickname) != NULL;
}

// Fonction pour supprimer un client de la liste
void remove_client(struct ClientNode** head, int fd) {
    struct ClientNode *current = *head, *prev = NULL;
    while (current != NULL && current->fd != fd) {
        prev = current;
        current = current->next;
    }
    if (current == NULL) return;
    if (prev == NULL) {
        *head = current->next;
    } else {
        prev->next = current->next;
    }
    free(current);
}

// Fonction pour gérer les commandes des clients
void handle_client_message(int client_fd, char *buffer, struct ClientNode **clients) {
    struct message msg;
    memcpy(&msg, buffer, sizeof(struct message));

    struct ClientNode* client = find_client_by_fd(*clients, client_fd);
    if (client == NULL) return;

    if (msg.type == NICKNAME_NEW) {
        if (nickname_exists(*clients, msg.infos)) {
            snprintf(buffer, MSG_LEN, "Nickname %s is already in use.\n", msg.infos);
            send(client_fd, buffer, strlen(buffer), 0);
        } else {
            strncpy(client->nickname, msg.infos, NICK_LEN);
            snprintf(buffer, MSG_LEN, "Welcome %s!\n", client->nickname);
            send(client_fd, buffer, strlen(buffer), 0);
        }
    } else if (msg.type == NICKNAME_LIST) {
        struct ClientNode* current = *clients;
        snprintf(buffer, MSG_LEN, "Online users are:\n");
        send(client_fd, buffer, strlen(buffer), 0);
        while (current != NULL) {
            if (strlen(current->nickname) > 0) {  // N'affiche que les users avec un pseudo
                snprintf(buffer, MSG_LEN, "- %s\n", current->nickname);
                send(client_fd, buffer, strlen(buffer), 0);
            }
            current = current->next;
        }
    } else if (msg.type == NICKNAME_INFOS) {
       struct ClientNode* target = find_client_by_nickname(*clients, msg.infos);
        if (target != NULL) {
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(target->addr.sin_addr), ip, INET_ADDRSTRLEN);
            
            // Convertir le time_t en chaîne de caractères et supprimer le retour à la ligne
            char time_str[26];
            strncpy(time_str, ctime(&(target->connection_time)), sizeof(time_str) - 1);
            time_str[strlen(time_str) - 1] = '\0';
            
            snprintf(buffer, MSG_LEN, "[Server] : %s connected since %s from %s:%d\n",
                    target->nickname,
                    time_str,
                    ip,
                    ntohs(target->addr.sin_port));
            send(client_fd, buffer, strlen(buffer), 0);
        } else {
            snprintf(buffer, MSG_LEN, "[Server] : User %s not found.\n", msg.infos);
            send(client_fd, buffer, strlen(buffer), 0);
        }


    } else if (msg.type == UNICAST_SEND) {
// Trouver l'émetteur
    struct ClientNode* sender = find_client_by_fd(*clients, client_fd);
    if (!sender) return;
    
    // Trouver le destinataire
    struct ClientNode* target = find_client_by_nickname(*clients, msg.infos);
    
    // Récupérer le contenu du message
    char message_content[MSG_LEN];
    memset(message_content, 0, MSG_LEN);
    int recv_len = recv(client_fd, message_content, msg.pld_len, 0);
    
    if (recv_len > 0) {
        if (target != NULL) {
            char formatted_msg[MSG_LEN];
            size_t header_len = snprintf(formatted_msg, MSG_LEN, 
                "[Private message from %s] : ", sender->nickname);
            
            if (header_len < MSG_LEN) {
                size_t remaining = MSG_LEN - header_len - 2; // -2 pour \n et \0
                size_t content_len = strlen(message_content);
                if (content_len > remaining) {
                    content_len = remaining;
                }
                strncpy(formatted_msg + header_len, message_content, content_len);
                header_len += content_len;
                formatted_msg[header_len] = '\n';
                formatted_msg[header_len + 1] = '\0';
                
                send(target->fd, formatted_msg, strlen(formatted_msg), 0);
                
                // Confirmation à l'émetteur
                snprintf(formatted_msg, MSG_LEN, "[Message sent to %s]\n", target->nickname);
                send(client_fd, formatted_msg, strlen(formatted_msg), 0);
            }
        } else {
            char error_msg[MSG_LEN];
            snprintf(error_msg, MSG_LEN, "[Server] : User %s does not exist\n", msg.infos);
            send(client_fd, error_msg, strlen(error_msg), 0);
        }
    }
    } else if (msg.type == BROADCAST_SEND) {
    struct ClientNode* sender = find_client_by_fd(*clients, client_fd);
    if (!sender) return;
    
    char message_content[MSG_LEN];
    memset(message_content, 0, MSG_LEN);
    int recv_len = recv(client_fd, message_content, msg.pld_len, 0);
    
    if (recv_len > 0) {
        char formatted_msg[MSG_LEN];
        size_t header_len = snprintf(formatted_msg, MSG_LEN, 
            "[Broadcast from %s] : ", sender->nickname);
        
        if (header_len < MSG_LEN) {
            size_t remaining = MSG_LEN - header_len - 2; // -2 pour \n et \0
            size_t content_len = strlen(message_content);
            if (content_len > remaining) {
                content_len = remaining;
            }
            strncpy(formatted_msg + header_len, message_content, content_len);
            header_len += content_len;
            formatted_msg[header_len] = '\n';
            formatted_msg[header_len + 1] = '\0';
            
            // Envoyer à tous les autres clients
            struct ClientNode* current = *clients;
            while (current != NULL) {
                if (current->fd != client_fd && strlen(current->nickname) > 0) {
                    send(current->fd, formatted_msg, strlen(formatted_msg), 0);
                }
                current = current->next;
            }
            
            // Confirmation à l'expéditeur
            char confirm_msg[MSG_LEN];
            snprintf(confirm_msg, MSG_LEN, "[Message broadcast to all users]\n");
            send(client_fd, confirm_msg, strlen(confirm_msg), 0);
        }
    }
    } else if (msg.type == ECHO_SEND) {
   char message_content[MSG_LEN];
    memset(message_content, 0, MSG_LEN);
    int recv_len = recv(client_fd, message_content, msg.pld_len, 0);
    if (recv_len > 0) {
        char echo_msg[MSG_LEN];
        size_t header_len = snprintf(echo_msg, MSG_LEN, "[Echo] ");
        
        if (header_len < MSG_LEN) {
            size_t remaining = MSG_LEN - header_len - 1; // -1 pour \0
            size_t content_len = strlen(message_content);
            if (content_len > remaining) {
                content_len = remaining;
            }
            strncpy(echo_msg + header_len, message_content, content_len);
            echo_msg[MSG_LEN - 1] = '\0';
            send(client_fd, echo_msg, strlen(echo_msg), 0);
        }
    }
    }
}

// Fonction pour créer et lier le socket serveur
int create_and_bind_socket(const char *port) {
    struct addrinfo hints, *result, *rp;
    int sfd;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, port, &hints, &result) != 0) {
        perror("getaddrinfo()");
        exit(EXIT_FAILURE);
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1) continue;

        if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1) {
            perror("setsockopt");
            close(sfd);
            continue;
        }
        if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(sfd);
    }
    freeaddrinfo(result);
    if (rp == NULL) {
        fprintf(stderr, "Could not bind\n");
        exit(EXIT_FAILURE);
    }
    return sfd;
}
// Fonction pour libérer la mémoire de la liste des clients
void free_client_list(struct ClientNode* head) {
    while (head != NULL) {
        struct ClientNode* temp = head;
        head = head->next;
        free(temp);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    struct ClientNode *clients = NULL;
    struct pollfd fds[MAX_CLIENTS];
    char buffer[MSG_LEN];
    
    int server_fd = create_and_bind_socket(argv[1]);

    if (listen(server_fd, SOMAXCONN) != 0) {
        perror("listen()");
        exit(EXIT_FAILURE);
    }
    memset(fds, -1, sizeof(fds));
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;
    printf("Server listening on port %s\n", argv[1]);
    while (1) {
        int poll_count = poll(fds, MAX_CLIENTS, -1);
        if (poll_count == -1) {
            perror("poll");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == server_fd) {
                    struct sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    int new_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
                    if (new_fd == -1) {
                        perror("accept");
                    } else {
                        printf("New connection from %s:%d (Client ID: %d)\n", 
                               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), new_fd);
                        for (int j = 1; j < MAX_CLIENTS; j++) {
                            if (fds[j].fd == -1) {
                                fds[j].fd = new_fd;
                                fds[j].events = POLLIN;
                                add_client(&clients, new_fd, client_addr);
                                break;
                            }
                        }
                    }
                } else {
                   memset(buffer, 0, MSG_LEN);
                    int bytes_received = recv(fds[i].fd, buffer, sizeof(struct message), 0);

                    if (bytes_received == 0) {
                        // Le client a fermé la connexion
                        printf("Client %d disconnected\n", fds[i].fd);
                        close(fds[i].fd);
                        fds[i].fd = -1;  // Désactiver ce descripteur dans fds
                        remove_client(&clients, fds[i].fd);  // Retirer le client de la liste
                    } else if (bytes_received < 0) {
                        perror("recv");
                    } else {
                        // Gérer le message du client
                        handle_client_message(fds[i].fd, buffer, &clients);
                    }
                }
            }
        }
    }
    
    free_client_list(clients);
    close(server_fd);
    return EXIT_SUCCESS;
}