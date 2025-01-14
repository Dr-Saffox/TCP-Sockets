// server.c

#include "common.h"
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

// Порт для подключения клиентов
//#define PORT 12345

// Обработчик сигнала SIGTERM для корректного завершения сервера
void sigterm_handler(int sig) {

    remove_semaphore(sem_id);
    printf("Server shutting down...\n");
    exit(0);
}

// Обработчик сигнала SIGHUP для перечитывания конфигурации
void sighup_handler(int sig) {
    printf("Received SIGHUP, reloading configuration...\n");
    // Перечитывание конфигурационного файла
    //read_config();
}

// Обработчик сигнала SIGCHLD для корректного завершения дочерних процессов-воркеров
void sigchld_handler(int sig) {
    while(waitpid(-1, NULL, WNOHANG) >0 )
        ;
}

// Функция демонизации сервера
void daemonize(int log) {
    pid_t pid, sid;

    // Создаём новый процесс
    pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);  // Ошибка при создании процесса
    }

    if (pid > 0) exit(0);  // Завершаем родительский процесс

    // Получаем идентификатор сессии для нового лидера сессии
    sid = setsid();
    if (sid < 0) {
        perror("setsid");
        exit(1);  // Ошибка при создании новой сессии
    }

    // Перенаправляем стандартные потоки в файл (например, для логирования)
    dup2(log, STDOUT_FILENO);  // Стандартный вывод
    //dup2(log, STDERR_FILENO);  // Стандартный вывод
    // Закрываем стандартные дескрипторы (stdin, stderr)
    close(log);
    close(STDIN_FILENO);
    close(STDERR_FILENO);


}

// ------------------------------------------------------------------------------------

// Функция для начала игры
void start_game(Game* game, const char* password, int client_fd) {
    lock_semaphore(sem_id);  // Блокируем доступ к разделяемой памяти

    strncpy(game->password, password, sizeof(game->password));  // Сохраняем пароль игры
    game->playerCount = 1;  // Устанавливаем, что в игре один игрок
    game->players_fd[0] = client_fd;
    game->currentPlayer = client_fd;  // Игрок 1 (X) начинает первым
    game->gameActive = 1;  // Игра активна
    game->move = 0; // Начальное кол-во ходов
    memset(game->board, EMPTY, sizeof(game->board));  // Очищаем игровое поле

    unlock_semaphore(sem_id);  // Разблокируем доступ
    printf("Player fd: %d started game with psw: %s\n", game->players_fd[0], game->password);
}

// Функция для присоединения второго игрока к игре
int join_game(Game* game, const char* password, int client_fd) {
    int res = 1;
    lock_semaphore(sem_id);  // Блокируем доступ к разделяемой памяти

    if (game->gameActive == 0 ||
        strncmp(game->password, password, sizeof(game->password)) != 0) {
        // Если игры нет или неверный пароль
        send(client_fd, "Error: Game not found or incorrect password.\n", 44, 0);
        res = 0;
    } else if (game->playerCount >= MAX_PLAYERS) {
        // Если в игре уже два игрока
        send(client_fd, "Error: Game is full.\n", 21, 0);
        res = 0;
    } else {
        game->playerCount++;  // Увеличиваем количество игроков
        game->players_fd[1] = client_fd;
        printf("Player fd: %d connected to game with psw: %s\n", game->players_fd[1], password);
        send(client_fd, "Joined the game!\nWait 1st player...\n", 36, 0);
    }

    unlock_semaphore(sem_id);  // Разблокируем доступ
    return res;
}

