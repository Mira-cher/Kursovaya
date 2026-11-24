#define _CRT_SECURE_NO_WARNINGS
#include "domino.h"
#include <windows.h>


void draw(Game* g) {
    system("cls");
    set_color(11); gotoxy(10, 2); printf("СТОЛ");
    gotoxy(10, 4);
    DominoNode* curr = g->table;
    if (!curr) printf("[ Пусто ]");
    while (curr) {
        printf("[%d|%d] ", curr->data.left, curr->data.right);
        curr = curr->next;
    }

    set_color(14); gotoxy(10, 10); printf("ВАША КОЛОДА");
    gotoxy(10, 12);
    PlayerNode* p = g->player.hand;
    int i = 1;
    while (p) {
        printf("%d:[%d|%d] ", i++, p->data.left, p->data.right);
        p = p->next;
    }

    set_color(10); gotoxy(10, 16);
    int b_cnt = 0;
    PlayerNode* b = g->bot.hand;
    while (b) { b_cnt++; b = b->next; }
    printf("Костей у бота: %d", b_cnt);

    int bz_cnt = 0;
    BazarNode* bz = g->bazar;
    while (bz) { bz_cnt++; bz = bz->next; }
    set_color(8); gotoxy(10, 18);
    printf("Костей в базаре: %d", bz_cnt);

    set_color(8); gotoxy(10, 22);
    printf("Нажми номер кости | ESC - меню | F1 - справка");
}

