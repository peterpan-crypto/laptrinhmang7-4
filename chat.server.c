#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <errno.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 2048
#define PORT 8080

typedef struct {
    int fd;
    int is_authenticated;
    char name[256];
} ClientState;

ClientState clients[MAX_CLIENTS];

void init_clients() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1; // 
        clients[i].is_authenticated = 0;
        memset(clients[i].name, 0, sizeof(clients[i].name));
    }
}

int main() {
    init_clients();

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Lỗi tạo socket()");
        return 1;
    }

    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Lỗi setsockopt() SO_REUSEADDR");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;   // Lắng nghe trên mọi interface
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Lỗi bind()");
        return 1;
    }

    if (listen(server_socket, 10) < 0) {
        perror("Lỗi listen()");
        return 1;
    }

    printf("Chat Server dang lang nghe tren cong %d...\n", PORT);

    fd_set master_set;
    FD_ZERO(&master_set);
    FD_SET(server_socket, &master_set);
    
    int max_fd = server_socket;

    while (1) {
        fd_set read_set = master_set; 

        if (select(max_fd + 1, &read_set, NULL, NULL, NULL) < 0) {
            perror("Lỗi select()");
            break;
        }

        if (FD_ISSET(server_socket, &read_set)) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int new_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);

            if (new_socket >= 0) {
                int index = -1;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].fd == -1) {
                        clients[i].fd = new_socket;
                        clients[i].is_authenticated = 0;
                        memset(clients[i].name, 0, sizeof(clients[i].name));
                        index = i;
                        break;
                    }
                }

                if (index != -1) {
                    FD_SET(new_socket, &master_set);
                    if (new_socket > max_fd) {
                        max_fd = new_socket; 
                    }
                    
                    printf("Client moi ket noi tu %s:%d (vi tri slot: %d). Dang cho xac thuc...\n", 
                           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), index);
                           
                    const char *prompt = "Vui long nhap ten theo dung cu phap: 'client_id: client_name'\n";
                    send(new_socket, prompt, strlen(prompt), 0);
                } else {
                    printf("Server da day (Over MAX_CLIENTS). Tu choi ket noi.\n");
                    const char *reject = "Server da day. Hay thu lai sau.\n";
                    send(new_socket, reject, strlen(reject), 0);
                    close(new_socket);
                }
            } else {
                perror("Lỗi accept()");
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int client_socket = clients[i].fd;
            
            if (client_socket != -1 && FD_ISSET(client_socket, &read_set)) {
                char buffer[BUFFER_SIZE];
                memset(buffer, 0, sizeof(buffer));
                
                int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

                if (bytes_received <= 0) {
                    printf("Client %d (Ten: %s) da ngat ket noi. Tieu huy tai nguyen slot nay.\n", i, clients[i].is_authenticated ? clients[i].name : "Non-Auth");
                    
                    close(client_socket);                    
                    FD_CLR(client_socket, &master_set);      
                    clients[i].fd = -1;                      
                    clients[i].is_authenticated = 0;
                    memset(clients[i].name, 0, sizeof(clients[i].name));
                    
                } else {
                    buffer[bytes_received] = '\0';

                    if (!clients[i].is_authenticated) {
                        if (strncmp(buffer, "client_id: ", 11) == 0) {
                            char parsed_name[256];
                            if (sscanf(buffer + 11, "%255s", parsed_name) == 1) {
                                strncpy(clients[i].name, parsed_name, sizeof(clients[i].name) - 1);
                                clients[i].is_authenticated = 1;

                                printf("Client %d xac thuc thanh cong voi ten: '%s'.\n", i, clients[i].name);
                                const char *success = "Xac thuc thanh cong! Ban da co the bat dau nhan tin.\n";
                                send(client_socket, success, strlen(success), 0);
                            } else {
                                const char *prompt = "Sai dinh dang. Luu y ten phai la ky tu viet lien. VD: 'client_id: Alice'\n";
                                send(client_socket, prompt, strlen(prompt), 0);
                            }
                        } else {
                            const char *prompt = "Sai cu phap. Vui long nhap dung: 'client_id: client_name'\n";
                            send(client_socket, prompt, strlen(prompt), 0);
                        }
                    } 
                    else {
                        buffer[strcspn(buffer, "\r\n")] = '\0'; 
                        
                        if (strlen(buffer) > 0) {
                            char out_buffer[BUFFER_SIZE + 512];
                            char time_str[64];
                            
                            time_t now = time(NULL);
                            struct tm *tm_info = localtime(&now);
                            
                            strftime(time_str, sizeof(time_str), "%Y/%m/%d %I:%M:%S %p", tm_info);

                            snprintf(out_buffer, sizeof(out_buffer), "%s %s: %s\n", time_str, clients[i].name, buffer);

                            for (int j = 0; j < MAX_CLIENTS; j++) {
                                if (clients[j].fd != -1 && clients[j].is_authenticated && j != i) {
                                    send(clients[j].fd, out_buffer, strlen(out_buffer), 0);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    printf("Tắt Server.\n");
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd != -1) {
            close(clients[i].fd);
        }
    }
    close(server_socket);
    
    return 0;
}
