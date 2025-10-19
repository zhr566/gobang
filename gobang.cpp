#define _CRT_SECURE_NO_WARNINGS
#include "gobang.h"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <random>

// 全局变量定义
int board[BOARD_SIZE][BOARD_SIZE] = { 0 };
const int direction[8][2] = { {1, 0}, {0, 1}, {1, 1}, {1, -1},{ -1,0},{0,-1},{-1,1},{-1,-1} };
Step steps[MAX_STEPS];
int step_count = 0;
int game_mode = PLAYER_FIRST;
int forbidden_mode = NO_FORBIDDEN;
std::unordered_map<std::string, std::vector<std::pair<int, int>>> opening_book;
TranspositionTable transposition_table(TT_SIZE);
ZobristHash zobrist_hash;

// 预计算模式评估表
const int PATTERN_SCORES[3][5] = {
    // 连续棋子数, 两端空格情况 -> 分数
    // 连续数: 2,3,4,5,6
    // 空格: 0(无空格),1(一端),2(两端)
    {10, 50, 500, 5000, 50000},  // 0端空格
    {20, 100, 2000, 20000, 200000}, // 1端空格  
    {50, 500, 5000, 50000, 500000}  // 2端空格
};

// 威胁评分等级常量（修改后：活三 > 双活三）
// 注意：WIN_SCORE 已经在头文件中定义，这里只定义其他常量
const int BLOCK_WIN_SCORE = 100000;     // 防守获胜威胁
const int BLOCK_FOUR_SCORE = 9000;      // 防守冲四
const int BLOCK_LIVE_THREE_SCORE = 8500; // 防守活三
const int BLOCK_JUMP_THREE_SCORE = 8300; // 防守跳活三
const int BLOCK_DOUBLE_THREE_SCORE = 8000; // 防守双活三
const int BLOCK_SLEEP_THREE_SCORE = 7000; // 防守眠三

// 位置价值表 (15×15棋盘)
const int POSITION_VALUE[BOARD_SIZE][BOARD_SIZE] = {
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1},
    {1, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 1},
    {1, 2, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 2, 1},
    {1, 2, 3, 4, 5, 5, 5, 5, 5, 5, 5, 4, 3, 2, 1},
    {1, 2, 3, 4, 5, 6, 6, 6, 6, 6, 5, 4, 3, 2, 1},
    {1, 2, 3, 4, 5, 6, 7, 7, 7, 6, 5, 4, 3, 2, 1},
    {1, 2, 3, 4, 5, 6, 7, 8, 7, 6, 5, 4, 3, 2, 1},
    {1, 2, 3, 4, 5, 6, 7, 7, 7, 6, 5, 4, 3, 2, 1},
    {1, 2, 3, 4, 5, 6, 6, 6, 6, 6, 5, 4, 3, 2, 1},
    {1, 2, 3, 4, 5, 5, 5, 5, 5, 5, 5, 4, 3, 2, 1},
    {1, 2, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 2, 1},
    {1, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 1},
    {1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}
};

// 初始化Zobrist哈希
void init_zobrist() {
    // 重新计算当前棋盘的哈希值
    zobrist_hash = ZobristHash();
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] != EMPTY) {
                zobrist_hash.update(i, j, EMPTY, board[i][j]);
            }
        }
    }
}

// 初始化开局表
void init_opening_book() {
    // 清空开局表
    opening_book.clear();

    // 标准开局 - 天元开局
    opening_book[""] = { {7, 7} };

    // 天元开局后的应对
    opening_book["7,7"] = { {6, 7}, {7, 6}, {8, 7}, {7, 8}, {6, 6}, {8, 8} };

    // 星位开局
    opening_book["3,3"] = { {3, 11}, {11, 3}, {11, 11}, {7, 7} };
    opening_book["3,11"] = { {3, 3}, {11, 3}, {11, 11}, {7, 7} };
    opening_book["11,3"] = { {3, 3}, {3, 11}, {11, 11}, {7, 7} };
    opening_book["11,11"] = { {3, 3}, {3, 11}, {11, 3}, {7, 7} };
}

// 获取棋盘状态的哈希字符串
std::string get_board_hash() {
    std::stringstream ss;
    for (int i = 0; i < step_count; i++) {
        ss << steps[i].x << "," << steps[i].y;
        if (i < step_count - 1) {
            ss << "-";
        }
    }
    return ss.str();
}

// 从开局表中获取推荐的走法
std::vector<std::pair<int, int>> get_opening_moves() {
    if (step_count >= OPENING_BOOK_MAX_MOVES) {
        return {};
    }

    std::string hash = get_board_hash();

    // 直接匹配
    if (opening_book.find(hash) != opening_book.end()) {
        return opening_book[hash];
    }

    return {};
}

