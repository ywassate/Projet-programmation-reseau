#include "common.h"
#include "msg_struct.h"
#include <time.h>

#define MAX_CLIENTS 256


// Structures pour stocker les informations des clients des channels et channeluser
struct ClientNode {
    int fd;
    struct sockaddr_in addr;
    char nickname[NICK_LEN];
    char current_channel[INFOS_LEN];
    time_t connection_time;
    struct ClientNode* next;
};
struct ChannelUserNode {
    struct ClientNode *client;  // Pointeur vers le client original
    struct ChannelUserNode *next;
};
struct ChannelNode {
    char name[INFOS_LEN];
    struct ChannelUserNode *users;
    struct ChannelNode *next;
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
    new_node->nickname[0] = '\0';
    new_node->current_channel[0] = '\0';
    new_node->connection_time = time(NULL);
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

//Fonction qui crée une channel user
struct ChannelUserNode* create_channel_user(struct ClientNode *client) {
    struct ChannelUserNode* new_node = malloc(sizeof(struct ChannelUserNode));
    if (new_node == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    new_node->client = client;
    new_node->next = NULL;
    return new_node;
}

// Fonction pour créer un nouveau salon
struct ChannelNode* create_channel_node(const char *name) {
    struct ChannelNode* new_channel = malloc(sizeof(struct ChannelNode));
    if (!new_channel) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    strncpy(new_channel->name, name, INFOS_LEN - 1);
    new_channel->name[INFOS_LEN - 1] = '\0';
    new_channel->users = NULL;  // Liste des utilisateurs est vide
    new_channel->next = NULL;
    return new_channel;
}

// Fonction pour ajouter un salon à la liste des salons
void add_channel(struct ChannelNode **channels, const char *name) {
    struct ChannelNode *new_channel = create_channel_node(name);
    new_channel->next = *channels;
    *channels = new_channel;
}

// Vérifier la validité d'un nom de salon
int is_valid_channel_name(const char *name) {
    if (name == NULL || strlen(name) == 0) return 0;
    for (int i = 0; i < strlen(name); i++) {
        if (!isalnum(name[i])) return 0;
    }
    return 1;
}

// Fonction pour trouver un salon par nom
struct ChannelNode* find_channel_by_name(struct ChannelNode *channels, const char *name) {
    while (channels != NULL) {
        if (strcmp(channels->name, name) == 0) return channels;
        channels = channels->next;
    }
    return NULL;
}

// Fonction pour supprimer un salon
void remove_channel(struct ChannelNode **channels, const char *name) {
    struct ChannelNode *current = *channels, *prev = NULL;
    while (current != NULL && strcmp(current->name, name) != 0) {
        prev = current;
        current = current->next;
    }
    if (current == NULL) return;
    // Libérer tous les nœuds d'utilisateurs du salon
    struct ChannelUserNode *user = current->users;
    while (user != NULL) {
        struct ChannelUserNode *temp = user;
        user = user->next;
        temp->client->current_channel[0] = '\0';  // Réinitialiser le salon actuel
        free(temp);
    }
    if (prev == NULL) {
        *channels = current->next;
    } else {
        prev->next = current->next;
    }
    free(current);
}

// Fonction pour ajouter un utilisateur à un salon
void add_user_to_channel(struct ClientNode *client, struct ChannelNode *channel) {
    if (!client || !channel) return;
    // Vérifier si l'utilisateur n'est pas déjà dans le salon
    struct ChannelUserNode *current = channel->users;
    while (current) {
        if (current->client->fd == client->fd) return;
        current = current->next;
    }
    // Créer un nouveau nœud d'utilisateur de salon
    struct ChannelUserNode *new_user = create_channel_user(client);
    new_user->next = channel->users;
    channel->users = new_user;
}

// Notifier tous les utilisateurs d'un salon
void notify_channel_users(struct ChannelNode *channel, int exclude_fd, const char *message) {
    if (!channel || !message) return;
    struct ChannelUserNode *user = channel->users;
    char buffer[MSG_LEN];
    while (user) {
        if (user->client->fd != exclude_fd) {
            snprintf(buffer, MSG_LEN, "INFO: %s\n", message);
            send(user->client->fd, buffer, strlen(buffer), 0);
        }
        user = user->next;
    }
}

// Fonction pour retirer un utilisateur d'un salon
void remove_user_from_channel(int client_fd, const char *channel_name, struct ChannelNode **channels) {
    struct ChannelNode *channel = find_channel_by_name(*channels, channel_name);
    if (!channel) return;

    struct ChannelUserNode *current = channel->users;
    struct ChannelUserNode *prev = NULL;

    while (current != NULL && current->client->fd != client_fd) {
        prev = current;
        current = current->next;
    }

    if (current != NULL) {
        if (prev == NULL) {
            channel->users = current->next;
        } else {
            prev->next = current->next;
        }
        current->client->current_channel[0] = '\0';  // Réinitialiser le salon actuel du client
        free(current);  // Libérer le nœud d'utilisateur de salon
    }

    // Si le salon est vide après cette opération, le supprimer
    if (channel->users == NULL) {
        char buffer[MSG_LEN];
        snprintf(buffer, MSG_LEN, "INFO: You were the last user in this channel, %s has been destroyed\n", channel->name);
        send(client_fd, buffer, strlen(buffer), 0);
        remove_channel(channels, channel_name);
    }
}

// Joindre un salon
void join_channel(int client_fd, const char *channel_name, struct ClientNode **clients, struct ChannelNode **channels) {
    struct ClientNode *client = find_client_by_fd(*clients, client_fd);
    struct ChannelNode *channel = find_channel_by_name(*channels, channel_name);
    if (!client || !channel) return;
    // Quitter l'ancien salon si l'utilisateur est déjà dans un salon
    if (client->current_channel[0] != '\0') {
        struct ChannelNode *old_channel = find_channel_by_name(*channels, client->current_channel);
        if (old_channel) {
            notify_channel_users(old_channel, client_fd, "has left the channel");
            remove_user_from_channel(client_fd, client->current_channel, channels);
        }
    }
    // Mettre à jour le salon actuel du client
    strncpy(client->current_channel, channel_name, INFOS_LEN - 1);
    client->current_channel[INFOS_LEN - 1] = '\0';
    // Ajouter le client au nouveau salon
    add_user_to_channel(client, channel);
    // Notifier les autres utilisateurs
    char join_msg[MSG_LEN];
    snprintf(join_msg, MSG_LEN, "%s has joined the channel", client->nickname);
    notify_channel_users(channel, client_fd, join_msg);
}

// Fonction pour compter les utilisateurs dans un salon
int count_users_in_channel(struct ChannelNode *channel) {
    int count = 0;
    struct ChannelUserNode *user = channel->users;
    while (user != NULL) {
        count++;
        user = user->next;
    }
    return count;
}

//Fonction pour gérer les messages des clients
void handle_client_message(int client_fd, char *buffer, struct ClientNode **clients, struct ChannelNode **channels) {
    struct message msg;
    memcpy(&msg, buffer, sizeof(struct message));

    struct ClientNode* client = find_client_by_fd(*clients, client_fd);
    if (client == NULL) return;

    if (msg.type == NICKNAME_NEW) {
        if (nickname_exists(*clients, msg.infos)) {
            snprintf(buffer, MSG_LEN, "Nickname %s is already in use.\n", msg.infos);
            send(client_fd, buffer, strlen(buffer), 0);
        } else {
            strncpy(client->nickname, msg.infos, NICK_LEN - 1);
            client->nickname[NICK_LEN - 1] = '\0';
            snprintf(buffer, MSG_LEN, "Welcome %s!\n", client->nickname);
            send(client_fd, buffer, strlen(buffer), 0);
        }
    } else if (msg.type == NICKNAME_LIST) {
        struct ClientNode* current = *clients;
        snprintf(buffer, MSG_LEN, "Online users are:\n");
        send(client_fd, buffer, strlen(buffer), 0);
        while (current != NULL) {
            if (strlen(current->nickname) > 0) {
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
        struct ClientNode* sender = find_client_by_fd(*clients, client_fd);
        struct ClientNode* target = find_client_by_nickname(*clients, msg.infos);
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
                
                // Envoi du message broadcast à tous les autres clients
                struct ClientNode* current = *clients;
                while (current != NULL) {
                    if (current->fd != client_fd && strlen(current->nickname) > 0) {
                        send(current->fd, formatted_msg, strlen(formatted_msg), 0);
                    }
                    current = current->next;
                }
                
                // Confirmation à l'émetteur
                snprintf(buffer, MSG_LEN, "[Message broadcast to all users]\n");
                send(client_fd, buffer, strlen(buffer), 0);
            }
        }
    } else if (msg.type == MULTICAST_QUIT) {
        if (strlen(msg.infos) > 0 && strcmp(client->current_channel, msg.infos) == 0) {
            struct ChannelNode* channel = find_channel_by_name(*channels, msg.infos);
            if (channel) {
                char leave_msg[MSG_LEN];
                snprintf(leave_msg, MSG_LEN, "%s has left the channel", client->nickname);
                notify_channel_users(channel, client_fd, leave_msg);
                remove_user_from_channel(client_fd, msg.infos, channels);
                memset(client->current_channel, 0, INFOS_LEN);
                snprintf(buffer, MSG_LEN, "You have left the channel %s.\n", msg.infos);
                send(client_fd, buffer, strlen(buffer), 0);
            }
        } else {
            snprintf(buffer, MSG_LEN, "You have disconnected from the server.\n");
            send(client_fd, buffer, strlen(buffer), 0);
            close(client_fd);
            remove_client(clients, client_fd);
        }
    } else if (msg.type == MULTICAST_CREATE) {
        if (!is_valid_channel_name(msg.infos)) {
            snprintf(buffer, MSG_LEN, "Invalid channel name. Only alphanumeric characters are allowed.\n");
            send(client_fd, buffer, strlen(buffer), 0);
        } else if (find_channel_by_name(*channels, msg.infos)) {
            snprintf(buffer, MSG_LEN, "Channel %s already exists.\n", msg.infos);
            send(client_fd, buffer, strlen(buffer), 0);
        } else {
            add_channel(channels, msg.infos);
            join_channel(client_fd, msg.infos, clients, channels);
            snprintf(buffer, MSG_LEN, "You have created and joined the channel %s.\n", msg.infos);
            send(client_fd, buffer, strlen(buffer), 0);
        }
    } else if (msg.type == MULTICAST_JOIN) {
        struct ChannelNode* channel = find_channel_by_name(*channels, msg.infos);
        if (!channel) {
            snprintf(buffer, MSG_LEN, "Channel %s does not exist.\n", msg.infos);
            send(client_fd, buffer, strlen(buffer), 0);
        } else {
            if (strlen(client->current_channel) > 0) {
                struct ChannelNode* old_channel = find_channel_by_name(*channels, client->current_channel);
                if (old_channel) {
                    char leave_msg[MSG_LEN];
                    snprintf(leave_msg, MSG_LEN, "%s has left the channel", client->nickname);
                    notify_channel_users(old_channel, client_fd, leave_msg);
                }
                remove_user_from_channel(client_fd, client->current_channel, channels);
                memset(client->current_channel, 0, INFOS_LEN);
            }
            join_channel(client_fd, msg.infos, clients, channels);
            snprintf(buffer, MSG_LEN, "You have joined the channel %s.\n", msg.infos);
            send(client_fd, buffer, strlen(buffer), 0);
        }
    } else if (msg.type == MULTICAST_LIST) {
        struct ChannelNode *current = *channels;
        snprintf(buffer, MSG_LEN, "Available channels:\n");
        send(client_fd, buffer, strlen(buffer), 0);
        
        while (current) {
            int user_count = count_users_in_channel(current);
            snprintf(buffer, MSG_LEN, "- %s (%d users)\n", current->name, user_count);
            send(client_fd, buffer, strlen(buffer), 0);
            current = current->next;
        }
    } else if (msg.type == MULTICAST_SEND) {
        struct ClientNode* sender = find_client_by_fd(*clients, client_fd);

        if (sender->current_channel[0] == '\0') {
            snprintf(buffer, MSG_LEN, "You are not in any channel. Join a channel to send messages.\n");
            send(client_fd, buffer, strlen(buffer), 0);
            return;
        }

        struct ChannelNode* channel = find_channel_by_name(*channels, sender->current_channel);
        if (!channel) {
            snprintf(buffer, MSG_LEN, "Channel %s no longer exists.\n", sender->current_channel);
            send(client_fd, buffer, strlen(buffer), 0);
            return;
        }

        char message_content[MSG_LEN];
        memset(message_content, 0, MSG_LEN);
        int recv_len = recv(client_fd, message_content, msg.pld_len, 0);
        if (recv_len <= 0) return;

        char formatted_msg[MSG_LEN];
        size_t header_len = snprintf(formatted_msg, MSG_LEN, 
            "[%s in %s]: ", sender->nickname, channel->name);
        
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
            
            // Envoi du message aux autres utilisateurs du canal
            struct ChannelUserNode* user = channel->users;
            while (user) {
                if (user->client->fd != client_fd) {
                    send(user->client->fd, formatted_msg, strlen(formatted_msg), 0);
                }
                user = user->next;
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

// Libération mémoire de la liste des clients et salons
void free_client_list(struct ClientNode* head) {
    while (head != NULL) {
        struct ClientNode* temp = head;
        head = head->next;
        free(temp);
    }
}
void free_channel_list(struct ChannelNode* head) {
    while (head != NULL) {
        struct ChannelNode* temp = head;
        head = head->next;
        free(temp);
    }
}

// Fonction principale du serveur
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct ClientNode *clients = NULL;
    struct ChannelNode *channels = NULL;
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
                                add_client(&clients, new_fd, client_addr);  // Ajouter le client à la liste
                                break;
                            }
                        }
                    }
                } else {
                    memset(buffer, 0, MSG_LEN);
                    int bytes_received = recv(fds[i].fd, buffer, sizeof(struct message), 0);

                    if (bytes_received == 0) {
                        printf("Client %d disconnected\n", fds[i].fd);
                        close(fds[i].fd);
                        fds[i].fd = -1;
                        remove_client(&clients, fds[i].fd);  // Retirer le client de la liste
                    } else if (bytes_received < 0) {
                        perror("recv");
                    } else {
                        handle_client_message(fds[i].fd, buffer, &clients, &channels);  // Gérer le message du client
                    }
                }
            }
        }
    }
    
    free_client_list(clients);  // Libérer la mémoire des clients
    free_channel_list(channels);  // Libérer la mémoire des salons
    close(server_fd);  // Fermer le socket serveur
    return EXIT_SUCCESS;
}