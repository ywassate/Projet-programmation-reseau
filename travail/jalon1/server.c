#include "common.h"
#define MAX_CLIENTS 256

// Structure pour stocker les informations des clients
struct ClientNode {
    int fd;
    struct sockaddr_in addr;
    struct ClientNode* next;
};

// Fonction pour créer un nouveau nœud client
struct ClientNode* create_client_node(int fd, struct sockaddr_in addr) {
    // Allouer de la mémoire pour un nouveau nœud client
    struct ClientNode* new_node = malloc(sizeof(struct ClientNode));
    if (new_node == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
     // Initialiser les champs du nouveau nœud
    new_node->fd = fd;
    new_node->addr = addr;
    new_node->next = NULL;
    return new_node;
}

// Fonction pour ajouter un client à la liste
void add_client(struct ClientNode** head, int fd, struct sockaddr_in addr) {
    struct ClientNode* new_node = create_client_node(fd, addr);
    new_node->next = *head;
    *head = new_node; // Le nouveau nœud devient la tête de la liste
}

// Fonction pour supprimer un client de la liste
void remove_client(struct ClientNode** head, int fd) {
    struct ClientNode *current = *head, *prev = NULL;
    while (current != NULL && current->fd != fd) {
        prev = current;
        current = current->next;
    }
    if (current == NULL) return;
    // Ajuster les pointeurs pour "sauter" le nœud à supprimer
    if (prev == NULL) {
        *head = current->next;
    } else {
        prev->next = current->next;
    }
    free(current);
}

// Fonction pour libérer la mémoire de la liste des clients
void free_client_list(struct ClientNode* head) {
    while (head != NULL) {
        struct ClientNode* temp = head;
        head = head->next;
        free(temp);
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

// Fonction pour gérer les messages des clients
void handle_client_message(int client_fd, char *buffer, struct ClientNode **clients) {
    ssize_t bytes_received = recv(client_fd, buffer, MSG_LEN, 0);
    if (bytes_received <= 0) {
        if (bytes_received == 0) {
           printf("Client with ID %d has disconnected\n", client_fd);
        } else {
            perror("recv");
        }
        close(client_fd);
        remove_client(clients, client_fd);
    } else {
        printf("Received from client with ID %d : %s", client_fd, buffer);
        // Echo le message reçu au client
        if (send(client_fd, buffer, bytes_received, 0) == -1) {
            perror("send");
        }
        if (strncmp(buffer, "/quit", 5) == 0) {
            printf("Client  with ID %d requested to quit\n", client_fd);
            close(client_fd);
            remove_client(clients, client_fd);
        }
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
                    // Nouvelle connexion
                    struct sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    int new_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
                    if (new_fd == -1) {
                        perror("accept");
                    } else {
                        printf("New connection from %s:%d (Client ID: %d)\n", 
                            inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), new_fd);
                        // Ajouter le nouveau client au tableau poll et à la liste des clients
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
                    // Gèrer les messages des clients existants
                    memset(buffer, 0, MSG_LEN);
                    handle_client_message(fds[i].fd, buffer, &clients);
                    if (buffer[0] == '\0') {
                        fds[i].fd = -1; // Marquer le descripteur comme libre si le client s'est déconnecté
                    }
                }
            }
        }
    }
    //Nettoyage 
    free_client_list(clients);
    close(server_fd);
    return EXIT_SUCCESS;
}