// 悔棋功能
bool undo_move(int steps_to_undo) {
    if (step_count < steps_to_undo) {
        return false;
    }

    for (int i = 0; i < steps_to_undo; i++) {
        if (step_count > 0) {
            Step last_step = steps[--step_count];
            zobrist_hash.update(last_step.x, last_step.y, last_step.player, EMPTY);
            board[last_step.x][last_step.y] = EMPTY;
        }
    }
    return true;
}

// 检查禁手
int check_forbidden(int x, int y, int player) {
    if (player != PLAYER) {
        return NO_FORBIDDEN_MOVE;
    }

    if (forbidden_mode == 0) {  // 修复：使用 == 0 而不是 !
        return NO_FORBIDDEN_MOVE;
    }

    // 临时放置棋子
    board[x][y] = player;

    int three_count = 0;
    int four_count = 0;
    bool overline = false;

    // 检查每个方向
    for (int i = 0; i < 8; i++) {
        int dx = direction[i][0];
        int dy = direction[i][1];

        DirInfo info = count_specific_direction(x, y, dx, dy, player);

        // 检查长连禁手（超过5子）
        if (info.continuous_chess > 5) {
            overline = true;
        }

        // 检查活三和活四
        if (info.check_start && info.check_end) {
            if (info.continuous_chess == 3) {
                three_count++;
            }
            else if (info.continuous_chess == 4) {
                four_count++;
            }
        }
    }

    // 移除临时棋子
    board[x][y] = EMPTY;

    // 判断禁手类型
    if (overline) {
        return OVERLINE_FORBIDDEN;
    }
    if (four_count >= 2) {
        return DOUBLE_FOUR_FORBIDDEN;
    }
    if (three_count >= 2) {
        return DOUBLE_THREE_FORBIDDEN;
    }

    return NO_FORBIDDEN_MOVE;
}

// 判断是否为禁手
bool is_forbidden_move(int x, int y, int player) {
    return check_forbidden(x, y, player) != NO_FORBIDDEN_MOVE;
}

// 显示禁手规则
void show_forbidden_rules() {
    printf("\n禁手规则说明:\n");
    printf("- 黑棋（玩家）有以下禁手:\n");
    printf("  1. 三三禁手: 同时形成两个或以上的活三\n");
    printf("  2. 四四禁手: 同时形成两个或以上的活四\n");
    printf("  3. 长连禁手: 形成超过五子的连珠\n");
    printf("- 白棋（AI）没有禁手\n");
}

void empty_board()
{
    for (int i = 0; i < BOARD_SIZE; i++)
    {
        for (int j = 0; j < BOARD_SIZE; j++)
        {
            board[i][j] = EMPTY;
        }
    }
    step_count = 0;
    init_zobrist();
    transposition_table.clear();
}

void print_board()
{
    printf("\n");

    // 打印顶部坐标
    printf("   ");
    for (int j = 0; j < BOARD_SIZE; j++) {
        printf("%2d", j + 1);
    }
    printf("\n");

    for (int i = 0; i < BOARD_SIZE; i++) {
        // 打印行号
        printf("%2d ", i + 1);

        // 打印棋盘内容
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == PLAYER)
                printf(" X");
            else if (board[i][j] == AI)
                printf(" O");
            else
                printf(" .");
        }
        printf("\n");
    }

    // 打印底部坐标
    printf("   ");
    for (int j = 0; j < BOARD_SIZE; j++) {
        printf("%2d", j + 1);
    }
    printf("\n");
}

bool have_space(int x, int y)
{
    return (x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE && board[x][y] == EMPTY);
}

bool player_move(int x, int y)
{
    if (!have_space(x, y))
        return false;

    // 检查禁手
    if (forbidden_mode && is_forbidden_move(x, y, PLAYER)) {
        int forbidden_type = check_forbidden(x, y, PLAYER);
        printf("禁手！");
        switch (forbidden_type) {
        case DOUBLE_THREE_FORBIDDEN:
            printf("三三禁手\n");
            break;
        case DOUBLE_FOUR_FORBIDDEN:
            printf("四四禁手\n");
            break;
        case OVERLINE_FORBIDDEN:
            printf("长连禁手\n");
            break;
        }
        return false;
    }

    board[x][y] = PLAYER;
    zobrist_hash.update(x, y, EMPTY, PLAYER);
    steps[step_count++] = Step{ PLAYER, x, y, 0 };
    return true;
}

