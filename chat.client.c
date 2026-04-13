#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <errno.h>

#define BUFFER_SIZE 2048
#define PORT 8080

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Sử dụng: %s <IP_Server>\n", argv[0]);
        printf("Ví dụ: %s 127.0.0.1\n", argv[0]);
        return 1;
    }

    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Lỗi tạo socket");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
        perror("Địa chỉ IP không hợp lệ");
        return 1;
    }

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Lỗi kết nối đến Server");
        return 1;
    }

    printf("Đã kết nối tới Server %s:%d\n", argv[1], PORT);

    fd_set read_fds;
    char buffer[BUFFER_SIZE];

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);       
        FD_SET(client_socket, &read_fds);      

        int max_fd = (client_socket > STDIN_FILENO) ? client_socket : STDIN_FILENO;

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("Lỗi select()");
            break;
        }

        if (FD_ISSET(client_socket, &read_fds)) {
            memset(buffer, 0, BUFFER_SIZE);
            int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
            
            if (bytes_received <= 0) {
                printf("\nServer đã đóng kết nối.\n");
                break;
            }
            buffer[bytes_received] = '\0';
            printf("%s", buffer);
            fflush(stdout); 
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            memset(buffer, 0, BUFFER_SIZE);
            if (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
                send(client_socket, buffer, strlen(buffer), 0);
            }
        }
    }

    close(client_socket);
    return 0;
}
