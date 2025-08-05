#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef HANDLE thread_t;
    #define close closesocket
#else
    #include <unistd.h>
    #include <pthread.h>
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    typedef pthread_t thread_t;
#endif

#define PORT 8080
#define BUFFER_SIZE 1024

#ifdef _WIN32
DWORD WINAPI receive_messages(void *arg)
#else
void *receive_messages(void *arg)
#endif
{
    int sock = *(int *)arg;
    char buffer[BUFFER_SIZE];

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            printf("Servidor desconectado.\n");
            break;
        }
        printf("\nMensagem: %s\n> ", buffer);
        fflush(stdout);
    }

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

int main() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Erro ao inicializar Winsock.\n");
        return 1;
    }
#endif

    int sock;
    struct sockaddr_in server_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Erro ao criar socket");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // IP do servidor (localhost)

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erro ao conectar ao servidor");
        return 1;
    }

    printf("Conectado ao servidor. Digite mensagens:\n");

    thread_t tid;
#ifdef _WIN32
    tid = CreateThread(NULL, 0, receive_messages, &sock, 0, NULL);
#else
    pthread_create(&tid, NULL, receive_messages, &sock);
    pthread_detach(tid);
#endif

    char message[BUFFER_SIZE];
    while (1) {
        printf("> ");
        fflush(stdout);
        if (fgets(message, BUFFER_SIZE, stdin) == NULL) break;

        // Remove newline
        size_t len = strlen(message);
        if (len > 0 && message[len - 1] == '\n') message[len - 1] = '\0';

        if (strcmp(message, "/quit") == 0) break;

        send(sock, message, strlen(message), 0);
    }

    close(sock);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