DirInfo count_specific_direction(int x, int y, int dx, int dy, int player)
{
    DirInfo info;
    info.continuous_chess = 1;
    info.check_start = false;
    info.check_end = false;

    // 检查正方向
    int nx = x + dx, ny = y + dy;
    while (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board[nx][ny] == player)
    {
        info.continuous_chess++;
        nx += dx;
        ny += dy;
    }
    if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE)
        if (board[nx][ny] == EMPTY)
            info.check_end = true;

    // 检查反方向
    nx = x - dx, ny = y - dy;
    while (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board[nx][ny] == player)
    {
        info.continuous_chess++;
        nx -= dx;
        ny -= dy;
    }
    if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE)
        if (board[nx][ny] == EMPTY)
            info.check_start = true;

    return info;
}

bool check_win(int x, int y, int player)
{
    for (int i = 0; i < 8; i++)
    {
        DirInfo info = count_specific_direction(x, y, direction[i][0], direction[i][1], player);
        if (info.continuous_chess >= 5)
            return true;
    }
    return false;
}

// 优化的评估函数 - 使用预计算模式
int evaluate_pos_fast(int x, int y, int player) {
    int original = board[x][y];
    board[x][y] = player;

    int total_score = 0;

    for (int i = 0; i < 8; i++) {
        int dx = direction[i][0], dy = direction[i][1];
        DirInfo info = count_specific_direction(x, y, dx, dy, player);

        // 使用预计算模式表
        int open_ends = (info.check_start ? 1 : 0) + (info.check_end ? 1 : 0);
        if (info.continuous_chess >= 2 && info.continuous_chess <= 6) {
            total_score += PATTERN_SCORES[open_ends][info.continuous_chess - 2];
        }

        // 连五直接返回最高分
        if (info.continuous_chess >= 5) {
            board[x][y] = original;
            return WIN_SCORE;
        }
    }

    board[x][y] = original;

    // 位置奖励
    int position_bonus = POSITION_VALUE[x][y] * 10;

    return total_score + position_bonus;
}

// 检查跳活三的特殊函数
bool check_jump_three(int x, int y, int dx, int dy, int player) {
    // 跳活三有两种形态：
    // 1. ○.○○ (中间有一个空位)
    // 2. ○○.○ (中间有一个空位)

    // 检查正方向
    int nx1 = x + dx, ny1 = y + dy;
    int nx2 = x + 2 * dx, ny2 = y + 2 * dy;
    int nx3 = x + 3 * dx, ny3 = y + 3 * dy;

    // 检查反方向
    int px1 = x - dx, py1 = y - dy;
    int px2 = x - 2 * dx, py2 = y - 2 * dy;
    int px3 = x - 3 * dx, py3 = y - 3 * dy;

    // 形态1: ○.○○ (当前位置是第一个棋子)
    if (have_space(nx1, ny1) &&
        board[nx2][ny2] == player &&
        board[nx3][ny3] == player) {
        // 检查两端是否为空
        bool start_empty = have_space(px1, py1);
        bool end_empty = (nx3 + dx >= 0 && nx3 + dx < BOARD_SIZE &&
            ny3 + dy >= 0 && ny3 + dy < BOARD_SIZE &&
            board[nx3 + dx][ny3 + dy] == EMPTY);
        if (start_empty && end_empty) return true;
    }

    // 形态2: ○○.○ (当前位置是第三个棋子)
    if (board[px1][py1] == player &&
        have_space(nx1, ny1) &&
        board[nx2][ny2] == player) {
        // 检查两端是否为空
        bool start_empty = (px1 - dx >= 0 && px1 - dx < BOARD_SIZE &&
            py1 - dy >= 0 && py1 - dy < BOARD_SIZE &&
            board[px1 - dx][py1 - dy] == EMPTY);
        bool end_empty = have_space(nx2 + dx, ny2 + dy);
        if (start_empty && end_empty) return true;
    }

    return false;
}

