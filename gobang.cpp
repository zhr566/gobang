#define _CRT_SECURE_NO_WARNINGS
#include "gobang.h"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <random>

// ȫ�ֱ�������
int board[BOARD_SIZE][BOARD_SIZE] = { 0 };
const int direction[8][2] = { {1, 0}, {0, 1}, {1, 1}, {1, -1},{ -1,0},{0,-1},{-1,1},{-1,-1} };
Step steps[MAX_STEPS];
int step_count = 0;
int game_mode = PLAYER_FIRST;
int forbidden_mode = NO_FORBIDDEN;
std::unordered_map<std::string, std::vector<std::pair<int, int>>> opening_book;
TranspositionTable transposition_table(TT_SIZE);
ZobristHash zobrist_hash;

// Ԥ����ģʽ������
const int PATTERN_SCORES[3][5] = {
    // ����������, ���˿ո���� -> ����
    // ������: 2,3,4,5,6
    // �ո�: 0(�޿ո�),1(һ��),2(����)
    {10, 50, 500, 5000, 50000},  // 0�˿ո�
    {20, 100, 2000, 20000, 200000}, // 1�˿ո�  
    {50, 500, 5000, 50000, 500000}  // 2�˿ո�
};

// ��в���ֵȼ��������޸ĺ󣺻��� > ˫������
// ע�⣺WIN_SCORE �Ѿ���ͷ�ļ��ж��壬����ֻ������������
const int BLOCK_WIN_SCORE = 100000;     // ���ػ�ʤ��в
const int BLOCK_FOUR_SCORE = 9000;      // ���س���
const int BLOCK_LIVE_THREE_SCORE = 8500; // ���ػ���
const int BLOCK_JUMP_THREE_SCORE = 8300; // ����������
const int BLOCK_DOUBLE_THREE_SCORE = 8000; // ����˫����
const int BLOCK_SLEEP_THREE_SCORE = 7000; // ��������

// λ�ü�ֵ�� (15��15����)
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

// ��ʼ��Zobrist��ϣ
void init_zobrist() {
    // ���¼��㵱ǰ���̵Ĺ�ϣֵ
    zobrist_hash = ZobristHash();
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] != EMPTY) {
                zobrist_hash.update(i, j, EMPTY, board[i][j]);
            }
        }
    }
}

// ��ʼ�����ֱ�
void init_opening_book() {
    // ��տ��ֱ�
    opening_book.clear();

    // ��׼���� - ��Ԫ����
    opening_book[""] = { {7, 7} };

    // ��Ԫ���ֺ��Ӧ��
    opening_book["7,7"] = { {6, 7}, {7, 6}, {8, 7}, {7, 8}, {6, 6}, {8, 8} };

    // ��λ����
    opening_book["3,3"] = { {3, 11}, {11, 3}, {11, 11}, {7, 7} };
    opening_book["3,11"] = { {3, 3}, {11, 3}, {11, 11}, {7, 7} };
    opening_book["11,3"] = { {3, 3}, {3, 11}, {11, 11}, {7, 7} };
    opening_book["11,11"] = { {3, 3}, {3, 11}, {11, 3}, {7, 7} };
}

// ��ȡ����״̬�Ĺ�ϣ�ַ���
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

// �ӿ��ֱ��л�ȡ�Ƽ����߷�
std::vector<std::pair<int, int>> get_opening_moves() {
    if (step_count >= OPENING_BOOK_MAX_MOVES) {
        return {};
    }

    std::string hash = get_board_hash();

    // ֱ��ƥ��
    if (opening_book.find(hash) != opening_book.end()) {
        return opening_book[hash];
    }

    return {};
}

// ���幦��
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

