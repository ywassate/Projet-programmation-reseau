#include "common.h"
#define TIMEOUT_DURATION 10 * 60 * 1000  // Timeout de 10 minutes en millisecondes

// Fonction pour gérer la connexion au serveur
int handle_connect(const char *server_name, const char *server_port) {
    struct addrinfo hints, *server_info, *current;
    int connection_fd;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;  // Accepte IPv4 ou IPv6
    hints.ai_socktype = SOCK_STREAM;  // Utilise TCP

    // Résolution du nom de domaine et du port
    if (getaddrinfo(server_name, server_port, &hints, &server_info) != 0) {
        perror("Error resolving server name");
        exit(EXIT_FAILURE);
    }

    // Tentative de connexion à l'une des adresses retournées
    for (current = server_info; current != NULL; current = current->ai_next) {
        connection_fd = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        if (connection_fd == -1) {
            continue;  // Essaye l'adresse suivante si la création du socket échoue
        }

        // Tente de se connecter au serveur
        if (connect(connection_fd, current->ai_addr, current->ai_addrlen) != -1) {
            break;  // Connexion réussie
        }

        close(connection_fd);
    }

    if (current == NULL) {
        fprintf(stderr, "Failed to connect to the server\n");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(server_info);  // Libère les informations d'adresse
    return connection_fd;
}

// Fonction pour gérer les événements de communication
void communication_handler(int sock_fd) {
    char buffer[MSG_LEN];
    struct pollfd pfds[2];

    // Prépare poll pour surveiller le socket et l'entrée standard (clavier)
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

        // Timeout atteint
        if (poll_result == 0) {
            printf("Inactivity timeout, disconnecting...\n");
            break;
        }

        // Vérifie si le serveur a envoyé un message
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
            printf("Message from server: %s", buffer);
        }

        // Vérifie si l'utilisateur a saisi un message
        if (pfds[1].revents & POLLIN) {
            memset(buffer, 0, MSG_LEN);
            fgets(buffer, MSG_LEN, stdin);

            // Si l'utilisateur tape "/quit", on arrête la boucle
            if (strcmp(buffer, "/quit\n") == 0) {
                printf("Disconnecting from server...\n");
                running = 0;
                break;
            }

            // Envoie du message au serveur
            if (send(sock_fd, buffer, strlen(buffer), 0) == -1) {
                perror("Error sending data");
                break;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <server_name> <server_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Connexion au serveur
    int sock_fd = handle_connect(argv[1], argv[2]);
    printf("Successfully connected to the server\n");

    // Gérer la communication avec le serveur
    communication_handler(sock_fd);

    // Fermeture de la connexion
    close(sock_fd);
    return EXIT_SUCCESS;
}