// 改进的堵活三检测 - 包括跳活三
bool block_live_three_improved(int player) {
    int opponent = (player == AI) ? PLAYER : AI;

    // 存储所有活三威胁位置和威胁程度
    std::vector<std::pair<int, int>> threat_positions;
    std::vector<int> threat_scores;

    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] != EMPTY) continue;

            // 检查这个位置是否能堵住已形成的活三
            int threat_level = 0;

            // 检查四个方向
            for (int dir = 0; dir < 4; dir++) {
                int dx = direction[dir][0];
                int dy = direction[dir][1];

                // 直接检查当前方向上对手已经形成的活三（不需要模拟放置）
                DirInfo info = count_specific_direction(i, j, dx, dy, opponent);

                // 活三条件：连续3子，且两端都有空格
                if (info.continuous_chess == 3 && info.check_start && info.check_end) {
                    threat_level = BLOCK_LIVE_THREE_SCORE;
                    break;
                }

                // 检查跳活三
                if (check_jump_three(i, j, dx, dy, opponent)) {
                    threat_level = BLOCK_JUMP_THREE_SCORE;
                    break;
                }

                // 检查冲四威胁
                if (info.continuous_chess == 4) {
                    threat_level = BLOCK_FOUR_SCORE;
                    break;
                }
            }

            // 如果检测到威胁，记录这个防守位置
            if (threat_level > 0) {
                threat_positions.push_back({ i, j });
                threat_scores.push_back(threat_level);
            }
        }
    }

    // 如果没有威胁，返回false
    if (threat_positions.empty()) {
        return false;
    }

    // 找到威胁最大的位置
    int best_index = 0;
    int max_threat = 0;
    for (int i = 0; i < threat_scores.size(); i++) {
        if (threat_scores[i] > max_threat) {
            max_threat = threat_scores[i];
            best_index = i;
        }
    }

    // 在威胁最大的位置防守
    int best_x = threat_positions[best_index].first;
    int best_y = threat_positions[best_index].second;

    board[best_x][best_y] = player;
    zobrist_hash.update(best_x, best_y, EMPTY, player);
    steps[step_count++] = Step{ player, best_x, best_y, max_threat };
    printf("%s落子(%d, %d) - 堵活三(威胁等级:%d)\n",
        (player == AI) ? "AI" : "玩家", best_x + 1, best_y + 1, max_threat);

    return true;
}

// 专门检测跳活三并防守
bool block_jump_three(int player) {
    int opponent = (player == AI) ? PLAYER : AI;

    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] != EMPTY) continue;

            // 检查四个方向的跳活三
            for (int dir = 0; dir < 4; dir++) {
                int dx = direction[dir][0];
                int dy = direction[dir][1];

                if (check_jump_three(i, j, dx, dy, opponent)) {
                    // 发现跳活三威胁，立即防守
                    board[i][j] = player;
                    zobrist_hash.update(i, j, EMPTY, player);
                    steps[step_count++] = Step{ player, i, j, BLOCK_JUMP_THREE_SCORE };
                    printf("%s落子(%d, %d) - 堵跳活三\n",
                        (player == AI) ? "AI" : "玩家", i + 1, j + 1);
                    return true;
                }
            }
        }
    }
    return false;
}

// 检查并堵住冲四
bool block_four(int player) {
    int opponent = (player == AI) ? PLAYER : AI;

    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] != EMPTY) continue;

            // 临时放置对手棋子
            board[i][j] = opponent;

            // 检查是否形成冲四（一端被封住的四子）
            for (int dir = 0; dir < 8; dir++) {
                int dx = direction[dir][0];
                int dy = direction[dir][1];

                DirInfo info = count_specific_direction(i, j, dx, dy, opponent);

                // 冲四条件：连续4子，且只有一端有空格
                if (info.continuous_chess == 4 &&
                    ((info.check_start && !info.check_end) ||
                        (!info.check_start && info.check_end))) {
                    // 恢复空位并防守
                    board[i][j] = EMPTY;
                    board[i][j] = player;
                    zobrist_hash.update(i, j, EMPTY, player);
                    steps[step_count++] = Step{ player, i, j, BLOCK_FOUR_SCORE };
                    printf("%s落子(%d, %d) - 堵冲四\n",
                        (player == AI) ? "AI" : "玩家", i + 1, j + 1);
                    return true;
                }
            }

            // 恢复空位
            board[i][j] = EMPTY;
        }
    }
    return false;
}

// 检查并堵住双活三（优先级降低）
bool block_double_three(int player) {
    int opponent = (player == AI) ? PLAYER : AI;

    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] != EMPTY) continue;

            // 临时放置对手棋子
            board[i][j] = opponent;

            // 检查是否形成双活三
            int live_three_count = 0;
            for (int dir = 0; dir < 8; dir++) {
                int dx = direction[dir][0];
                int dy = direction[dir][1];

                DirInfo info = count_specific_direction(i, j, dx, dy, opponent);

                // 活三条件
                if (info.continuous_chess == 3 && info.check_start && info.check_end) {
                    live_three_count++;
                }

                // 检查跳活三
                if (check_jump_three(i, j, dx, dy, opponent)) {
                    live_three_count++;
                }
            }

            // 恢复空位
            board[i][j] = EMPTY;

            // 如果发现双活三威胁，立即防守
            if (live_three_count >= 2) {
                board[i][j] = player;
                zobrist_hash.update(i, j, EMPTY, player);
                steps[step_count++] = Step{ player, i, j, BLOCK_DOUBLE_THREE_SCORE };
                printf("%s落子(%d, %d) - 堵双活三\n",
                    (player == AI) ? "AI" : "玩家", i + 1, j + 1);
                return true;
            }
        }
    }
    return false;
}

