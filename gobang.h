#ifndef GOBANG_H
#define GOBANG_H

#define _CRT_SECURE_NO_WARNINGS

#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <string>
#include <cstdint>

// 常量定义
const int BOARD_SIZE = 15;
const int MAX_STEPS = 225;

// 棋子状态
const int EMPTY = 0;
const int PLAYER = 1;
const int AI = 2;

// 游戏模式
const int PLAYER_FIRST = 0;
const int AI_FIRST = 1;

// 禁手模式
const int NO_FORBIDDEN = 0;
const int WITH_FORBIDDEN = 1;

// 分数常量
const int WIN_SCORE = 1000000;
const int FOUR_SCORE = 50000;
const int BLOCKED_FOUR_SCORE = 8000;
const int DOUBLE_THREE_SCORE = 3000;
const int THREE_SCORE = 800;
const int TWO_SCORE = 100;

// 迭代加深参数
const int TIME_LIMIT_MS = 10000;
const int MIN_DEPTH = 2;
const int MAX_DEPTH = 8;

// 开局表使用步数限制
const int OPENING_BOOK_MAX_MOVES = 8;

// 禁手类型
const int NO_FORBIDDEN_MOVE = 0;
const int DOUBLE_THREE_FORBIDDEN = 1;
const int DOUBLE_FOUR_FORBIDDEN = 2;
const int OVERLINE_FORBIDDEN = 3;

// 置换表相关
const int TT_SIZE = 1000000; // 100万个条目
const int TT_EXACT = 0;
const int TT_ALPHA = 1;
const int TT_BETA = 2;

// 方向信息结构体
struct DirInfo {
    int continuous_chess;
    bool check_start;
    bool check_end;
};

// 步数记录结构体
struct Step {
    int player;
    int x;
    int y;
    int score;
};

// 位置评估结构体
struct PosEvaluation {
    int x;
    int y;
    int score;
};

// 置换表条目
struct TTEntry {
    uint64_t hash;
    int depth;
    int score;
    int flag;
    int best_x;
    int best_y;
};

// 时间管理器
class TimeManager {
private:
    std::chrono::steady_clock::time_point start_time;
    int time_limit_ms;

public:
    TimeManager(int limit_ms) : time_limit_ms(limit_ms) {
        start_time = std::chrono::steady_clock::now();
    }

    bool should_stop() const {
        auto current = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            current - start_time);
        return elapsed.count() > time_limit_ms;
    }

    double elapsed_ratio() const {
        auto current = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            current - start_time);
        return static_cast<double>(elapsed.count()) / time_limit_ms;
    }
};

// 置换表
class TranspositionTable {
private:
    std::vector<TTEntry> table;
    size_t size;

public:
    TranspositionTable(size_t table_size) : size(table_size) {
        table.resize(table_size);
        clear();
    }

    void clear() {
        for (auto& entry : table) {
            entry.hash = 0;
            entry.depth = -1;
        }
    }

    void store(uint64_t hash, int depth, int score, int flag, int best_x, int best_y) {
        size_t index = hash % size;
        // 替换策略：总是替换，或者根据深度决定
        if (depth >= table[index].depth) {
            table[index] = { hash, depth, score, flag, best_x, best_y };
        }
    }

    bool probe(uint64_t hash, int depth, int alpha, int beta,
        int& score, int& best_x, int& best_y) {
        size_t index = hash % size;
        TTEntry& entry = table[index];

        if (entry.hash == hash && entry.depth >= depth) {
            best_x = entry.best_x;
            best_y = entry.best_y;

            if (entry.flag == TT_EXACT) {
                score = entry.score;
                return true;
            }
            else if (entry.flag == TT_ALPHA && entry.score <= alpha) {
                score = alpha;
                return true;
            }
            else if (entry.flag == TT_BETA && entry.score >= beta) {
                score = beta;
                return true;
            }
        }
        return false;
    }
};

// Zobrist哈希
class ZobristHash {
private:
    uint64_t table[BOARD_SIZE][BOARD_SIZE][3]; // [x][y][player]
    uint64_t hash;

public:
    ZobristHash() : hash(0) {
        // 使用简单伪随机数生成器
        std::srand(12345);
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                for (int k = 0; k < 3; k++) {
                    table[i][j][k] =
                        (static_cast<uint64_t>(std::rand()) << 32) | std::rand();
                }
            }
        }
    }

    void update(int x, int y, int old_player, int new_player) {
        hash ^= table[x][y][old_player];
        hash ^= table[x][y][new_player];
    }

    uint64_t get_hash() const { return hash; }
    void set_hash(uint64_t h) { hash = h; }
};

// 全局变量声明
extern int board[BOARD_SIZE][BOARD_SIZE];
extern const int direction[8][2];
extern Step steps[MAX_STEPS];
extern int step_count;
extern int game_mode;
extern int forbidden_mode;
extern std::unordered_map<std::string, std::vector<std::pair<int, int>>> opening_book;
extern TranspositionTable transposition_table;
extern ZobristHash zobrist_hash;

// 函数声明
void empty_board();
void print_board();
bool have_space(int x, int y);
bool player_move(int x, int y);
bool undo_move(int steps_to_undo = 1);
DirInfo count_specific_direction(int x, int y, int dx, int dy, int player);
bool check_win(int x, int y, int player);
int check_forbidden(int x, int y, int player);
bool is_forbidden_move(int x, int y, int player);
int evaluate_pos(int x, int y, int player);
int evaluate_board(int player);
void ai_move(int depth);
void ai_move_with_iterative_deepening(int max_depth);
void review_process();
std::vector<PosEvaluation> get_candidate_moves(int player, int max_moves = 15);
int alpha_beta(int depth, int alpha, int beta, bool maximizing_player, int player, TimeManager& tm);
int alpha_beta_with_tt(int depth, int alpha, int beta, bool maximizing_player, int player, TimeManager& tm);
void init_opening_book();
std::string get_board_hash();
std::vector<std::pair<int, int>> get_opening_moves();
void show_forbidden_rules();
void init_zobrist();

inline void clear_input_buffer()
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}
}

#endif // GOBANG_H