// Функция для обработки хода игрока
int process_turn(Game* game, const char* buffer, int client_fd) {
    int row, col;

    lock_semaphore(sem_id);  // Блокируем доступ к разделяемой памяти

    /*
    if(game->playerCount < 2) {
        unlock_semaphore(sem_id);  // Разблокируем доступ
        send(client_fd, "Waiting second player...\n", 25, 0);
        return;
    }
    */

    // Проверяем, что ход игрока сделан в свою очередь
    if ((game->currentPlayer == game->players_fd[0] && client_fd != game->players_fd[0]) ||
        (game->currentPlayer == game->players_fd[1] && client_fd != game->players_fd[1])) {
        unlock_semaphore(sem_id);  // Разблокируем доступ
        send(client_fd, "Error: It's not your turn!\n", 27, 0);
        return 0;
    }

    // Разбираем введённые данные
    if (sscanf(buffer+5, "%d %d", &row, &col) != 2) {
        unlock_semaphore(sem_id);  // Разблокируем доступ
        send(client_fd, "Error: Invalid input format!\n", 29, 0);
        return 0;
    }

    // Проверяем, что координаты внутри поля и клетка пуста
    if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE) {
        unlock_semaphore(sem_id);  // Разблокируем доступ
        send(client_fd,"Error: Invalid move! Valid range: 0-4.\n", 39, 0);
        return 0;
    }
    
    if (game->board[row][col] != EMPTY) {
        unlock_semaphore(sem_id);  // Разблокируем доступ
        send(client_fd, "Error: Invalid move! Cell is already occupied.\n", 47, 0);
        return 0;
    }

    // Обновляем игровое поле
    game->board[row][col] = (client_fd == game->players_fd[0]) ? X : O;

    // Проверяем, не победил ли игрок
    if (check_winner(game, row, col)) {
        printf("Player fd: %d win the game!\n", client_fd);
        game->gameActive = 0;  // Завершаем игру
        unlock_semaphore(sem_id);  // Разблокируем доступ
        return 1;
    }

    // Переходим к следующему ходу
    game->currentPlayer = (game->currentPlayer == game->players_fd[0]) ? game->players_fd[1] : game->players_fd[0];

    unlock_semaphore(sem_id);  // Разблокируем доступ
    return 1;
}

// Функция для отправки текущего состояния игрового поля клиенту
void send_board(Game* game, int cl_fd) {
    char board[MAX_BUFFER];
    bzero(&board, sizeof(board));

    strncat(board, "Current board state:\n", 21);

    for (int i = 0; i < BOARD_SIZE; ++i) {
        for (int j = 0; j < BOARD_SIZE; ++j) {
            if (game->board[i][j] == X) strncat(board, "X ", 2);
            else if (game->board[i][j] == O) strncat(board, "O ", 2);
            else strncat(board, ". ", 2);
        }
        strncat(board, "\n", 1);
    }
    strncat(board, "\n", 1);
    send(cl_fd, board, strlen(board), 0);
}

// Функция для проверки победителя
int check_winner(Game* game, int row, int col) {
    char player = game->board[row][col];

    // Направления: горизонталь, вертикаль, две диагонали
    int directions[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};

    for (int dir = 0; dir < 4; dir++) {
        int ncount = 1;

        // Смотрим в одну сторону
        for (int step = 1; step < 5; step++) {
            int r = row + step * directions[dir][0];
            int c = col + step * directions[dir][1];
            if (r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE && game->board[r][c] == player) {
                ncount++;
            } else {
                break;
            }
        }

        // Смотрим в другую сторону
        for (int step = 1; step < 5; step++) {
            int r = row - step * directions[dir][0];
            int c = col - step * directions[dir][1];
            if (r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE && game->board[r][c] == player) {
                ncount++;
            } else {
                break;
            }
        }

        // Если нашли 5 подряд, то игрок выиграл
        if (ncount >= 5) {
            return 1;
        }
    }

    return 0;
}

// Функция для завершения игры
void cleanup_game(Game* game) {
    lock_semaphore(sem_id);  // Блокируем доступ к разделяемой памяти
    game->gameActive = 0;  // Останавливаем игру
    unlock_semaphore(sem_id);  // Разблокируем доступ
}

// --------------------------------------------------------------------