// 检查眠三并防守（一端被封住的三子）
bool block_sleep_three(int player) {
    int opponent = (player == AI) ? PLAYER : AI;

    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] != EMPTY) continue;

            // 检查这个位置是否能形成眠三
            board[i][j] = opponent;

            for (int dir = 0; dir < 8; dir++) {
                int dx = direction[dir][0];
                int dy = direction[dir][1];

                DirInfo info = count_specific_direction(i, j, dx, dy, opponent);

                // 眠三条件：连续3子，且只有一端有空格
                if (info.continuous_chess == 3 &&
                    ((info.check_start && !info.check_end) ||
                        (!info.check_start && info.check_end))) {

                    // 恢复空位并防守
                    board[i][j] = EMPTY;
                    board[i][j] = player;
                    zobrist_hash.update(i, j, EMPTY, player);
                    steps[step_count++] = Step{ player, i, j, BLOCK_SLEEP_THREE_SCORE };
                    printf("%s落子(%d, %d) - 堵眠三\n",
                        (player == AI) ? "AI" : "玩家", i + 1, j + 1);
                    return true;
                }
            }

            board[i][j] = EMPTY;
        }
    }
    return false;
}

// 检查获胜威胁并防守
bool block_win_threat(int player) {
    int opponent = (player == AI) ? PLAYER : AI;

    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == EMPTY) {
                board[i][j] = opponent;
                if (check_win(i, j, opponent)) {
                    board[i][j] = EMPTY;
                    board[i][j] = player;
                    zobrist_hash.update(i, j, EMPTY, player);
                    steps[step_count++] = Step{ player, i, j, BLOCK_WIN_SCORE };
                    printf("%s落子(%d, %d) - 紧急防守获胜威胁\n",
                        (player == AI) ? "AI" : "玩家", i + 1, j + 1);
                    return true;
                }
                board[i][j] = EMPTY;
            }
        }
    }
    return false;
}

// 新增：检测潜在威胁
int evaluate_potential_threat(int x, int y, int player) {
    int opponent = (player == AI) ? PLAYER : AI;
    int threat_score = 0;

    // 临时放置对手棋子
    board[x][y] = opponent;

    // 检查获胜威胁
    if (check_win(x, y, opponent)) {
        board[x][y] = EMPTY;
        return 10000; // 最高威胁
    }

    // 检查冲四威胁
    int four_count = 0;
    for (int dir = 0; dir < 8; dir++) {
        int dx = direction[dir][0];
        int dy = direction[dir][1];
        DirInfo info = count_specific_direction(x, y, dx, dy, opponent);

        if (info.continuous_chess == 4 && (info.check_start || info.check_end)) {
            four_count++;
        }
    }
    if (four_count > 0) {
        threat_score += 8000 + four_count * 500;
    }

    // 检查活三威胁
    int live_three_count = 0;
    for (int dir = 0; dir < 4; dir++) {
        int dx = direction[dir][0];
        int dy = direction[dir][1];
        DirInfo info = count_specific_direction(x, y, dx, dy, opponent);

        if (info.continuous_chess == 3 && info.check_start && info.check_end) {
            live_three_count++;
        }

        if (check_jump_three(x, y, dx, dy, opponent)) {
            live_three_count++;
        }
    }

    board[x][y] = EMPTY;

    // 双活三威胁
    if (live_three_count >= 2) {
        threat_score += 7500;
    }
    else if (live_three_count == 1) {
        threat_score += 6000;
    }

    return threat_score;
}

// 新增：检查如果对手下在某位置会形成的威胁
bool check_potential_threat(int x, int y, int player) {
    int opponent = (player == AI) ? PLAYER : AI;

    // 临时放置对手棋子
    board[x][y] = opponent;

    // 检查是否会形成立即获胜
    if (check_win(x, y, opponent)) {
        board[x][y] = EMPTY;
        return true;
    }

    // 检查是否会形成冲四
    for (int dir = 0; dir < 8; dir++) {
        int dx = direction[dir][0];
        int dy = direction[dir][1];
        DirInfo info = count_specific_direction(x, y, dx, dy, opponent);

        if (info.continuous_chess == 4 && (info.check_start || info.check_end)) {
            board[x][y] = EMPTY;
            return true;
        }
    }

    // 检查是否会形成双活三
    int live_three_count = 0;
    for (int dir = 0; dir < 4; dir++) {
        int dx = direction[dir][0];
        int dy = direction[dir][1];
        DirInfo info = count_specific_direction(x, y, dx, dy, opponent);

        if (info.continuous_chess == 3 && info.check_start && info.check_end) {
            live_three_count++;
        }

        if (check_jump_three(x, y, dx, dy, opponent)) {
            live_three_count++;
        }
    }

    board[x][y] = EMPTY;

    // 如果会形成双活三，也是严重威胁
    return (live_three_count >= 2);
}