// ������
int check_forbidden(int x, int y, int player) {
    if (player != PLAYER) {
        return NO_FORBIDDEN_MOVE;
    }

    if (forbidden_mode == 0) {  // �޸���ʹ�� == 0 ������ !
        return NO_FORBIDDEN_MOVE;
    }

    // ��ʱ��������
    board[x][y] = player;

    int three_count = 0;
    int four_count = 0;
    bool overline = false;

    // ���ÿ������
    for (int i = 0; i < 8; i++) {
        int dx = direction[i][0];
        int dy = direction[i][1];

        DirInfo info = count_specific_direction(x, y, dx, dy, player);

        // ��鳤�����֣�����5�ӣ�
        if (info.continuous_chess > 5) {
            overline = true;
        }

        // �������ͻ���
        if (info.check_start && info.check_end) {
            if (info.continuous_chess == 3) {
                three_count++;
            }
            else if (info.continuous_chess == 4) {
                four_count++;
            }
        }
    }

    // �Ƴ���ʱ����
    board[x][y] = EMPTY;

    // �жϽ�������
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

// �ж��Ƿ�Ϊ����
bool is_forbidden_move(int x, int y, int player) {
    return check_forbidden(x, y, player) != NO_FORBIDDEN_MOVE;
}

// ��ʾ���ֹ���
void show_forbidden_rules() {
    printf("\n���ֹ���˵��:\n");
    printf("- ���壨��ң������½���:\n");
    printf("  1. ��������: ͬʱ�γ����������ϵĻ���\n");
    printf("  2. ���Ľ���: ͬʱ�γ����������ϵĻ���\n");
    printf("  3. ��������: �γɳ������ӵ�����\n");
    printf("- ���壨AI��û�н���\n");
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

    // ��ӡ��������
    printf("   ");
    for (int j = 0; j < BOARD_SIZE; j++) {
        printf("%2d", j + 1);
    }
    printf("\n");

    for (int i = 0; i < BOARD_SIZE; i++) {
        // ��ӡ�к�
        printf("%2d ", i + 1);

        // ��ӡ��������
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

    // ��ӡ�ײ�����
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

    // ������
    if (forbidden_mode && is_forbidden_move(x, y, PLAYER)) {
        int forbidden_type = check_forbidden(x, y, PLAYER);
        printf("���֣�");
        switch (forbidden_type) {
        case DOUBLE_THREE_FORBIDDEN:
            printf("��������\n");
            break;
        case DOUBLE_FOUR_FORBIDDEN:
            printf("���Ľ���\n");
            break;
        case OVERLINE_FORBIDDEN:
            printf("��������\n");
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

    // ���������
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

    // ��鷴����
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

// �Ż����������� - ʹ��Ԥ����ģʽ
int evaluate_pos_fast(int x, int y, int player) {
    int original = board[x][y];
    board[x][y] = player;

    int total_score = 0;

    for (int i = 0; i < 8; i++) {
        int dx = direction[i][0], dy = direction[i][1];
        DirInfo info = count_specific_direction(x, y, dx, dy, player);

        // ʹ��Ԥ����ģʽ��
        int open_ends = (info.check_start ? 1 : 0) + (info.check_end ? 1 : 0);
        if (info.continuous_chess >= 2 && info.continuous_chess <= 6) {
            total_score += PATTERN_SCORES[open_ends][info.continuous_chess - 2];
        }

        // ����ֱ�ӷ�����߷�
        if (info.continuous_chess >= 5) {
            board[x][y] = original;
            return WIN_SCORE;
        }
    }

    board[x][y] = original;

    // λ�ý���
    int position_bonus = POSITION_VALUE[x][y] * 10;

    return total_score + position_bonus;
}

// ��������������⺯��
bool check_jump_three(int x, int y, int dx, int dy, int player) {
    // ��������������̬��
    // 1. ��.��� (�м���һ����λ)
    // 2. ���.�� (�м���һ����λ)

    // ���������
    int nx1 = x + dx, ny1 = y + dy;
    int nx2 = x + 2 * dx, ny2 = y + 2 * dy;
    int nx3 = x + 3 * dx, ny3 = y + 3 * dy;

    // ��鷴����
    int px1 = x - dx, py1 = y - dy;
    int px2 = x - 2 * dx, py2 = y - 2 * dy;
    int px3 = x - 3 * dx, py3 = y - 3 * dy;

    // ��̬1: ��.��� (��ǰλ���ǵ�һ������)
    if (have_space(nx1, ny1) &&
        board[nx2][ny2] == player &&
        board[nx3][ny3] == player) {
        // ��������Ƿ�Ϊ��
        bool start_empty = have_space(px1, py1);
        bool end_empty = (nx3 + dx >= 0 && nx3 + dx < BOARD_SIZE &&
            ny3 + dy >= 0 && ny3 + dy < BOARD_SIZE &&
            board[nx3 + dx][ny3 + dy] == EMPTY);
        if (start_empty && end_empty) return true;
    }

    // ��̬2: ���.�� (��ǰλ���ǵ���������)
    if (board[px1][py1] == player &&
        have_space(nx1, ny1) &&
        board[nx2][ny2] == player) {
        // ��������Ƿ�Ϊ��
        bool start_empty = (px1 - dx >= 0 && px1 - dx < BOARD_SIZE &&
            py1 - dy >= 0 && py1 - dy < BOARD_SIZE &&
            board[px1 - dx][py1 - dy] == EMPTY);
        bool end_empty = have_space(nx2 + dx, ny2 + dy);
        if (start_empty && end_empty) return true;
    }

    return false;
}

// �Ľ��Ķ»������ - ����������
bool block_live_three_improved(int player) {
    int opponent = (player == AI) ? PLAYER : AI;

    // �洢���л�����вλ�ú���в�̶�
    std::vector<std::pair<int, int>> threat_positions;
    std::vector<int> threat_scores;

    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] != EMPTY) continue;

            // ������λ���Ƿ��ܶ�ס���γɵĻ���
            int threat_level = 0;

            // ����ĸ�����
            for (int dir = 0; dir < 4; dir++) {
                int dx = direction[dir][0];
                int dy = direction[dir][1];

                // ֱ�Ӽ�鵱ǰ�����϶����Ѿ��γɵĻ���������Ҫģ����ã�
                DirInfo info = count_specific_direction(i, j, dx, dy, opponent);

                // ��������������3�ӣ������˶��пո�
                if (info.continuous_chess == 3 && info.check_start && info.check_end) {
                    threat_level = BLOCK_LIVE_THREE_SCORE;
                    break;
                }

                // ���������
                if (check_jump_three(i, j, dx, dy, opponent)) {
                    threat_level = BLOCK_JUMP_THREE_SCORE;
                    break;
                }

                // ��������в
                if (info.continuous_chess == 4) {
                    threat_level = BLOCK_FOUR_SCORE;
                    break;
                }
            }

            // �����⵽��в����¼�������λ��
            if (threat_level > 0) {
                threat_positions.push_back({ i, j });
                threat_scores.push_back(threat_level);
            }
        }
    }

    // ���û����в������false
    if (threat_positions.empty()) {
        return false;
    }

    // �ҵ���в����λ��
    int best_index = 0;
    int max_threat = 0;
    for (int i = 0; i < threat_scores.size(); i++) {
        if (threat_scores[i] > max_threat) {
            max_threat = threat_scores[i];
            best_index = i;
        }
    }

    // ����в����λ�÷���
    int best_x = threat_positions[best_index].first;
    int best_y = threat_positions[best_index].second;

    board[best_x][best_y] = player;
    zobrist_hash.update(best_x, best_y, EMPTY, player);
    steps[step_count++] = Step{ player, best_x, best_y, max_threat };
    printf("%s����(%d, %d) - �»���(��в�ȼ�:%d)\n",
        (player == AI) ? "AI" : "���", best_x + 1, best_y + 1, max_threat);

    return true;
}

// ר�ż��������������
bool block_jump_three(int player) {
    int opponent = (player == AI) ? PLAYER : AI;

    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] != EMPTY) continue;

            // ����ĸ������������
            for (int dir = 0; dir < 4; dir++) {
                int dx = direction[dir][0];
                int dy = direction[dir][1];

                if (check_jump_three(i, j, dx, dy, opponent)) {
                    // ������������в����������
                    board[i][j] = player;
                    zobrist_hash.update(i, j, EMPTY, player);
                    steps[step_count++] = Step{ player, i, j, BLOCK_JUMP_THREE_SCORE };
                    printf("%s����(%d, %d) - ��������\n",
                        (player == AI) ? "AI" : "���", i + 1, j + 1);
                    return true;
                }
            }
        }
    }
    return false;
}

