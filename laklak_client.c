#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080 // Sunucudaki ile aynı port numarasını kullanın
#define BUFFER_SIZE 1024

void *receive_messages(void *socket) {
    int server_socket = *(int *)socket;
    char buffer[BUFFER_SIZE];
    int nbytes;

    while ((nbytes = recv(server_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[nbytes] = '\0';
        printf("%s\n", buffer);
    }

    pthread_exit(NULL);
}

int main() {
    int server_socket;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    pthread_t thread_id;

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(PORT);

    if (connect(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    pthread_create(&thread_id, NULL, receive_messages, (void *)&server_socket);
    pthread_detach(thread_id);

    while (1) {
        fgets(buffer, BUFFER_SIZE, stdin);
        send(server_socket, buffer, strlen(buffer), 0);
    }

    close(server_socket);
    return 0;
}