// 改进的综合防守函数 - 包含预测性威胁检测
bool comprehensive_defense(int player) {
    int opponent = (player == AI) ? PLAYER : AI;

    // 1. 检查立即获胜威胁 (最高优先级)
    if (block_win_threat(player)) {
        return true;
    }

    // 2. 检查对手下一步可能形成的致命威胁
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] != EMPTY) continue;

            // 检查如果对手下在这里是否会形成致命威胁
            if (check_potential_threat(i, j, player)) {
                // 立即防守这个位置
                board[i][j] = player;
                zobrist_hash.update(i, j, EMPTY, player);
                steps[step_count++] = Step{ player, i, j, 9500 };
                printf("%s落子(%d, %d) - 预防性防守\n",
                    (player == AI) ? "AI" : "玩家", i + 1, j + 1);
                return true;
            }
        }
    }

    // 3. 原有的防守逻辑 (按优先级顺序)
    if (block_four(player)) return true;
    if (block_live_three_improved(player)) return true;
    if (block_jump_three(player)) return true;
    if (block_double_three(player)) return true;
    if (block_sleep_three(player)) return true;

    return false;
}

// 改进的候选走法生成 - 考虑潜在威胁
std::vector<PosEvaluation> get_candidate_moves(int player, int max_moves) {
    std::vector<PosEvaluation> candidates;

    // 在开局阶段，优先考虑开局表的推荐
    if (step_count < OPENING_BOOK_MAX_MOVES) {
        auto opening_moves = get_opening_moves();
        for (const auto& move : opening_moves) {
            if (have_space(move.first, move.second)) {
                if (player == PLAYER && forbidden_mode && is_forbidden_move(move.first, move.second, PLAYER)) {
                    continue;
                }
                int score = evaluate_pos_fast(move.first, move.second, player) + 5000;
                candidates.push_back({ move.first, move.second, score });
            }
        }
    }

    // 1. 首先考虑防守潜在威胁
    std::vector<std::pair<int, int>> threat_positions;
    std::vector<int> threat_scores;

    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] != EMPTY) continue;

            // 评估这个位置对对手的潜在威胁
            int threat_level = evaluate_potential_threat(i, j, player);
            if (threat_level > 0) {
                threat_positions.push_back({ i, j });
                threat_scores.push_back(threat_level);
            }
        }
    }

    // 将高威胁位置加入候选
    for (int i = 0; i < threat_positions.size() && i < max_moves / 2; i++) {
        int x = threat_positions[i].first;
        int y = threat_positions[i].second;
        candidates.push_back({ x, y, threat_scores[i] });
    }

    // 2. 原有的候选走法生成逻辑
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] != EMPTY) continue;

            if (player == PLAYER && forbidden_mode && is_forbidden_move(i, j, PLAYER)) {
                continue;
            }

            bool has_neighbor = false;
            // 优化：减少邻居检查范围
            for (int di = -1; di <= 1; di++) {
                for (int dj = -1; dj <= 1; dj++) {
                    if (di == 0 && dj == 0) continue;
                    int ni = i + di, nj = j + dj;
                    if (ni >= 0 && ni < BOARD_SIZE && nj >= 0 && nj < BOARD_SIZE) {
                        if (board[ni][nj] != EMPTY) {
                            has_neighbor = true;
                            break;
                        }
                    }
                }
                if (has_neighbor) break;
            }

            if (has_neighbor || step_count < 4) {
                int score = evaluate_pos_fast(i, j, player);
                candidates.push_back({ i, j, score });
            }
        }
    }

    // 使用nth_element进行部分排序，比完全排序更快
    if (candidates.size() > max_moves) {
        std::nth_element(candidates.begin(), candidates.begin() + max_moves, candidates.end(),
            [](const PosEvaluation& a, const PosEvaluation& b) {
                return a.score > b.score;
            });
        candidates.resize(max_moves);
    }
    else {
        std::sort(candidates.begin(), candidates.end(),
            [](const PosEvaluation& a, const PosEvaluation& b) {
                return a.score > b.score;
            });
    }

    return candidates;
}

int evaluate_pos(int x, int y, int player) {
    return evaluate_pos_fast(x, y, player);
}

