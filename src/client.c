// client.c

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

// Функция генерации "UUID"
void generate_uuid(char *uuid) {
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    uint8_t buffer[4];
    if (read(fd, buffer, sizeof(buffer)) != sizeof(buffer)) {
        perror("read");
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd);

    //buffer[6] = (buffer[6] & 0x0F) | 0x40; // Версия 4
    //buffer[8] = (buffer[8] & 0x3F) | 0x80; // Вариант RFC 4122

    snprintf(uuid, 37, "%02x%02x%02x%02x", buffer[0], buffer[1], buffer[2], buffer[3]);
}

// Функция для подключения клиента к серверу
int connect_to_server(const char* host, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);  // Создание сокета
    if (!sockfd) {
        perror("socket");
        exit(1);  // Ошибка при создании сокета
    }

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    // Преобразуем имя хоста в IP-адрес
    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        exit(1);  // Ошибка при преобразовании адреса
    }

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect");
        exit(1);  // Ошибка при подключении к серверу
    }

//    struct sockaddr_in local = {0};
//    socklen_t local_len = sizeof(local);
//    if (getsockname(sockfd, (struct sockaddr*)&local, &local_len) == -1) {
//        perror("getsockname");
//        exit(1);
//    }
//    printf("IP DATA: %d\n", /*inet_ntoa(local.sin_addr),*/ ntohs(local.sin_port));

    return sockfd;
}

// Функция для начала игры
void start_game(int sockfd, const char* password) {
    char buffer[MAX_BUFFER] = "";
    snprintf(buffer, sizeof(buffer), "start %s", password);  // Формируем команду
    printf("buf: %s\n", buffer);
    send(sockfd, buffer, strlen(buffer), 0);  // Отправляем команду серверу
}

// Функция для присоединения к игре
void join_game(int sockfd, const char* password) {
    char buffer[MAX_BUFFER] = "";
    snprintf(buffer, sizeof(buffer), "join %s", password);  // Формируем команду
    printf("buf: %s\n", buffer);
    send(sockfd, buffer, strlen(buffer), 0);  // Отправляем команду серверу
}

// Функция для выполнения хода игрока
void make_move(int sockfd, int x, int y) {
    char buf[MAX_BUFFER] = "";
    int row, col;
    snprintf(buf, sizeof(buf), "move %d %d", x, y);  // Формируем команду
    send(sockfd, buf, strlen(buf), 0);  // Отправляем команду серверу
}

int main(int argc, char* argv[]) {

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <hostname> <port>\n", argv[0]);
        exit(1);  // Неверные параметры запуска
    }

    const char* host = argv[1];  // Хост сервера
    int port = atoi(argv[2]);  // Порт сервера

    // Подключаемся к серверу
    int sockfd;
    sockfd = connect_to_server(host, port);

    // Генерация "UUID"
    //char uuid[4]; // UUID длиной 4 символа
    //generate_uuid(uuid);
    //printf("Generated UUID: %s\n", uuid);

    // Основной цикл игры
    char command[MAX_BUFFER];
    char buffer[MAX_BUFFER];

    // Для нужд житейских
    short starting = 0;

    while(starting == 0) {

        starting++;

        bzero(&command, sizeof(command));
        printf("Enter command (start/join): ");
        fgets(command, sizeof(command), stdin);

        // Обрабатываем команды пользователя
        if (strncmp(command, "start", 5) == 0) {
            start_game(sockfd, command + 6);  // Пароль после команды "start "
        } else if (strncmp(command, "join", 4) == 0) {
            join_game(sockfd, command + 5);  // Пароль после команды "join "
        }
        else {
            printf("Unknown command!\n");
            exit(0);
        }

        // Для принятия ответа
        bzero(&buffer, MAX_BUFFER);
        int recv_bytes = recv(sockfd, buffer, MAX_BUFFER, 0);
        printf("Serv: %s\n", buffer);

        if(strncmp(buffer, "Error", 5) == 0) starting--;
    }

    bzero(&buffer, sizeof(buffer));
    int recv_bytes = recv(sockfd, buffer, MAX_BUFFER, 0);
    printf("Serv: %s\n", buffer);

    // Игровой цикл
    while (1) {
        int x, y;

        if(strncmp(buffer, "You", 3) != 0 && strncmp(buffer, "Error", 5) != 0) {
            printf("Enter your move as <row col>: ");
            scanf("%d %d", &x, &y);  // Ввод координат хода
            make_move(sockfd, x, y);  // Отправляем ход на сервер
        }

        bzero(&buffer, sizeof(buffer));

        int recv_bytes = recv(sockfd, buffer, MAX_BUFFER, 0);
        printf("Serv: %s\n", buffer);

        if(strncmp(buffer, "You", 3) == 0) break;
        else if(strncmp(buffer, "Error", 5) == 0) {
            printf("Enter your move as <row col>: ");
            scanf("%d %d", &x, &y);  // Ввод координат хода
            make_move(sockfd, x, y);  // Отправляем ход на сервер

            recv_bytes = recv(sockfd, buffer, MAX_BUFFER, 0);
            printf("Serv: %s\n", buffer);
        }

        recv_bytes = recv(sockfd, buffer, MAX_BUFFER, 0);
        printf("Serv: %s\n", buffer);

        if(strncmp(buffer, "You", 3) == 0) break;
    }

    close(sockfd);
    return 0;
}