// ��鲢��ס����
bool block_four(int player) {
    int opponent = (player == AI) ? PLAYER : AI;

    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] != EMPTY) continue;

            // ��ʱ���ö�������
            board[i][j] = opponent;

            // ����Ƿ��γɳ��ģ�һ�˱���ס�����ӣ�
            for (int dir = 0; dir < 8; dir++) {
                int dx = direction[dir][0];
                int dy = direction[dir][1];

                DirInfo info = count_specific_direction(i, j, dx, dy, opponent);

                // ��������������4�ӣ���ֻ��һ���пո�
                if (info.continuous_chess == 4 &&
                    ((info.check_start && !info.check_end) ||
                        (!info.check_start && info.check_end))) {
                    // �ָ���λ������
                    board[i][j] = EMPTY;
                    board[i][j] = player;
                    zobrist_hash.update(i, j, EMPTY, player);
                    steps[step_count++] = Step{ player, i, j, BLOCK_FOUR_SCORE };
                    printf("%s����(%d, %d) - �³���\n",
                        (player == AI) ? "AI" : "���", i + 1, j + 1);
                    return true;
                }
            }

            // �ָ���λ
            board[i][j] = EMPTY;
        }
    }
    return false;
}

// ��鲢��ס˫���������ȼ����ͣ�
bool block_double_three(int player) {
    int opponent = (player == AI) ? PLAYER : AI;

    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] != EMPTY) continue;

            // ��ʱ���ö�������
            board[i][j] = opponent;

            // ����Ƿ��γ�˫����
            int live_three_count = 0;
            for (int dir = 0; dir < 8; dir++) {
                int dx = direction[dir][0];
                int dy = direction[dir][1];

                DirInfo info = count_specific_direction(i, j, dx, dy, opponent);

                // ��������
                if (info.continuous_chess == 3 && info.check_start && info.check_end) {
                    live_three_count++;
                }

                // ���������
                if (check_jump_three(i, j, dx, dy, opponent)) {
                    live_three_count++;
                }
            }

            // �ָ���λ
            board[i][j] = EMPTY;

            // �������˫������в����������
            if (live_three_count >= 2) {
                board[i][j] = player;
                zobrist_hash.update(i, j, EMPTY, player);
                steps[step_count++] = Step{ player, i, j, BLOCK_DOUBLE_THREE_SCORE };
                printf("%s����(%d, %d) - ��˫����\n",
                    (player == AI) ? "AI" : "���", i + 1, j + 1);
                return true;
            }
        }
    }
    return false;
}