int evaluate_board(int player) {
    int total_score = 0;

    // 优化：只评估有棋子的区域
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == player) {
                total_score += evaluate_pos_fast(i, j, player) / 10;
            }
            else if (board[i][j] != EMPTY) {
                total_score -= evaluate_pos_fast(i, j, (player == AI) ? PLAYER : AI) / 10;
            }
        }
    }

    return total_score;
}

// 带置换表的Alpha-Beta搜索
int alpha_beta_with_tt(int depth, int alpha, int beta, bool maximizing_player, int player, TimeManager& tm) {
    if (tm.should_stop()) {
        return maximizing_player ? -WIN_SCORE : WIN_SCORE;
    }

    // 检查胜负
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] != EMPTY) {
                if (check_win(i, j, AI)) return WIN_SCORE + depth;
                if (check_win(i, j, PLAYER)) return -WIN_SCORE - depth;
            }
        }
    }

    if (depth == 0) {
        return evaluate_board(AI);
    }

    // 查询置换表
    uint64_t current_hash = zobrist_hash.get_hash();
    int tt_score, tt_best_x, tt_best_y;
    if (transposition_table.probe(current_hash, depth, alpha, beta, tt_score, tt_best_x, tt_best_y)) {
        return tt_score;
    }

    auto candidates = get_candidate_moves(maximizing_player ? AI : PLAYER, 10);
    int original_alpha = alpha;
    int best_score = maximizing_player ? -WIN_SCORE * 2 : WIN_SCORE * 2;
    int best_x = -1, best_y = -1;

    if (maximizing_player) {
        for (const auto& candidate : candidates) {
            if (tm.should_stop()) break;

            // 尝试走法
            board[candidate.x][candidate.y] = AI;
            zobrist_hash.update(candidate.x, candidate.y, EMPTY, AI);

            int eval = alpha_beta_with_tt(depth - 1, alpha, beta, false, player, tm);

            // 撤销走法
            board[candidate.x][candidate.y] = EMPTY;
            zobrist_hash.update(candidate.x, candidate.y, AI, EMPTY);

            if (eval > best_score) {
                best_score = eval;
                best_x = candidate.x;
                best_y = candidate.y;
            }
            alpha = std::max(alpha, eval);
            if (beta <= alpha) break;
        }
    }
    else {
        for (const auto& candidate : candidates) {
            if (tm.should_stop()) break;

            board[candidate.x][candidate.y] = PLAYER;
            zobrist_hash.update(candidate.x, candidate.y, EMPTY, PLAYER);

            int eval = alpha_beta_with_tt(depth - 1, alpha, beta, true, player, tm);

            board[candidate.x][candidate.y] = EMPTY;
            zobrist_hash.update(candidate.x, candidate.y, PLAYER, EMPTY);

            if (eval < best_score) {
                best_score = eval;
                best_x = candidate.x;
                best_y = candidate.y;
            }
            beta = std::min(beta, eval);
            if (beta <= alpha) break;
        }
    }

    // 存储到置换表
    int tt_flag;
    if (best_score <= original_alpha) {
        tt_flag = TT_BETA;
    }
    else if (best_score >= beta) {
        tt_flag = TT_ALPHA;
    }
    else {
        tt_flag = TT_EXACT;
    }

    transposition_table.store(current_hash, depth, best_score, tt_flag, best_x, best_y);

    return best_score;
}

// 原来的alpha_beta函数（保持兼容性）
int alpha_beta(int depth, int alpha, int beta, bool maximizing_player, int player, TimeManager& tm) {
    return alpha_beta_with_tt(depth, alpha, beta, maximizing_player, player, tm);
}

