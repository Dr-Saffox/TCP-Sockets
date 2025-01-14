#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

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

    printf("buf: %s\n", buffer);

    // Установить версию (4) и вариант (RFC 4122)
    //buffer[6] = (buffer[6] & 0x0F) | 0x40; // Версия 4
    //buffer[8] = (buffer[8] & 0x3F) | 0x80; // Вариант RFC 4122

    // Преобразовать байты в строку формата UUID
    //snprintf(uuid, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
    //         buffer[0], buffer[1], buffer[2], buffer[3],
    //         buffer[4], buffer[5],
    //         buffer[6], buffer[7],
    //         buffer[8], buffer[9],
    //         buffer[10], buffer[11], buffer[12], buffer[13], buffer[14], buffer[15]);
    snprintf(uuid, 37, "%02x%02x%02x%02x", buffer[0], buffer[1], buffer[2], buffer[3]);
}

int main() {
    char uuid[37]; // UUID длиной 36 символов + null-терминатор
    generate_uuid(uuid);
    printf("Generated UUID: %s\n", uuid);
    return 0;
}
