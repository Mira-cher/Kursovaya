#define _CRT_SECURE_NO_WARNINGS
#include "domino.h"

void save_record(const char* name, int score) {
    FILE* f = fopen("records.dat", "ab");
    if (f) {
        Record r;
        strncpy(r.name, name, 49);
        r.name[49] = '\0';
        r.score = score;
        fwrite(&r, sizeof(Record), 1, f);
        fclose(f);
    }
}

void show_records() {
    system("cls");
    set_color(10);
    printf("ТАБЛИЦА РЕКОРДОВ\n\n");
    set_color(7);
    FILE* f = fopen("records.dat", "rb");

    if (!f) {
        printf("Рекордов пока нет.\n");
    }
    else {
        Record r;
        printf("%-20s | %s\n", "ИМЯ ИГРОКА", "ОЧКИ");
        printf("------------------------------------\n");
        while (fread(&r, sizeof(Record), 1, f)) {
            printf("%-20s | %d\n", r.name, r.score);
        }
        fclose(f);
    }
    printf("\nНажми любую клавишу...");
    _getch();
}