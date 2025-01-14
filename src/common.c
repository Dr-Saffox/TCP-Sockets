// common.c

#include "common.h"

// Указатели на идентификаторы разделяемой памяти и семафора
int shm_id = -1;
int sem_id = -1;
int count = 0;


// Функция для инициализации разделяемой памяти
void init_shared_memory() {
    // Получаем идентификатор разделяемой памяти
    shm_id = shmget(SHM_KEY, sizeof(Game), IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("Its shmget");
        exit(1);  // Ошибка при создании разделяемой памяти
    }

    // Создаем семафор для синхронизации доступа
    sem_id = create_semaphore();
}

// Функция для присоединения к разделяемой памяти
Game* attach_shared_memory() {
    Game* game = (Game*)shmat(shm_id, NULL, 0);
    if (game == (void*)-1) {
        perror("shmat");
        exit(1);  // Ошибка при присоединении к разделяемой памяти
    }
    return game;
}

// Функция для отключения от разделяемой памяти
void detach_shared_memory(Game* game) {
    if (shmdt(game) == -1) {
        perror("shmdt");
        exit(1);  // Ошибка при отсоединении от разделяемой памяти
    }
}

// --------------------------------------------------------------------

// Функция для блокировки семафора (синхронизация доступа)
void lock_semaphore(int semid) {
    struct sembuf sb = {0, -1, 0};  // Операция блокировки семафора
    if (semop(semid, &sb, 1) == -1) {
        perror("semop lock");
        exit(1);  // Ошибка при блокировке семафора
    }
}

// Функция для разблокировки семафора (освобождение доступа)
void unlock_semaphore(int semid) {
    struct sembuf sb = {0, 1, 0};  // Операция разблокировки семафора
    if (semop(semid, &sb, 1) == -1) {
        perror("semop unlock");
        exit(1);  // Ошибка при разблокировке семафора
    }
}

// Функция для создания семафора
int create_semaphore() {
    int semid;

    // Создаём набор из одного семафора
    semid = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    if (semid == -1) {
        perror("semget");
        exit(EXIT_FAILURE);
    }

    // Инициализируем значение семафора в 1
    if (semctl(semid, 0, SETVAL, 1) == -1) {
        perror("semctl SETVAL");
        semctl(semid, 0, IPC_RMID); // Удаляем семафор при ошибке
        exit(EXIT_FAILURE);
    }

    return semid;
}

// Функция для удаления семафора
void remove_semaphore(int semid) {
    if (semctl(semid, 0, IPC_RMID, 0) == -1) {
        perror("semctl IPC_RMID");
        exit(1);  // Ошибка при удалении семафора
    }
}
