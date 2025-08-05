#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Identifica sistema operacional antes da compilação
#ifdef _WIN_32
  #include <winsock2.h>
  #include <windows.h>
  // Diz ao linker para adicionar automaticamente a biblioteca ws2_32.lib(sockets) durante a compilação.
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
//----------

#define PORT 8080
#define MAX_CLIENTS 5
// Tamanho da mensagem
#define BUFFER_SIZE 1024 

int clients[MAX_CLIENTS];
int client_count = 0;

// Mecanismo de controle de concorrência para threads dos clientes(semáforo/mutex)
#ifdef _WIN32
    CRITICAL_SECTION lock;
#else
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
#endif

void lock_mutex() {
  #ifdef _WIN32
    EnterCriticalSection(&lock);
  #else
    pthread_mutex_lock(&lock);
  #endif
}

void unlock_mutex() {
  #ifdef _WIN32
    LeaveCriticalSection(&lock);
  #else
    pthread_mutex_unlock(&lock);
  #endif
}
//----------

// Função de rede para inicialização e limpeza do socket windowsAPI
int init_network() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printf("Falha ao inicializar Winsock. Erro: %d\n", WSAGetLastError());
        return -1;
    }
    printf("Winsock inicializado com sucesso.\n");
#endif
    return 0;
}

void cleanup_network() {
#ifdef _WIN32
    WSACleanup();
#endif
}
//----------

void broadcast_message(const char* message, int sender_socket){
  lock_mutex();
  for(int i = 0; i<client_count; i++){
    if(clients[i] != sender_socket){
      send(clients[i], message, strlen(message), 0);
    }
  }
  unlock_mutex();
}

#ifdef _WIN32
DWORD WINAPI handle_client(void *arg)
#else
void *handle_client(void *arg)
#endif
{
    int client_socket = *(int *)arg;
    char buffer[BUFFER_SIZE];

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            printf("Cliente desconectado.\n");
            break;
        }

        printf("Mensagem recebida: %s\n", buffer);
        broadcast_message(buffer, client_socket);
    }

    // Remove cliente problem[atico da lista de forma eficiente neste cenário(swap and pop O(n))
    lock_mutex();
    for (int i = 0; i < client_count; i++) {
        if (clients[i] == client_socket) {
            clients[i] = clients[client_count - 1];
            client_count--;
            break;
        }
    }
    unlock_mutex();

    close(client_socket);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

int main(void) {

  if(init_network() != 0) return 1;

  int server_socket;
  struct sockaddr_in server_addr, client_addr;
  socklen_t addr_size = sizeof(client_addr);

  // Criação do socket
  server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket < 0) {
      perror("Erro ao criar socket");
      return 1;
  }

  // Configura endereço do servidor
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(PORT);

  // Faz bind
  if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
      perror("Erro no bind");
      return 1;
  }

  // Escuta conexões
  if (listen(server_socket, MAX_CLIENTS) < 0) {
      perror("Erro no listen");
      return 1;
  }

  printf("Servidor iniciado na porta %d...\n", PORT);

  while (1) {
    int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size);
    if (client_socket < 0) {
        perror("Erro no accept");
        continue;
    }

    printf("Novo cliente conectado: %s\n", inet_ntoa(client_addr.sin_addr));

    lock_mutex();
    if (client_count < MAX_CLIENTS) {
        clients[client_count++] = client_socket;

        thread_t tid;
  #ifdef _WIN32
        tid = CreateThread(NULL, 0, handle_client, &client_socket, 0, NULL);
  #else
        pthread_create(&tid, NULL, handle_client, &client_socket);
        pthread_detach(tid);
  #endif
    } else {
        printf("Servidor cheio. Conexão recusada.\n");
        close(client_socket);
    }
    unlock_mutex();
  }

  close(server_socket);
  cleanup_network();
  
  return 0;
}