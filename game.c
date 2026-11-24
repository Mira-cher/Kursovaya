#define _CRT_SECURE_NO_WARNINGS
#include "domino.h"
#include <stdbool.h>
#include <stdarg.h>
#define MAX_HAND 28

typedef struct {
    int index;
    int side;
    int new_end;
    int sum;
    bool block;
} Candidate;

static int count_table(DominoNode* table) {
    int cnt = 0;
    while (table) {
        cnt++;
        table = table->next;
    }
    return cnt;
}

void gotoxy(int x, int y) {
    COORD c = { (SHORT)x, (SHORT)y };
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), c);
}

void set_color(int color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), (WORD)color);
}

Domino create_domino(int l, int r) {
    Domino d = { l, r };
    return d;
}

void generate_full_set(Domino* set) {
    int index = 0;
    for (int i = 0; i <= 6; i++) {
        for (int j = i; j <= 6; j++) {
            set[index++] = create_domino(i, j);
        }
    }
}

void shuffle_set(Domino* set, int size) {
    for (int i = size - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        Domino temp = set[i];
        set[i] = set[j];
        set[j] = temp;
    }
}

int has_valid_moves(PlayerNode* hand, DominoNode* table) {
    PlayerNode* curr = hand;
    while (curr) {
        if (can_place(table, curr->data) > 0) return 1;
        curr = curr->next;
    }
    return 0;
}

int can_place(DominoNode* table, Domino d) {
    if (table == NULL) return 2;
    if (d.right == table->data.left || d.left == table->data.left) return 1;
    DominoNode* curr = table;
    while (curr->next) curr = curr->next;
    if (d.left == curr->data.right || d.right == curr->data.right) return 2;
    return 0;
}

void free_all(Game* g) {
    while (g->table) {
        DominoNode* t = g->table;
        g->table = g->table->next;
        free(t);
    }
    while (g->player.hand) {
        PlayerNode* t = g->player.hand;
        g->player.hand = g->player.hand->next;
        free(t);
    }
    while (g->bot.hand) {
        PlayerNode* t = g->bot.hand;
        g->bot.hand = g->bot.hand->next;
        free(t);
    }
    while (g->bazar) {
        BazarNode* t = g->bazar;
        g->bazar = g->bazar->next;
        free(t);
    }
}

/*  Бэктрекинг-поиск (глубина 3) + логирование в logs.txt
  Все оценки — неотрицательные (метод "награды"). */

#define SEARCH_DEPTH 3
#define MAX_SIM_BRANCH 28

 // Веса "наград" 
#define W_SIZE      100   // бонус за каждую "отсутствующую" кость в руке 
#define W_MOBILITY   30   // бонус за каждый легальный ход            
#define W_ALIVE     500   // бонус за то, что не в тупике             
#define W_CONC       15   // бонус за единицу концентрации масти     

static FILE* g_log = NULL;

static void log_open(void) {
    g_log = fopen("logs.txt", "a");
    if (g_log) {
        time_t t = time(NULL);
        struct tm* lt = localtime(&t);
        fprintf(g_log, "\n========================================\n");
        fprintf(g_log, "[%02d.%02d.%04d %02d:%02d:%02d] Анализ хода бота\n",
            lt->tm_mday, lt->tm_mon + 1, lt->tm_year + 1900,
            lt->tm_hour, lt->tm_min, lt->tm_sec);
    }
}

static void log_close(void) {
    if (g_log) { fclose(g_log); g_log = NULL; }
}

static void log_write(const char* fmt, ...) {
    if (!g_log) return;
    va_list a; va_start(a, fmt);
    vfprintf(g_log, fmt, a);
    va_end(a);
    fflush(g_log);
}

typedef struct {
    int hand_idx;
    int side;
    int new_left;
    int new_right;
    int sum;
} SimMove;

typedef struct {
    int score;
    double realization;
    int steps;
    SimMove path[SEARCH_DEPTH];
    Domino tiles[SEARCH_DEPTH];
    int hand_sizes[SEARCH_DEPTH + 1];
    int end_left[SEARCH_DEPTH];
    int end_right[SEARCH_DEPTH];
} SimResult;

static int sim_enum_moves(const Domino* hand, int hs, int L, int R,
    SimMove* out, int max_out) {
    int cnt = 0;
    for (int i = 0; i < hs && cnt < max_out; i++) {
        int a = hand[i].left, b = hand[i].right;
        int added_left = 0, nl_val = 0;
        if (a == L || b == L) {
            nl_val = (a == L) ? b : a;
            out[cnt++] = (SimMove){ i, 1, nl_val, R, a + b };
            added_left = 1;
        }
        if (a == R || b == R) {
            int nr = (a == R) ? b : a;
            if (added_left && L == R && nl_val == L && nr == R) continue;
            out[cnt++] = (SimMove){ i, 2, L, nr, a + b };
        }
    }
    return cnt;
}

