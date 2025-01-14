// common.h

#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>

// Максимальное количество игроков
#define MAX_PLAYERS 2

// Размер игрового поля
#define BOARD_SIZE 5  // Размер поля для Гомоку

// Ключи для разделяемой памяти и семафоров
#define SHM_KEY 1236
#define SEM_KEY 5678

// Максимальный размер буфера для сообщений
#define MAX_BUFFER 1024

// Для семафора
extern int sem_id;

// Для main сервера
extern int count;

// Перечисление для состояния клетки на поле (пустая, крестик, нолик)
typedef enum { EMPTY, X, O } Cell;

// Структура для хранения состояния игры
typedef struct {
    Cell board[BOARD_SIZE][BOARD_SIZE];  // Игровое поле
    int currentPlayer;  // Индекс текущего игрока (0 - Player 1, 1 - Player 2)
    int gameActive;  // Статус игры: 1 - игра идет, 0 - игра завершена
    char password[64];  // Пароль для игры
    int playerCount;  // Количество игроков (максимум 2)
    int players_fd[2];  // дескрипторы подключенных
    int move;               // номер текущего хода (начиная с "1")
} Game;

// Структура для хранения информации о клиенте
typedef struct {
    int sockfd;  // Сокет клиента
    struct sockaddr_in addr;  // Адрес клиента
} Client;

// Функции для работы с разделяемой памятью и семафорами
void init_shared_memory();  // Инициализация разделяемой памяти
Game* attach_shared_memory();  // Присоединение к разделяемой памяти
void detach_shared_memory(Game* game);  // Отключение от разделяемой памяти
void lock_semaphore(int semid);  // Блокировка семафора
void unlock_semaphore(int semid);  // Разблокировка семафора
int create_semaphore();  // Создание семафора
void remove_semaphore(int semid);  // Удаление семафора

#endif // COMMON_H