// ������������أ�һ�˱���ס�����ӣ�
bool block_sleep_three(int player) {
    int opponent = (player == AI) ? PLAYER : AI;

    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] != EMPTY) continue;

            // ������λ���Ƿ����γ�����
            board[i][j] = opponent;

            for (int dir = 0; dir < 8; dir++) {
                int dx = direction[dir][0];
                int dy = direction[dir][1];

                DirInfo info = count_specific_direction(i, j, dx, dy, opponent);

                // ��������������3�ӣ���ֻ��һ���пո�
                if (info.continuous_chess == 3 &&
                    ((info.check_start && !info.check_end) ||
                        (!info.check_start && info.check_end))) {

                    // �ָ���λ������
                    board[i][j] = EMPTY;
                    board[i][j] = player;
                    zobrist_hash.update(i, j, EMPTY, player);
                    steps[step_count++] = Step{ player, i, j, BLOCK_SLEEP_THREE_SCORE };
                    printf("%s����(%d, %d) - ������\n",
                        (player == AI) ? "AI" : "���", i + 1, j + 1);
                    return true;
                }
            }

            board[i][j] = EMPTY;
        }
    }
    return false;
}

// ����ʤ��в������
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
                    printf("%s����(%d, %d) - �������ػ�ʤ��в\n",
                        (player == AI) ? "AI" : "���", i + 1, j + 1);
                    return true;
                }
                board[i][j] = EMPTY;
            }
        }
    }
    return false;
}