// Функция для обработки клиента
void handle_client(int client_fd, Game* game) {
    char buffer[MAX_BUFFER] = "";
    //ssize_t bytes_received;

    // Оставляем соединение открытым (блокирующий recv)
    while( recv(client_fd, buffer, MAX_BUFFER, 0) > 0 ) {

        //buffer[bytes_received] = '\0';  // Завершаем строку
        printf("From player fd: %d Data in buffer: %s", client_fd, buffer);

        // Проверяем команду клиента
        if (strncmp(buffer, "start", 5) == 0) {

            if(game->playerCount < 1) {
                start_game(game, buffer + 6, client_fd); // Запускаем игру
                // Отправляем приветственное сообщение
                send(client_fd, "Welcome to the server!\nWaiting second player...\n", 48, 0);
                send_board(game, client_fd);
            }
            else {
                send(client_fd, "Error: server can startup only 1 game!\n", 39, 0);
            }

        } else if (strncmp(buffer, "join", 4) == 0) {

            // Присоединяем клиента к игре
            if(join_game(game, buffer + 5, client_fd) == 1) {
                // Уведомляем создавшего игру о втором игроке
                send(game->players_fd[0], "New player joined in your game!\n", 32, 0);
                // *Отправляем сообщение об успешном подключении внутри join_game
                send_board(game, client_fd);
                int localmove = game->move;
                        while(localmove + 1 != game->move)
                            ;
                send_board(game, client_fd);
            }

        }
        else if (strncmp(buffer, "move", 4) == 0) {

            // Проверяем очередность хода и его возможность
            if(game->playerCount < 2) {
                send(client_fd, "Error: You alone. Game stoped.\n", 31, 0);
            }
            else {

                int res = process_turn(game, buffer, client_fd);  // обработка хода

                if(res) {
                    game->move++;   // +1 ход в статистику

                    // Либо отправляем обоим доску, либо сообщение о финале
                    if(game->gameActive == 1) {
                        send_board(game, client_fd);

                        int localmove = game->move;
                        while(localmove + 1 != game->move) {
                            if (!game->gameActive) break;
                        }

                        send_board(game, client_fd);
                        if (!game->gameActive) send(client_fd, "You lost game!\n", 15, 0);
                    }
                    else send(client_fd, "You win game!\n", 14, 0);
                }
            }

        }
        else send(client_fd, "Unknown command!\n", 17, 0);

        bzero(&buffer, sizeof(buffer));
        printf("\nServer listening (players: %d moves: %d last player fd: %d)...\n\n", game->playerCount, game->move, client_fd);
    }

    close(client_fd);
    printf("Client with fd=%d closed connection.\n", client_fd);

    game->playerCount--;
    printf("\nServer listening (players: %d moves: %d last player fd: %d)...\n\n", game->playerCount, game->move, client_fd);

    return;
}

// Функция для чтения конфигурационного файла
int read_config(int sockfd) {
    char PORT[5] = "";
    char logname[255] = "";
    char symb;

    int conf = open("config.txt", O_RDONLY | 0644);
    if(conf == -1) {
        perror("open config");
    }

    int index = 0;
    do {
        read(conf, &symb, 1);
        if (symb != '\n') PORT[index++] = symb;
    }
    while(symb != '\n');

    index = 0;
    do {
        read(conf, &symb, 1);
        if (symb != '\n') logname[index++] = symb;
    }
    while(symb != '\n');

    close(conf);

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");   //INADDR_ANY
    server_addr.sin_port = htons(atoi(PORT));

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        exit(1);  // Ошибка при привязке сокета
    }

    if (listen(sockfd, 5) == -1) {
        perror("listen");
        exit(1);  // Ошибка при прослушивании порта
    }

    printf("Server is running on port %d...\n", atoi(PORT));

    char filename[256];
    char timestamp[64];
    time_t now = time(NULL);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S", localtime(&now));
    snprintf(filename, sizeof(filename), "%s_%s.log", logname, timestamp);
    int log_descriptor = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    return log_descriptor;
}

int main() {
    // Устанавливаем обработчики сигналов
    signal(SIGTERM, sigterm_handler);
    signal(SIGHUP, sighup_handler);
    signal(SIGCHLD, sigchld_handler);

    // Инициализируем разделяемую память и семафоры
    init_shared_memory();
    Game* game = attach_shared_memory();
    game->playerCount = 0;

    // Создаём структуру для приёма клиентов
    struct sockaddr_in client_addr;


    // Настройка серверного сокета
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(1);  // Ошибка при создании сокета
    }

    // Читаем исходный лог-файл с указанием порта
    int logfile = read_config(sockfd);

    // Демонизируем процесс
    daemonize(logfile);
    printf("Now server is daemon!..Count players: %d\n\n", game->playerCount);

    int client_fd[3];
    count = 0;
    // Основной цикл сервера
    while (1) {
        //count = game->playerCount;

        // Ожидание клиента
        bzero(&client_addr, sizeof(client_addr));
        int client_addr_len = sizeof(client_addr);

        client_fd[count] = accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_len);

        printf("New client fd: %d PORT: %d len: %d count: %d\n", client_fd[game->playerCount], ntohs(client_addr.sin_port), client_addr_len, game->playerCount);

        if (client_fd[count] == -1) {
            perror("accept");
            continue;  // Ошибка при принятии клиента, пробуем снова
        }
        else if (count >= MAX_PLAYERS) {
            send(client_fd[count], "Error: Server is full!\n", 23, 0);
            close(client_fd[count]);
            count--;
        }
        else {
            if (fork() == 0) {
                // Дочерний процесс обрабатывает клиента
                close(sockfd);
                handle_client(client_fd[count], game);
                exit(0);  // Закрытие дочернего процесса
            }

        }
        count++;
        close(client_fd);  // Закрытие сокета в родительском процессе
    }

    return 0;
}
