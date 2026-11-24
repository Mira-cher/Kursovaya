#define _CRT_SECURE_NO_WARNINGS
#ifndef DOMINO_H
#define DOMINO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <conio.h>
#include <time.h>

typedef enum { MENU, PLAYING, RECORDS, EXIT } GameState;

// Объединение для оптимизации работы с входными данными
typedef union {
    int key_code;      // Код нажатой клавиши (например, '1'-'9')
    char action[20];   // Строковая команда (задел на будущее)
} InputData;

typedef struct { int left; int right; } Domino;

typedef struct DominoNode {
    Domino data;
    struct DominoNode* next, * prev;
} DominoNode;

typedef struct PlayerNode {
    Domino data;
    struct PlayerNode* next;
} PlayerNode;

typedef struct BazarNode {
    Domino data;
    struct BazarNode* next;
} BazarNode;

typedef struct { PlayerNode* hand; int score; } Player;

typedef struct {
    GameState state;
    DominoNode* table;
    Player player;
    Player bot;
    BazarNode* bazar;
} Game;

typedef struct { char name[50]; int score; } Record;

// Прототипы функций из game.c
void gotoxy(int x, int y);
void set_color(int color);
Domino create_domino(int l, int r);
void generate_full_set(Domino* set);
void shuffle_set(Domino* set, int size);
DominoNode* add_to_left(DominoNode* head, Domino d);
DominoNode* add_to_right(DominoNode* head, Domino d);
void add_to_hand(Player* p, Domino d);
void add_to_bazar(BazarNode** bazar, Domino d);
int take_from_bazar(BazarNode** bazar, Domino* out);
int is_bazar_empty(BazarNode* bazar);
void init_game(Game* g);
int can_place(DominoNode* table, Domino d);
void save_record(const char* name, int score);
void show_records();
void bot_turn(Game* g);
void free_all(Game* g);
int has_valid_moves(PlayerNode* hand, DominoNode* table);
void draw(Game* g);

#endif