// ���������Ǳ����в
int evaluate_potential_threat(int x, int y, int player) {
    int opponent = (player == AI) ? PLAYER : AI;
    int threat_score = 0;

    // ��ʱ���ö�������
    board[x][y] = opponent;

    // ����ʤ��в
    if (check_win(x, y, opponent)) {
        board[x][y] = EMPTY;
        return 10000; // �����в
    }

    // ��������в
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

    // ��������в
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

    // ˫������в
    if (live_three_count >= 2) {
        threat_score += 7500;
    }
    else if (live_three_count == 1) {
        threat_score += 6000;
    }

    return threat_score;
}

// ��������������������ĳλ�û��γɵ���в
bool check_potential_threat(int x, int y, int player) {
    int opponent = (player == AI) ? PLAYER : AI;

    // ��ʱ���ö�������
    board[x][y] = opponent;

    // ����Ƿ���γ�������ʤ
    if (check_win(x, y, opponent)) {
        board[x][y] = EMPTY;
        return true;
    }

    // ����Ƿ���γɳ���
    for (int dir = 0; dir < 8; dir++) {
        int dx = direction[dir][0];
        int dy = direction[dir][1];
        DirInfo info = count_specific_direction(x, y, dx, dy, opponent);

        if (info.continuous_chess == 4 && (info.check_start || info.check_end)) {
            board[x][y] = EMPTY;
            return true;
        }
    }

    // ����Ƿ���γ�˫����
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

    // ������γ�˫������Ҳ��������в
    return (live_three_count >= 2);
}

// �Ľ����ۺϷ��غ��� - ����Ԥ������в���
bool comprehensive_defense(int player) {
    int opponent = (player == AI) ? PLAYER : AI;

    // 1. ���������ʤ��в (������ȼ�)
    if (block_win_threat(player)) {
        return true;
    }

    // 2. ��������һ�������γɵ�������в
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] != EMPTY) continue;

            // �������������������Ƿ���γ�������в
            if (check_potential_threat(i, j, player)) {
                // �����������λ��
                board[i][j] = player;
                zobrist_hash.update(i, j, EMPTY, player);
                steps[step_count++] = Step{ player, i, j, 9500 };
                printf("%s����(%d, %d) - Ԥ���Է���\n",
                    (player == AI) ? "AI" : "���", i + 1, j + 1);
                return true;
            }
        }
    }

    // 3. ԭ�еķ����߼� (�����ȼ�˳��)
    if (block_four(player)) return true;
    if (block_live_three_improved(player)) return true;
    if (block_jump_three(player)) return true;
    if (block_double_three(player)) return true;
    if (block_sleep_three(player)) return true;

    return false;
}

// �Ľ��ĺ�ѡ�߷����� - ����Ǳ����в
std::vector<PosEvaluation> get_candidate_moves(int player, int max_moves) {
    std::vector<PosEvaluation> candidates;

    // �ڿ��ֽ׶Σ����ȿ��ǿ��ֱ���Ƽ�
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

    // 1. ���ȿ��Ƿ���Ǳ����в
    std::vector<std::pair<int, int>> threat_positions;
    std::vector<int> threat_scores;

    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] != EMPTY) continue;

            // �������λ�öԶ��ֵ�Ǳ����в
            int threat_level = evaluate_potential_threat(i, j, player);
            if (threat_level > 0) {
                threat_positions.push_back({ i, j });
                threat_scores.push_back(threat_level);
            }
        }
    }

    // ������вλ�ü����ѡ
    for (int i = 0; i < threat_positions.size() && i < max_moves / 2; i++) {
        int x = threat_positions[i].first;
        int y = threat_positions[i].second;
        candidates.push_back({ x, y, threat_scores[i] });
    }

    // 2. ԭ�еĺ�ѡ�߷������߼�
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] != EMPTY) continue;

            if (player == PLAYER && forbidden_mode && is_forbidden_move(i, j, PLAYER)) {
                continue;
            }

            bool has_neighbor = false;
            // �Ż��������ھӼ�鷶Χ
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

    // ʹ��nth_element���в������򣬱���ȫ�������
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

    // �Ż���ֻ���������ӵ�����
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

