#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define USER_FILE "users.txt"

typedef struct {
    int socket;
    struct sockaddr_in address;
    char username[32];
    char password[32];
    char name[32];
    char surname[32];
    char mood[32];
    int online;
} Client;

Client clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void load_users() {
    FILE *file = fopen(USER_FILE, "r");
    if (file == NULL) return;
    while (fscanf(file, "%s %s %s %s", clients[client_count].username, clients[client_count].password, clients[client_count].name, clients[client_count].surname) != EOF) {
        clients[client_count].online = 0;
        client_count++;
    }
    fclose(file);
}

void save_user(Client *client) {
    FILE *file = fopen(USER_FILE, "a");
    if (file != NULL) {
        fprintf(file, "%s %s %s %s\n", client->username, client->password, client->name, client->surname);
        fclose(file);
    }
}

Client* find_client_by_username(const char *username) {
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].username, username) == 0) {
            return &clients[i];
        }
    }
    return NULL;
}

void process_command(Client *client, char *command) {
    char response[BUFFER_SIZE] = {0};

    if (strncmp(command, "REGISTER", 8) == 0) {
        char username[32], password[32], name[32], surname[32];
        sscanf(command, "REGISTER %s %s %s %s", username, password, name, surname);
        Client *existing_client = find_client_by_username(username);
        if (existing_client == NULL) {
            Client new_client;
            strcpy(new_client.username, username);
            strcpy(new_client.password, password);
            strcpy(new_client.name, name);
            strcpy(new_client.surname, surname);
            new_client.online = 0;
            clients[client_count++] = new_client;
            save_user(&new_client);
            sprintf(response, "REGISTERED %s\n", username);
        } else {
            sprintf(response, "ERROR: User %s already exists\n", username);
        }
    } else if (strncmp(command, "LOGIN", 5) == 0) {
        char username[32], password[32], mood[32] = "Happy";
        int matched = sscanf(command, "LOGIN %s %s %s", username, password, mood);
        if (matched < 2) {
            sprintf(response, "ERROR: Invalid command format\n");
        } else {
            Client *existing_client = find_client_by_username(username);
            if (existing_client != NULL && strcmp(existing_client->password, password) == 0) {
                existing_client->socket = client->socket;
                existing_client->online = 1;
                strcpy(existing_client->mood, mood);
                sprintf(response, "LOGGED IN %s\n", username);
            } else {
                sprintf(response, "ERROR: Invalid username or password\n");
            }
        }
    } else if (strncmp(command, "LOGOUT", 6) == 0) {
        client->online = 0;
        sprintf(response, "LOGGED OUT %s\n", client->username);
    } else if (strncmp(command, "MSG", 3) == 0) {
        char username[32], message[BUFFER_SIZE];
        sscanf(command, "MSG %s %[^\n]", username, message);
        if (strcmp(username, "*") == 0) {
            for (int i = 0; i < client_count; i++) {
                if (clients[i].online) {
                    char full_message[BUFFER_SIZE];
                    sprintf(full_message, "MSG from %s: %s\n", client->username, message);
                    send(clients[i].socket, full_message, strlen(full_message), 0);
                }
            }
        } else {
            Client *target_client = find_client_by_username(username);
            if (target_client != NULL && target_client->online) {
                char full_message[BUFFER_SIZE];
                sprintf(full_message, "MSG from %s: %s\n", client->username, message);
                send(target_client->socket, full_message, strlen(full_message), 0);
            } else {
                sprintf(response, "ERROR: User %s not online\n", username);
            }
        }
    } else if (strncmp(command, "LIST", 4) == 0) {
        for (int i = 0; i < client_count; i++) {
            sprintf(response + strlen(response), "%s %s\n", clients[i].username, clients[i].online ? "Online" : "Offline");
        }
    } else if (strncmp(command, "INFO", 4) == 0) {
        char username[32];
        sscanf(command, "INFO %s", username);
        Client *target_client = find_client_by_username(username);
        if (target_client != NULL) {
            sprintf(response, "INFO %s %s %s\n", target_client->name, target_client->surname, target_client->mood);
        } else {
            sprintf(response, "ERROR: User %s not found\n", username);
        }
    } else {
        sprintf(response, "ERROR: Unknown command\n");
    }
    send(client->socket, response, strlen(response), 0);
}

void *handle_client(void *arg) {
    Client *client = (Client *)arg;
    char buffer[BUFFER_SIZE];
    int nbytes;

    while ((nbytes = recv(client->socket, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[nbytes] = '\0';
        process_command(client, buffer);
    }

    close(client->socket);
    pthread_mutex_lock(&clients_mutex);
    client->online = 0;
    pthread_mutex_unlock(&clients_mutex);
    pthread_exit(NULL);
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 3) < 0) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    load_users();

    while (1) {
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len)) < 0) {
            perror("Accept failed");
            close(server_socket);
            exit(EXIT_FAILURE);
        }

        pthread_mutex_lock(&clients_mutex);
        Client *client = (Client *)malloc(sizeof(Client));
        client->socket = client_socket;
        client->address = client_addr;
        client->online = 1;
        clients[client_count++] = *client;
        pthread_mutex_unlock(&clients_mutex);

        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, (void *)client);
        pthread_detach(thread_id);
    }

    close(server_socket);
    return 0;
}