// 带迭代加深的AI移动函数
void ai_move_with_iterative_deepening(int max_depth) {
    TimeManager tm(TIME_LIMIT_MS);
    int best_x = -1, best_y = -1;
    int best_score = -WIN_SCORE * 2;

    // 第一步下在天元（如果是AI先手）
    if (step_count == 0 && game_mode == AI_FIRST) {
        board[7][7] = AI;
        zobrist_hash.update(7, 7, EMPTY, AI);
        steps[step_count++] = Step{ AI, 7, 7, 0 };
        printf("AI落子(8, 8) - 天元开局\n");
        return;
    }

    // 检查开局表
    if (step_count < OPENING_BOOK_MAX_MOVES) {
        auto opening_moves = get_opening_moves();
        if (!opening_moves.empty()) {
            std::vector<std::pair<int, int>> valid_moves;
            for (const auto& move : opening_moves) {
                if (have_space(move.first, move.second)) {
                    valid_moves.push_back(move);
                }
            }

            if (!valid_moves.empty()) {
                int random_index = rand() % valid_moves.size();
                best_x = valid_moves[random_index].first;
                best_y = valid_moves[random_index].second;
                board[best_x][best_y] = AI;
                zobrist_hash.update(best_x, best_y, EMPTY, AI);
                steps[step_count++] = Step{ AI, best_x, best_y, WIN_SCORE / 2 };
                printf("AI落子(%d, %d) - 开局定式\n", best_x + 1, best_y + 1);
                return;
            }
        }
    }

    // 检查是否可以直接获胜
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == EMPTY) {
                board[i][j] = AI;
                if (check_win(i, j, AI)) {
                    steps[step_count++] = Step{ AI, i, j, WIN_SCORE };
                    printf("AI落子(%d, %d) - 获胜!\n", i + 1, j + 1);
                    return;
                }
                board[i][j] = EMPTY;
            }
        }
    }

    // 使用改进的综合防守策略（包含预测性威胁检测）
    if (comprehensive_defense(AI)) {
        return;
    }

    // 迭代加深搜索
    for (int depth = MIN_DEPTH; depth <= max_depth; depth++) {
        if (tm.should_stop()) break;

        auto candidates = get_candidate_moves(AI, 12);
        int current_best_score = -WIN_SCORE * 2;
        int current_best_x = -1, current_best_y = -1;
        int alpha = -WIN_SCORE * 2;
        int beta = WIN_SCORE * 2;

        for (const auto& candidate : candidates) {
            if (tm.should_stop()) break;

            board[candidate.x][candidate.y] = AI;
            zobrist_hash.update(candidate.x, candidate.y, EMPTY, AI);

            int score = alpha_beta_with_tt(depth - 1, alpha, beta, false, AI, tm);

            board[candidate.x][candidate.y] = EMPTY;
            zobrist_hash.update(candidate.x, candidate.y, AI, EMPTY);

            if (score > current_best_score) {
                current_best_score = score;
                current_best_x = candidate.x;
                current_best_y = candidate.y;
                best_score = current_best_score;
                best_x = current_best_x;
                best_y = current_best_y;
                alpha = std::max(alpha, score);
            }

            if (tm.should_stop()) break;
        }

        // 如果找到必胜解，提前结束
        if (best_score >= WIN_SCORE - 1000) {
            break;
        }

        if (tm.should_stop()) break;
    }

    if (best_x != -1 && best_y != -1) {
        board[best_x][best_y] = AI;
        zobrist_hash.update(best_x, best_y, EMPTY, AI);
        steps[step_count++] = Step{ AI, best_x, best_y, best_score };
        printf("AI落子(%d, %d)\n", best_x + 1, best_y + 1);
    }
    else {
        // 备用方案
        printf("使用备用搜索\n");
        auto candidates = get_candidate_moves(AI, 10);
        if (!candidates.empty()) {
            best_x = candidates[0].x;
            best_y = candidates[0].y;
            board[best_x][best_y] = AI;
            zobrist_hash.update(best_x, best_y, EMPTY, AI);
            steps[step_count++] = Step{ AI, best_x, best_y, candidates[0].score };
            printf("AI落子(%d, %d)\n", best_x + 1, best_y + 1);
        }
    }
}

void ai_move(int depth) {
    // 根据游戏阶段调整最大深度
    int max_depth;
    if (step_count < 10) {
        max_depth = 4;
    }
    else if (step_count < 100) {
        max_depth = 6;
    }
    else {
        max_depth = 8;
    }

    ai_move_with_iterative_deepening(max_depth);
}

void review_process()
{
    printf("\n复盘记录(总步数：%d)\n", step_count);
    clear_input_buffer();

    int temp_board[BOARD_SIZE][BOARD_SIZE];
    memset(temp_board, EMPTY, sizeof(temp_board));

    for (int i = 0; i < step_count; i++)
    {
        Step s = steps[i];
        temp_board[s.x][s.y] = s.player;

        printf("\n第%d步: %s 落子于(%d, %d)\n",
            i + 1,
            (s.player == PLAYER) ? "玩家" : "AI",
            s.x + 1, s.y + 1);

        printf("   ");
        for (int col = 0; col < BOARD_SIZE; col++) {
            printf("%2d", col + 1);
        }
        printf("\n");

        for (int row = 0; row < BOARD_SIZE; row++)
        {
            printf("%2d ", row + 1);
            for (int col = 0; col < BOARD_SIZE; col++)
            {
                if (temp_board[row][col] == PLAYER)
                    printf(" X");
                else if (temp_board[row][col] == AI)
                    printf(" O");
                else
                    printf(" .");
            }
            printf("\n");
        }

        if (i < step_count - 1)
        {
            printf("\n按Enter继续下一步...");
            clear_input_buffer();
        }
    }
    printf("\n复盘结束！按Enter返回...");
    clear_input_buffer();
}