// ���û����Alpha-Beta����
int alpha_beta_with_tt(int depth, int alpha, int beta, bool maximizing_player, int player, TimeManager& tm) {
    if (tm.should_stop()) {
        return maximizing_player ? -WIN_SCORE : WIN_SCORE;
    }

    // ���ʤ��
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

    // ��ѯ�û���
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

            // �����߷�
            board[candidate.x][candidate.y] = AI;
            zobrist_hash.update(candidate.x, candidate.y, EMPTY, AI);

            int eval = alpha_beta_with_tt(depth - 1, alpha, beta, false, player, tm);

            // �����߷�
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

    // �洢���û���
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

// ԭ����alpha_beta���������ּ����ԣ�
int alpha_beta(int depth, int alpha, int beta, bool maximizing_player, int player, TimeManager& tm) {
    return alpha_beta_with_tt(depth, alpha, beta, maximizing_player, player, tm);
}

// �����������AI�ƶ�����
void ai_move_with_iterative_deepening(int max_depth) {
    TimeManager tm(TIME_LIMIT_MS);
    int best_x = -1, best_y = -1;
    int best_score = -WIN_SCORE * 2;

    // ��һ��������Ԫ�������AI���֣�
    if (step_count == 0 && game_mode == AI_FIRST) {
        board[7][7] = AI;
        zobrist_hash.update(7, 7, EMPTY, AI);
        steps[step_count++] = Step{ AI, 7, 7, 0 };
        printf("AI����(8, 8) - ��Ԫ����\n");
        return;
    }

    // ��鿪�ֱ�
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
                printf("AI����(%d, %d) - ���ֶ�ʽ\n", best_x + 1, best_y + 1);
                return;
            }
        }
    }

    // ����Ƿ����ֱ�ӻ�ʤ
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == EMPTY) {
                board[i][j] = AI;
                if (check_win(i, j, AI)) {
                    steps[step_count++] = Step{ AI, i, j, WIN_SCORE };
                    printf("AI����(%d, %d) - ��ʤ!\n", i + 1, j + 1);
                    return;
                }
                board[i][j] = EMPTY;
            }
        }
    }

    // ʹ�øĽ����ۺϷ��ز��ԣ�����Ԥ������в��⣩
    if (comprehensive_defense(AI)) {
        return;
    }

    // ������������
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

        // ����ҵ���ʤ�⣬��ǰ����
        if (best_score >= WIN_SCORE - 1000) {
            break;
        }

        if (tm.should_stop()) break;
    }

    if (best_x != -1 && best_y != -1) {
        board[best_x][best_y] = AI;
        zobrist_hash.update(best_x, best_y, EMPTY, AI);
        steps[step_count++] = Step{ AI, best_x, best_y, best_score };
        printf("AI����(%d, %d)\n", best_x + 1, best_y + 1);
    }
    else {
        // ���÷���
        printf("ʹ�ñ�������\n");
        auto candidates = get_candidate_moves(AI, 10);
        if (!candidates.empty()) {
            best_x = candidates[0].x;
            best_y = candidates[0].y;
            board[best_x][best_y] = AI;
            zobrist_hash.update(best_x, best_y, EMPTY, AI);
            steps[step_count++] = Step{ AI, best_x, best_y, candidates[0].score };
            printf("AI����(%d, %d)\n", best_x + 1, best_y + 1);
        }
    }
}

void ai_move(int depth) {
    // ������Ϸ�׶ε���������
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
    printf("\n���̼�¼(�ܲ�����%d)\n", step_count);
    clear_input_buffer();

    int temp_board[BOARD_SIZE][BOARD_SIZE];
    memset(temp_board, EMPTY, sizeof(temp_board));

    for (int i = 0; i < step_count; i++)
    {
        Step s = steps[i];
        temp_board[s.x][s.y] = s.player;

        printf("\n��%d��: %s ������(%d, %d)\n",
            i + 1,
            (s.player == PLAYER) ? "���" : "AI",
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
            printf("\n��Enter������һ��...");
            clear_input_buffer();
        }
    }
    printf("\n���̽�������Enter����...");
    clear_input_buffer();
}