int main() {
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);
    srand((unsigned int)time(NULL));
    Game g;
    init_game(&g);
    InputData input;

    while (g.state != EXIT) {
        if (g.state == MENU) {
            system("cls");
            set_color(14);
            printf("ДОМИНО\n\n");
            set_color(7);
            printf("1. Играть\n2. Рекорды\n3. Выход\n4. О программе\n5. Справка\n");
            input.key_code = _getch();

            if (input.key_code == '1') {
                free_all(&g);
                g.state = PLAYING;

                Domino full_set[28];
                generate_full_set(full_set);
                shuffle_set(full_set, 28);

                for (int i = 0; i < 7; i++) {
                    add_to_hand(&g.player, full_set[i]);
                    add_to_hand(&g.bot, full_set[i + 7]);
                }

                g.bazar = NULL;
                for (int i = 14; i < 28; i++) {
                    add_to_bazar(&g.bazar, full_set[i]);
                }
            }
            else if (input.key_code == '2') {
                show_records();
            }
            else if (input.key_code == '3') {
                free_all(&g);
                g.state = EXIT;
            }
            else if (input.key_code == '4') {
                system("cls");
                set_color(14);
                printf("О ПРОГРАММЕ\n\n");
                set_color(7);
                printf("Игра: Домино\n");
                printf("Авторы: Черных Мира, Винтоняк Андрей\n");
                printf("Группа: 5131001/50602\n");
                printf("Год: 2026\n");
                printf("ВУЗ: СПбПУ\n");
                printf("Институт: ИКНК\n");
                printf("Кафедра: Кибербезопасность\n\n");
                set_color(10);
                printf("\nНажми любую клавишу...");
                _getch();
            }
            else if (input.key_code == '5') {
                system("cls");
                set_color(14);
                printf("СПРАВКА\n\n");
                set_color(7);
                printf("Как играть в Домино:\n");
                printf("  - У вас 7 костей, у бота 7 костей\n");
                printf("  - 14 костей в базаре\n");
                printf("  - Нажмите 1-7 чтобы выбрать кость\n");
                printf("  - Если нет хода - берёте кость из базара\n");
                printf("  - Если базар пуст - ход пропускается\n");
                printf("  - Рыба - когда никто не может ходить И базар пуст\n");
                printf("  - ESC - выход в меню\n");
                set_color(10);
                printf("\nНажми любую клавишу...");
                _getch();
            }
        }
        else if (g.state == PLAYING) {
            draw(&g);

            //ПРОВЕРКА ПОБЕДЫ
            if (!g.player.hand) {
                system("cls");
                set_color(10);
                printf("ВЫ ПОБЕДИЛИ!");
                save_record("Игрок", 100);
                _getch();
                g.state = MENU;
                continue;
            }
            if (!g.bot.hand) {
                system("cls");
                set_color(12);
                printf("БОТ ВЫИГРАЛ!");
                save_record("Бот", 100);
                _getch();
                g.state = MENU;
                continue;
            }

            //ПРОВЕРКА ХОДОВ ИГРОКА
            int p_can = has_valid_moves(g.player.hand, g.table);

            if (!p_can) {
                Domino new_tile;
                if (take_from_bazar(&g.bazar, &new_tile)) {
                    set_color(12); gotoxy(10, 24);
                    printf("Нет ходов! Взята кость из базара: [%d|%d]", new_tile.left, new_tile.right);
                    add_to_hand(&g.player, new_tile);
                    Sleep(1000);
                    continue;
                }
                else {
                    set_color(12); gotoxy(10, 24);
                    printf("Базар пуст! Ход переходит боту...");
                    Sleep(1000);
                    bot_turn(&g);
                    continue;
                }
            }

            //ОЖИДАНИЕ ХОДА ИГРОКА
            input.key_code = _getch();

            if (input.key_code == 27) {
                g.state = MENU;
                continue;
            }
            if (input.key_code == 59) {
                system("cls");
                set_color(14);
                printf("СПРАВКА\n\n");
                set_color(7);
                printf("Нажмите 1-7 чтобы выбрать кость\n");
                printf("Если нет хода - берёте из базара\n");
                printf("ESC - меню\n");
                set_color(10);
                printf("\nНажми любую клавишу...");
                _getch();
                continue;
            }

            if (input.key_code >= '1' && input.key_code <= '9') {
                int target_idx = input.key_code - '0';
                PlayerNode* curr = g.player.hand, * prev = NULL;
                for (int i = 1; i < target_idx && curr; i++) {
                    prev = curr;
                    curr = curr->next;
                }

                if (curr) {
                    int side = can_place(g.table, curr->data);
                    if (side > 0) {
                        Domino d = curr->data;
                        if (side == 1) {
                            if (d.right != g.table->data.left) {
                                int t = d.left; d.left = d.right; d.right = t;
                            }
                            g.table = add_to_left(g.table, d);
                        }
                        else {
                            if (g.table) {
                                DominoNode* r = g.table;
                                while (r->next) r = r->next;
                                if (d.left != r->data.right) {
                                    int t = d.left; d.left = d.right; d.right = t;
                                }
                            }
                            g.table = add_to_right(g.table, d);
                        }
                        if (prev) prev->next = curr->next;
                        else g.player.hand = curr->next;
                        free(curr);
                        draw(&g);

                        // ХОД БОТА С ПРОВЕРКОЙ БАЗАРА
                        if (g.player.hand) {
                            Sleep(1000);
                            int b_can = has_valid_moves(g.bot.hand, g.table);

                            // Цикл: пока у бота нет хода и в базаре есть кости - берем
                            while (!b_can && !is_bazar_empty(g.bazar)) {
                                Domino bot_tile;
                                take_from_bazar(&g.bazar, &bot_tile);
                                add_to_hand(&g.bot, bot_tile);

                                set_color(12); gotoxy(10, 24);
                                printf("У бота нет ходов! Бот взял кость из базара: [%d|%d]", bot_tile.left, bot_tile.right);
                                Sleep(1000);

                                // Стираем сообщение
                                gotoxy(10, 24);
                                printf("                                                          ");

                                b_can = has_valid_moves(g.bot.hand, g.table);
                            }

                            if (b_can) {
                                bot_turn(&g);
                            }
                            else if (is_bazar_empty(g.bazar)) {
                                set_color(12); gotoxy(10, 24);
                                printf("Базар пуст! Бот пропускает ход...");
                                Sleep(1000);
                            }

                            // Стираем сообщение перед перерисовкой
                            gotoxy(10, 24);
                            printf("                                                          ");
                        }
                    }
                    else {
                        set_color(12); gotoxy(10, 24);
                        printf("Нельзя поставить эту кость!");
                        Sleep(1000);
                    }
                }
            }

            //ПРОВЕРКА РЫБЫ
            int p_can_final = has_valid_moves(g.player.hand, g.table);
            int b_can_final = has_valid_moves(g.bot.hand, g.table);
            int bazar_empty = is_bazar_empty(g.bazar);

            if (!p_can_final && !b_can_final && bazar_empty && g.table != NULL) {
                set_color(11); gotoxy(10, 24);
                printf("РЫБА! Ходов больше нет. Базар пуст.");
                _getch();
                g.state = MENU;
                continue;
            }
        }
    }

    free_all(&g);
    return 0;
}