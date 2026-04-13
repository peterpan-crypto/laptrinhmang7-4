#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define PORT 9000
#define MAX_CLIENTS 100
#define BUFFER_SIZE 2048

typedef struct {
    int fd;
    int is_authed;
} client_t;

client_t clients[MAX_CLIENTS];

void init_clients() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
        clients[i].is_authed = 0;
    }
}

void add_client(int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd == -1) {
            clients[i].fd = fd;
            clients[i].is_authed = 0;
            return;
        }
    }
    char *msg = "Server is full.\n";
    send(fd, msg, strlen(msg), 0);
    close(fd);
}

void remove_client(int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd == fd) {
            clients[i].fd = -1;
            clients[i].is_authed = 0;
            close(fd);
            return;
        }
    }
}

int authenticate(const char *username, const char *password) {
    FILE *f = fopen("users.txt", "r");
    if (!f) {
        perror("Loi: Khong the mo file users.txt");
        return 0; 
    }
    
    char u[256], p[256];

    while (fscanf(f, "%255s %255s", u, p) == 2) {
        if (strcmp(u, username) == 0 && strcmp(p, password) == 0) {
            fclose(f);
            return 1; 
        }
    }
    
    fclose(f);
    return 0; 
}

int main() {
    int listener;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        perror("Loi tao socket");
        exit(1);
    }
    
    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    if (bind(listener, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Loi bind");
        exit(1);
    }
    
    if (listen(listener, 10) < 0) {
        perror("Loi listen");
        exit(1);
    }
    
    printf("[-] Telnet Server dang chay o cong %d (Moi truong Ubuntu)\n", PORT);
    printf("[-] Cho doi ket noi tu client...\n");
    
    init_clients(); 
    
    fd_set readfds;
    int max_fd;
    
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(listener, &readfds); 
        max_fd = listener;
        
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd != -1) {
                FD_SET(clients[i].fd, &readfds);
                if (clients[i].fd > max_fd) {
                    max_fd = clients[i].fd;
                }
            }
        }
        
        int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("Loi select");
            break;
        }
        
        if (FD_ISSET(listener, &readfds)) {
            int new_fd = accept(listener, (struct sockaddr *)&client_addr, &addr_len);
            if (new_fd < 0) {
                perror("Loi accept");
            } else {
                printf("[+] Ket noi moi tu %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                add_client(new_fd);
                
                char *msg_welcome = "Vui long nhap tai khoan va mat khau (Cu phap: user pass):\n> ";
                send(new_fd, msg_welcome, strlen(msg_welcome), 0);
            }
        }
        
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int client_fd = clients[i].fd;
            
            if (client_fd != -1 && FD_ISSET(client_fd, &readfds)) {
                char buffer[BUFFER_SIZE];
                int n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
                
                if (n <= 0) {

                    printf("[-] Client %d da ngat ket noi.\n", client_fd);
                    remove_client(client_fd);
                } else {
                    buffer[n] = '\0';
                    
                    while (n > 0 && (buffer[n-1] == '\r' || buffer[n-1] == '\n')) {
                        buffer[n-1] = '\0';
                        n--;
                    }
                    
                    if (n == 0) {

                        char *prompt = "> ";
                        send(client_fd, prompt, strlen(prompt), 0);
                        continue;
                    }
                    
                    if (clients[i].is_authed == 0) {
                        char user[256] = {0};
                        char pass[256] = {0};
                        
                        if (sscanf(buffer, "%255s %255s", user, pass) == 2) {
                            if (authenticate(user, pass)) {
                                clients[i].is_authed = 1;
                                char *msg = ">>> Dang nhap thanh cong! Vui long nhap lenh can thuc thi:\n> ";
                                send(client_fd, msg, strlen(msg), 0);
                            } else {

                                char *msg = ">>> Sai thong tin dang nhap! Dang ngat ket noi client...\n";
                                send(client_fd, msg, strlen(msg), 0);
                                printf("[-] Client %d bam sai thong tin tai khoan. Dang ngat ket noi.\n", client_fd);
                                remove_client(client_fd);
                            }
                        } else {
                            char *msg = ">>> Sai cu phap! Vui long nhap theo dung chuan (user pass):\n> ";
                            send(client_fd, msg, strlen(msg), 0);
                        }
                    } else {

                        printf("[*] Client %d yeu cau lenh: %s\n", client_fd, buffer);

                        if (strcmp(buffer, "exit") == 0 || strcmp(buffer, "quit") == 0) {
                             char *msg = "Tam biet.\n";
                             send(client_fd, msg, strlen(msg), 0);
                             printf("[-] Client %d tu dong thoat thong qua lenh exit.\n", client_fd);
                             remove_client(client_fd);
                             continue;
                        }
                        
                        char cmd[BUFFER_SIZE + 32];
                        snprintf(cmd, sizeof(cmd), "%s > out.txt", buffer); 
                        system(cmd);
                        
                        FILE *f = fopen("out.txt", "rb");
                        if (f) {
                            char result_buf[2048];
                            size_t bytes_read;
                            int has_output = 0;

                            while ((bytes_read = fread(result_buf, 1, sizeof(result_buf), f)) > 0) {
                                send(client_fd, result_buf, bytes_read, 0);
                                has_output = 1;
                            }
                            fclose(f);
                            
                            char *prompt = "\n> ";
                            send(client_fd, prompt, strlen(prompt), 0);
                        } else {
                            char *msg = "Khong the thuc thi hoac tao duoc file out.txt de doc\n> ";
                            send(client_fd, msg, strlen(msg), 0);
                        }
                    }
                }
            }
        }
    }
    
    close(listener);
    return 0;
}