// Оценка позиции в терминах "наград" (все слагаемые >= 0).
static int sim_eval(const Domino* hand, int hs, int L, int R) {
    if (hs == 0) return 1000000;                 // победа 

    // 1) Награда за малое число костей в руке: (7 - hs) * W_SIZE 
    int size_reward = (7 - hs) * W_SIZE;         // [0, 600] при hs>=1

    // 2) Мобильность: сколько легальных ходов сейчас есть 
    SimMove tmp[MAX_SIM_BRANCH];
    int n = sim_enum_moves(hand, hs, L, R, tmp, MAX_SIM_BRANCH);
    int mob_reward = n * W_MOBILITY;            // [0, 420]

    // 3) Бонус за то, что не в тупике  
    int alive_bonus = (n > 0) ? W_ALIVE : 0;     // 0 или 500 

    // 4) Концентрация: длина самой "толстой" масти в руке 
    int sc[7] = { 0 };
    for (int i = 0; i < hs; i++) {
        sc[hand[i].left]++;
        if (hand[i].left != hand[i].right) sc[hand[i].right]++;
    }
    int mx = 0;
    for (int v = 0; v < 7; v++) if (sc[v] > mx) mx = sc[v];
    int conc_reward = mx * W_CONC;               // [0, 105] 

    return size_reward + mob_reward + alive_bonus + conc_reward;
}

static void sim_search(const Domino* hand, int hs, int L, int R,
    int depth_rem, int depth_cur,
    SimResult* best,
    SimMove* cur_path, Domino* cur_tiles, int* cur_sizes,
    int* cur_L, int* cur_R) {
    int sc = sim_eval(hand, hs, L, R);

    if (depth_rem == 0 || hs == 0) {
        if (sc > best->score || best->steps == 0) {
            best->score = sc;
            best->steps = depth_cur;
            best->hand_sizes[depth_cur] = hs;
            for (int i = 0; i < depth_cur; i++) {
                best->path[i] = cur_path[i];
                best->tiles[i] = cur_tiles[i];
                best->hand_sizes[i] = cur_sizes[i];
                best->end_left[i] = cur_L[i];
                best->end_right[i] = cur_R[i];
            }
        }
        return;
    }

    SimMove moves[MAX_SIM_BRANCH];
    int n = sim_enum_moves(hand, hs, L, R, moves, MAX_SIM_BRANCH);
    if (n == 0) {
        if (sc > best->score || best->steps == 0) {
            best->score = sc;
            best->steps = depth_cur;
            best->hand_sizes[depth_cur] = hs;
            for (int i = 0; i < depth_cur; i++) {
                best->path[i] = cur_path[i];
                best->tiles[i] = cur_tiles[i];
                best->hand_sizes[i] = cur_sizes[i];
                best->end_left[i] = cur_L[i];
                best->end_right[i] = cur_R[i];
            }
        }
        return;
    }

    for (int m = 0; m < n; m++) {
        Domino nh[28];
        int ns = 0;
        for (int i = 0; i < hs; i++) {
            if (i != moves[m].hand_idx) nh[ns++] = hand[i];
        }
        cur_path[depth_cur] = moves[m];
        cur_tiles[depth_cur] = hand[moves[m].hand_idx];
        cur_sizes[depth_cur] = hs;
        cur_L[depth_cur] = moves[m].new_left;
        cur_R[depth_cur] = moves[m].new_right;

        sim_search(nh, ns, moves[m].new_left, moves[m].new_right,
            depth_rem - 1, depth_cur + 1,
            best, cur_path, cur_tiles, cur_sizes, cur_L, cur_R);
    }
}

static double sim_realization(const SimResult* res,
    const int* known_cnt,
    int known_total,
    int player_hand_size) {
    if (res->steps <= 1) return 1.0;
    int unk_total = 28 - known_total;
    if (unk_total <= 0) return 0.0;
    double p = 1.0;
    for (int i = 0; i < res->steps - 1; i++) {
        int L = res->end_left[i];
        int R = res->end_right[i];
        int unk_L = 7 - known_cnt[L]; if (unk_L < 0) unk_L = 0;
        int unk_R = 7 - known_cnt[R]; if (unk_R < 0) unk_R = 0;
        double pL = (double)unk_L / unk_total;
        double pR = (double)unk_R / unk_total;
        double p_fail;
        if (L == R) p_fail = 1.0 - pL;
        else        p_fail = 1.0 - pL - pR;
        if (p_fail < 0) p_fail = 0;
        if (p_fail > 1) p_fail = 1;
        double pp = 1.0;
        for (int k = 0; k < player_hand_size; k++) pp *= p_fail;
        p *= pp;
    }
    return p;
}


void bot_turn(Game* g) {
    if (g->table == NULL) {
        PlayerNode* curr = g->bot.hand;
        PlayerNode* best = curr;
        int max_sum = curr->data.left + curr->data.right;
        curr = curr->next;
        while (curr) {
            int s = curr->data.left + curr->data.right;
            if (s > max_sum) {
                max_sum = s;
                best = curr;
            }
            curr = curr->next;
        }
        Domino d = best->data;
        g->table = add_to_right(NULL, d);
        PlayerNode* prev = NULL;
        PlayerNode* cur = g->bot.hand;
        while (cur != best) { prev = cur; cur = cur->next; }
        if (prev) prev->next = cur->next;
        else g->bot.hand = cur->next;
        free(cur);
        return;
    }
    int known_cnt[7] = { 0 };
    for (DominoNode* node = g->table; node != NULL; node = node->next) {
        known_cnt[node->data.left]++;
        if (node->data.left != node->data.right)
            known_cnt[node->data.right]++;
    }
    for (PlayerNode* p = g->bot.hand; p != NULL; p = p->next) {
        known_cnt[p->data.left]++;
        if (p->data.left != p->data.right)
            known_cnt[p->data.right]++;
    }
    int table_count = count_table(g->table);
    Domino hand_dom[MAX_HAND];
    PlayerNode* hand_nodes[MAX_HAND];
    int hand_size = 0;
    for (PlayerNode* p = g->bot.hand; p != NULL; p = p->next) {
        if (hand_size < MAX_HAND) {
            hand_dom[hand_size] = p->data;
            hand_nodes[hand_size] = p;
            hand_size++;
        }
    }
    int hand_cnt[7] = { 0 };
    for (int i = 0; i < hand_size; i++) {
        hand_cnt[hand_dom[i].left]++;
        if (hand_dom[i].left != hand_dom[i].right)
            hand_cnt[hand_dom[i].right]++;
    }
    Candidate candidates[28];
    int cand_cnt = 0;
    int left_end = g->table->data.left;
    DominoNode* last = g->table;
    while (last->next) last = last->next;
    int right_end = last->data.right;

    for (int i = 0; i < hand_size; i++) {
        int a = hand_dom[i].left;
        int b = hand_dom[i].right;
        bool can_left = (a == left_end || b == left_end);
        bool can_right = (a == right_end || b == right_end);

        if (can_left) {
            int new_end = (a == left_end) ? b : a;
            if (a == b) new_end = a;
            bool block = (known_cnt[a] == 7) || (known_cnt[b] == 7);
            candidates[cand_cnt++] = (Candidate){ i, 1, new_end, a + b, block };
        }
        if (can_right) {
            int new_end = (a == right_end) ? b : a;
            if (a == b) new_end = a;
            bool block = (known_cnt[a] == 7) || (known_cnt[b] == 7);
            candidates[cand_cnt++] = (Candidate){ i, 2, new_end, a + b, block };
        }
    }
    int chosen = -1;
    // последняя фишка
    {
        int best_idx = -1, best_sum = -1;
        for (int i = 0; i < cand_cnt; i++) {
            if (candidates[i].block && candidates[i].sum > best_sum) {
                best_sum = candidates[i].sum;
                best_idx = i;
            }
        }
        if (best_idx != -1) chosen = best_idx;
    }
    // сбросить самую тяжёлую
    if (chosen == -1 && table_count <= 8) {
        int best_idx = 0;
        for (int i = 1; i < cand_cnt; i++) {
            if (candidates[i].sum > candidates[best_idx].sum)
                best_idx = i;
        }
        chosen = best_idx;
    }
    // ход самой длинной мастью
    if (chosen == -1) {
        int best_suit = 0;
        for (int v = 1; v <= 6; v++) {
            if (hand_cnt[v] > hand_cnt[best_suit])
                best_suit = v;
        }
        int best_match = -1, fallback = -1;
        for (int i = 0; i < cand_cnt; i++) {
            int a = hand_dom[candidates[i].index].left;
            int b = hand_dom[candidates[i].index].right;
            if (a == best_suit || b == best_suit) {
                if (candidates[i].new_end == best_suit) {
                    best_match = i;
                    break;
                }
                if (fallback == -1) fallback = i;
            }
        }
        if (best_match != -1) chosen = best_match;
        else if (fallback != -1) chosen = fallback;
        else {
            int best_idx = 0;
            for (int i = 1; i < cand_cnt; i++) {
                if (candidates[i].sum > candidates[best_idx].sum)
                    best_idx = i;
            }
            chosen = best_idx;
        }
    }

    /* Бэктрекинг-анализ всех кандидатов.
          combined = score * (0.2 + 0.8 * real)
      монотонна по обоим аргументам: больше score → лучше,
      больше real → лучше. */
    if (cand_cnt > 0) {
        log_open();
        {
            Domino sim_hand[28];
            int sim_hand_size = 0;
            for (PlayerNode* p = g->bot.hand; p; p = p->next)
                if (sim_hand_size < 28) sim_hand[sim_hand_size++] = p->data;

            int known_local[7] = { 0 };
            int known_total = 0;
            int table_cnt_now = 0;
            for (DominoNode* n = g->table; n; n = n->next) {
                known_local[n->data.left]++;
                if (n->data.left != n->data.right) known_local[n->data.right]++;
                known_total++;
                table_cnt_now++;
            }
            for (int i = 0; i < sim_hand_size; i++) {
                known_local[sim_hand[i].left]++;
                if (sim_hand[i].left != sim_hand[i].right)
                    known_local[sim_hand[i].right]++;
                known_total++;
            }
            int bazar_cnt = 0;
            for (BazarNode* b = g->bazar; b; b = b->next) bazar_cnt++;
            int player_hand_size = 28 - table_cnt_now - bazar_cnt - sim_hand_size;
            if (player_hand_size < 0) player_hand_size = 0;

            log_write("Всего кандидатов: %d\n", cand_cnt);
            log_write("Известно костей : %d, в базаре: %d, "
                "в руке игрока: %d\n",
                known_total, bazar_cnt, player_hand_size);
            log_write("Костей у бота: %d, на столе: %d\n",
                sim_hand_size, table_cnt_now);
            log_write("Текущие концы: (%d, %d), глубина поиска: %d\n",
                left_end, right_end, SEARCH_DEPTH);
            log_write("Формула взвешивания: score * (0.2 + 0.8 * real)\n\n");

            SimResult results[28];
            double combined[28];

            for (int ci = 0; ci < cand_cnt; ci++) {
                int hidx = candidates[ci].index;
                Domino nh[28];
                int ns = 0;
                for (int i = 0; i < sim_hand_size; i++)
                    if (i != hidx) nh[ns++] = sim_hand[i];

                int nL = (candidates[ci].side == 1) ? candidates[ci].new_end : left_end;
                int nR = (candidates[ci].side == 2) ? candidates[ci].new_end : right_end;

                SimResult base;
                base.steps = 1;
                base.path[0] = (SimMove){ hidx, candidates[ci].side, nL, nR,
                                               candidates[ci].sum };
                base.tiles[0] = sim_hand[hidx];
                base.hand_sizes[0] = sim_hand_size;
                base.end_left[0] = nL;
                base.end_right[0] = nR;
                base.hand_sizes[1] = ns;
                base.score = sim_eval(nh, ns, nL, nR);

                SimResult deeper;
                deeper.score = -1;          // все реальные значения >= 0 
                deeper.steps = 0;
                SimMove cp[SEARCH_DEPTH];
                Domino  ct[SEARCH_DEPTH];
                int     cs[SEARCH_DEPTH];
                int     cL[SEARCH_DEPTH], cR[SEARCH_DEPTH];
                sim_search(nh, ns, nL, nR, SEARCH_DEPTH - 1, 0,
                    &deeper, cp, ct, cs, cL, cR);

                SimResult combined_res = base;
                if (deeper.steps > 0) {
                    for (int k = 0; k < deeper.steps && (1 + k) < SEARCH_DEPTH; k++) {
                        int idx = 1 + k;
                        combined_res.path[idx] = deeper.path[k];
                        combined_res.tiles[idx] = deeper.tiles[k];
                        combined_res.hand_sizes[idx] = deeper.hand_sizes[k];
                        combined_res.end_left[idx] = deeper.end_left[k];
                        combined_res.end_right[idx] = deeper.end_right[k];
                    }
                    combined_res.steps = 1 + deeper.steps;
                    combined_res.hand_sizes[combined_res.steps] = deeper.hand_sizes[deeper.steps];
                    combined_res.score = deeper.score;
                }

                double real = sim_realization(&combined_res, known_local,
                    known_total, player_hand_size);
                combined_res.realization = real;
                results[ci] = combined_res;
                /* score >= 0, real in [0,1] */
                combined[ci] = combined_res.score * (0.2 + 0.8 * real);

                log_write("--- Кандидат %d: [%d|%d] %s ---\n",
                    ci, sim_hand[hidx].left, sim_hand[hidx].right,
                    candidates[ci].side == 1 ? "(влево)" : "(вправо)");
                log_write("  Оценка позиции (score): %d\n",
                    combined_res.score);
                log_write("  Длина цепочки: %d шагов\n", combined_res.steps);
                for (int k = 0; k < combined_res.steps; k++) {
                    log_write("    шаг %d: [%d|%d] -> концы (%d,%d), в руке %d\n",
                        k + 1,
                        combined_res.tiles[k].left, combined_res.tiles[k].right,
                        combined_res.end_left[k], combined_res.end_right[k],
                        combined_res.hand_sizes[k + 1]);
                }
                log_write("  Шанс реализации: %.1f%%\n", real * 100.0);
                log_write("  Оценка (score*(0.2+0.8*real)): %.1f\n\n", combined[ci]);
            }

            int best_ci = 0;
            for (int ci = 1; ci < cand_cnt; ci++)
                if (combined[ci] > combined[best_ci]) best_ci = ci;

            double heur_combined = (chosen >= 0) ? combined[chosen] : -1.0;

            log_write(">>> Эвристический выбор: кандидат %d (взвеш. %.1f)\n",
                chosen, heur_combined);
            log_write(">>> Лучший по поиску:  кандидат %d (взвеш. %.1f)\n",
                best_ci, combined[best_ci]);

            if (chosen >= 0 && best_ci != chosen &&
                combined[best_ci] > heur_combined * 1.05 &&
                combined[best_ci] > heur_combined + 5.0) {
                log_write(">>> ПЕРЕОПРЕДЕЛЕНИЕ: выбран кандидат %d\n", best_ci);
                chosen = best_ci;
            }
            else {
                log_write(">>> Оставляем эвристический выбор.\n");
            }
            log_write("\n");
        }
        log_close();
    }

    int idx = candidates[chosen].index;
    int side = candidates[chosen].side;
    Domino d = hand_dom[idx];

    if (side == 1) {
        if (d.right != left_end) {
            int t = d.left; d.left = d.right; d.right = t;
        }
        g->table = add_to_left(g->table, d);
    }
    else {
        if (d.left != right_end) {
            int t = d.left; d.left = d.right; d.right = t;
        }
        g->table = add_to_right(g->table, d);
    }

    PlayerNode* target = hand_nodes[idx];
    PlayerNode* prev = NULL;
    PlayerNode* cur = g->bot.hand;
    while (cur != target) { prev = cur; cur = cur->next; }
    if (prev) prev->next = cur->next;
    else g->bot.hand = cur->next;
    free(cur);
}

DominoNode* add_to_left(DominoNode* head, Domino d) {
    DominoNode* n = (DominoNode*)malloc(sizeof(DominoNode));
    if (!n) return head;
    n->data = d; n->prev = NULL; n->next = head;
    if (head) head->prev = n;
    return n;
}

DominoNode* add_to_right(DominoNode* head, Domino d) {
    DominoNode* n = (DominoNode*)malloc(sizeof(DominoNode));
    if (!n) return head;
    n->data = d; n->next = NULL;
    if (!head) { n->prev = NULL; return n; }
    DominoNode* t = head;
    while (t->next) t = t->next;
    t->next = n; n->prev = t;
    return head;
}

void add_to_hand(Player* p, Domino d) {
    PlayerNode* n = (PlayerNode*)malloc(sizeof(PlayerNode));
    if (n) { n->data = d; n->next = p->hand; p->hand = n; }
}

void add_to_bazar(BazarNode** bazar, Domino d) {
    BazarNode* n = (BazarNode*)malloc(sizeof(BazarNode));
    if (!n) return;
    n->data = d; n->next = NULL;
    if (*bazar == NULL) *bazar = n;
    else {
        BazarNode* curr = *bazar;
        while (curr->next) curr = curr->next;
        curr->next = n;
    }
}

int take_from_bazar(BazarNode** bazar, Domino* out) {
    if (*bazar == NULL) return 0;
    BazarNode* temp = *bazar;
    *out = temp->data;
    *bazar = (*bazar)->next;
    free(temp);
    return 1;
}

int is_bazar_empty(BazarNode* bazar) {
    return bazar == NULL;
}

void init_game(Game* g) {
    g->state = MENU;
    g->table = NULL;
    g->player.hand = NULL;
    g->bot.hand = NULL;
    g->bazar = NULL;
    g->player.score = 0;
    g->bot.score = 0;
}