#define _CRT_SECURE_NO_WARNINGS
#include <graphics.h>
#include <conio.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <sstream>
#include "gobang.h"

using namespace std;

const int GRID_SIZE = 40;
const int MARGIN = 50;
const int WINDOW_WIDTH = 2 * MARGIN + BOARD_SIZE * GRID_SIZE;
const int WINDOW_HEIGHT = 2 * MARGIN + BOARD_SIZE * GRID_SIZE + 120;

const COLORREF BOARD_COLOR = RGB(220, 179, 92);
const COLORREF LINE_COLOR = BLACK;
const COLORREF PLAYER_COLOR = BLACK;
const COLORREF AI_COLOR = WHITE;
const COLORREF TEXT_COLOR = BLACK;
const COLORREF BUTTON_COLOR = RGB(70, 130, 180);
const COLORREF BUTTON_HOVER_COLOR = RGB(100, 149, 237);
const COLORREF BUTTON_DISABLED_COLOR = RGB(180, 180, 180);
const COLORREF BACKGROUND_COLOR = RGB(240, 240, 240);

bool game_running = true;
int hover_x = -1, hover_y = -1;
bool need_redraw = true;
int current_player;

void init_graphics() {
    initgraph(WINDOW_WIDTH, WINDOW_HEIGHT);
    setbkcolor(BACKGROUND_COLOR);
    cleardevice();
    settextcolor(TEXT_COLOR);
}

void draw_board() {
    setfillcolor(BOARD_COLOR);
    solidrectangle(MARGIN - 10, MARGIN - 10,
        MARGIN + BOARD_SIZE * GRID_SIZE + 10,
        MARGIN + BOARD_SIZE * GRID_SIZE + 10);

    setlinecolor(LINE_COLOR);
    setlinestyle(PS_SOLID, 2);

    for (int i = 0; i < BOARD_SIZE; i++) {
        line(MARGIN, MARGIN + i * GRID_SIZE,
            MARGIN + (BOARD_SIZE - 1) * GRID_SIZE, MARGIN + i * GRID_SIZE);
        line(MARGIN + i * GRID_SIZE, MARGIN,
            MARGIN + i * GRID_SIZE, MARGIN + (BOARD_SIZE - 1) * GRID_SIZE);
    }

    setfillcolor(BLACK);
    int star_positions[5][2] = { {3,3}, {3,11}, {7,7}, {11,3}, {11,11} };
    for (int i = 0; i < 5; i++) {
        int x = MARGIN + star_positions[i][0] * GRID_SIZE;
        int y = MARGIN + star_positions[i][1] * GRID_SIZE;
        solidcircle(x, y, 4);
    }

    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] != EMPTY) {
                int x = MARGIN + i * GRID_SIZE;
                int y = MARGIN + j * GRID_SIZE;

                if (board[i][j] == PLAYER) {
                    setfillcolor(PLAYER_COLOR);
                    solidcircle(x, y, GRID_SIZE / 2 - 2);
                }
                else {
                    setfillcolor(AI_COLOR);
                    solidcircle(x, y, GRID_SIZE / 2 - 2);
                    setlinecolor(BLACK);
                    circle(x, y, GRID_SIZE / 2 - 2);
                }
            }
        }
    }
}

void draw_button(int x, int y, int width, int height, const TCHAR* text, bool hover = false, bool enabled = true) {
    if (enabled) {
        COLORREF color = hover ? BUTTON_HOVER_COLOR : BUTTON_COLOR;
        setfillcolor(color);
    }
    else {
        setfillcolor(BUTTON_DISABLED_COLOR);
    }

    solidroundrect(x, y, x + width, y + height, 10, 10);

    if (enabled) {
        settextcolor(WHITE);
    }
    else {
        settextcolor(RGB(100, 100, 100));
    }

    settextstyle(14, 0, _T("宋体"));
    setbkmode(TRANSPARENT);

    int text_width = textwidth(text);
    int text_height = textheight(text);
    outtextxy(x + (width - text_width) / 2, y + (height - text_height) / 2, text);

    settextcolor(TEXT_COLOR);
}

void draw_game_info() {
    int info_y = MARGIN + BOARD_SIZE * GRID_SIZE + 10;

    setfillcolor(BACKGROUND_COLOR);
    solidrectangle(0, info_y - 5, WINDOW_WIDTH, WINDOW_HEIGHT);

    settextcolor(TEXT_COLOR);
    settextstyle(16, 0, _T("宋体"));
    setbkmode(TRANSPARENT);

    if (game_running) {
        if ((step_count == 0 && game_mode == PLAYER_FIRST) ||
            (step_count > 0 && steps[step_count - 1].player == AI)) {
            outtextxy(MARGIN, info_y, _T("玩家回合 - 点击落子"));
        }
        else {
            outtextxy(MARGIN, info_y, _T("AI思考中..."));
        }
    }
    else {
        if (step_count > 0) {
            if (check_win(steps[step_count - 1].x, steps[step_count - 1].y, PLAYER)) {
                outtextxy(MARGIN, info_y, _T("游戏结束！玩家获胜！"));
            }
            else if (check_win(steps[step_count - 1].x, steps[step_count - 1].y, AI)) {
                outtextxy(MARGIN, info_y, _T("游戏结束！AI获胜！"));
            }
            else {
                outtextxy(MARGIN, info_y, _T("游戏结束！平局！"));
            }
        }
    }

    TCHAR step_text[50];
    _stprintf_s(step_text, _T("步数: %d"), step_count);
    outtextxy(WINDOW_WIDTH - 150, info_y, step_text);

    if (forbidden_mode && game_mode == PLAYER_FIRST) {
        outtextxy(MARGIN, info_y + 25, _T("禁手规则: 开启"));
    }
    else {
        outtextxy(MARGIN, info_y + 25, _T("禁手规则: 关闭"));
    }

    outtextxy(MARGIN, info_y + 50, _T("按 U 悔棋, 按 R 重新开始, 按 ESC 退出"));

    int undo_button_x = WINDOW_WIDTH - 120;
    int undo_button_y = info_y + 45;
    bool can_undo = step_count > (current_player == AI ? 1 : 0);

    bool hover_undo = false;
    if (MouseHit()) {
        MOUSEMSG m = GetMouseMsg();
        hover_undo = (m.x >= undo_button_x && m.x <= undo_button_x + 80 &&
            m.y >= undo_button_y && m.y <= undo_button_y + 30);
    }

    draw_button(undo_button_x, undo_button_y, 80, 30, _T("悔棋(U)"), hover_undo, can_undo);

    int restart_button_x = WINDOW_WIDTH - 220;
    int restart_button_y = info_y + 45;
    bool hover_restart = false;
    if (MouseHit()) {
        MOUSEMSG m = GetMouseMsg();
        hover_restart = (m.x >= restart_button_x && m.x <= restart_button_x + 80 &&
            m.y >= restart_button_y && m.y <= restart_button_y + 30);
    }

    draw_button(restart_button_x, restart_button_y, 80, 30, _T("重新开始(R)"), hover_restart, true);
}

void draw_hover_effect() {
    if (hover_x != -1 && hover_y != -1 && board[hover_x][hover_y] == EMPTY) {
        int x = MARGIN + hover_x * GRID_SIZE;
        int y = MARGIN + hover_y * GRID_SIZE;

        setlinecolor(RED);
        setlinestyle(PS_DASH, 1);
        circle(x, y, GRID_SIZE / 2 - 1);
        setlinestyle(PS_SOLID, 2);
    }
}

void draw_all() {
    cleardevice();
    draw_board();
    draw_hover_effect();
    draw_game_info();
}

int show_menu() {
    initgraph(400, 500);
    setbkcolor(BACKGROUND_COLOR);
    cleardevice();

    settextcolor(TEXT_COLOR);
    settextstyle(24, 0, _T("宋体"));
    setbkmode(TRANSPARENT);
    outtextxy(120, 50, _T("五子棋游戏"));

    settextstyle(16, 0, _T("宋体"));
    outtextxy(100, 100, _T("选择游戏设置"));

    int button_y = 150;
    int button_width = 200;
    int button_height = 40;
    int button_x = (400 - button_width) / 2;

    outtextxy(button_x, button_y - 25, _T("先后手:"));
    draw_button(button_x, button_y, button_width, button_height,
        game_mode == PLAYER_FIRST ? _T("玩家先手 (黑棋)") : _T("AI先手 (白棋)"));

    outtextxy(button_x, button_y + 60, _T("禁手规则:"));
    if (game_mode == PLAYER_FIRST) {
        draw_button(button_x, button_y + 85, button_width, button_height,
            forbidden_mode ? _T("开启禁手") : _T("关闭禁手"));
    }
    else {
        draw_button(button_x, button_y + 85, button_width, button_height,
            _T("AI先手无禁手"));
    }

    draw_button(button_x, button_y + 170, button_width, button_height, _T("开始游戏"));

    draw_button(button_x, button_y + 230, button_width, button_height, _T("退出游戏"));

    MOUSEMSG m;
    while (true) {
        m = GetMouseMsg();

        if (m.uMsg == WM_LBUTTONDOWN) {
            if (m.x >= button_x && m.x <= button_x + button_width &&
                m.y >= button_y && m.y <= button_y + button_height) {
                game_mode = (game_mode == PLAYER_FIRST) ? AI_FIRST : PLAYER_FIRST;
                closegraph();
                return show_menu();
            }

            if (game_mode == PLAYER_FIRST &&
                m.x >= button_x && m.x <= button_x + button_width &&
                m.y >= button_y + 85 && m.y <= button_y + 85 + button_height) {
                forbidden_mode = !forbidden_mode;
                closegraph();
                return show_menu();
            }

            if (m.x >= button_x && m.x <= button_x + button_width &&
                m.y >= button_y + 170 && m.y <= button_y + 170 + button_height) {
                closegraph();
                return 1;
            }

            if (m.x >= button_x && m.x <= button_x + button_width &&
                m.y >= button_y + 230 && m.y <= button_y + 230 + button_height) {
                closegraph();
                return 0;
            }
        }

        cleardevice();
        settextcolor(TEXT_COLOR);
        settextstyle(24, 0, _T("宋体"));
        outtextxy(120, 50, _T("五子棋游戏"));

        settextstyle(16, 0, _T("宋体"));
        outtextxy(100, 100, _T("选择游戏设置"));

        outtextxy(button_x, button_y - 25, _T("先后手:"));
        bool hover1 = (m.x >= button_x && m.x <= button_x + button_width &&
            m.y >= button_y && m.y <= button_y + button_height);
        draw_button(button_x, button_y, button_width, button_height,
            game_mode == PLAYER_FIRST ? _T("玩家先手 (黑棋)") : _T("AI先手 (白棋)"), hover1);

        outtextxy(button_x, button_y + 60, _T("禁手规则:"));
        bool hover2 = false;
        if (game_mode == PLAYER_FIRST) {
            hover2 = (m.x >= button_x && m.x <= button_x + button_width &&
                m.y >= button_y + 85 && m.y <= button_y + 85 + button_height);
            draw_button(button_x, button_y + 85, button_width, button_height,
                forbidden_mode ? _T("开启禁手") : _T("关闭禁手"), hover2);
        }
        else {
            draw_button(button_x, button_y + 85, button_width, button_height,
                _T("AI先手无禁手"));
        }

        bool hover3 = (m.x >= button_x && m.x <= button_x + button_width &&
            m.y >= button_y + 170 && m.y <= button_y + 170 + button_height);
        draw_button(button_x, button_y + 170, button_width, button_height, _T("开始游戏"), hover3);

        bool hover4 = (m.x >= button_x && m.x <= button_x + button_width &&
            m.y >= button_y + 230 && m.y <= button_y + 230 + button_height);
        draw_button(button_x, button_y + 230, button_width, button_height, _T("退出游戏"), hover4);

        Sleep(10);
    }
}

bool show_game_over_dialog() {
    initgraph(300, 200);
    setbkcolor(BACKGROUND_COLOR);
    cleardevice();

    settextcolor(TEXT_COLOR);
    settextstyle(18, 0, _T("宋体"));
    setbkmode(TRANSPARENT);

    if (step_count > 0) {
        if (check_win(steps[step_count - 1].x, steps[step_count - 1].y, PLAYER)) {
            outtextxy(80, 40, _T("玩家获胜！"));
        }
        else if (check_win(steps[step_count - 1].x, steps[step_count - 1].y, AI)) {
            outtextxy(80, 40, _T("AI获胜！"));
        }
        else {
            outtextxy(80, 40, _T("平局！"));
        }
    }

    int button_width = 120;
    int button_height = 40;
    int button_x = (300 - button_width) / 2;

    draw_button(button_x, 90, button_width, button_height, _T("重新开始"));
    draw_button(button_x, 150, button_width, button_height, _T("返回菜单"));

    MOUSEMSG m;
    bool result = false;

    while (true) {
        m = GetMouseMsg();
        if (m.uMsg == WM_LBUTTONDOWN) {
            if (m.x >= button_x && m.x <= button_x + button_width &&
                m.y >= 90 && m.y <= 90 + button_height) {
                result = true;
                break;
            }
            if (m.x >= button_x && m.x <= button_x + button_width &&
                m.y >= 150 && m.y <= 150 + button_height) {
                result = false;
                break;
            }
        }
        Sleep(10);
    }

    closegraph();
    return result;
}

void handle_undo() {
    int min_steps_needed = (current_player == AI) ? 2 : 1;
    if (step_count >= min_steps_needed) {
        int undo_steps = (current_player == AI) ? 2 : 1;
        if (undo_move(undo_steps)) {
            current_player = PLAYER;
            need_redraw = true;
        }
    }
}

void game_loop() {
    init_graphics();
    empty_board();
    game_running = true;
    need_redraw = true;

    current_player = (game_mode == PLAYER_FIRST) ? PLAYER : AI;

    if (current_player == AI) {
        ai_move(4);  // 使用固定深度搜索
        current_player = PLAYER;
        need_redraw = true;
    }

    draw_all();

    while (game_running) {
        if (need_redraw) {
            draw_all();
            need_redraw = false;
        }

        if (MouseHit()) {
            MOUSEMSG m = GetMouseMsg();

            bool hover_changed = false;
            if (m.x >= MARGIN && m.x < MARGIN + BOARD_SIZE * GRID_SIZE &&
                m.y >= MARGIN && m.y < MARGIN + BOARD_SIZE * GRID_SIZE) {
                int new_hover_x = (m.x - MARGIN + GRID_SIZE / 2) / GRID_SIZE;
                int new_hover_y = (m.y - MARGIN + GRID_SIZE / 2) / GRID_SIZE;

                if (new_hover_x != hover_x || new_hover_y != hover_y) {
                    hover_x = new_hover_x;
                    hover_y = new_hover_y;
                    hover_changed = true;
                }
            }
            else if (hover_x != -1 || hover_y != -1) {
                hover_x = -1;
                hover_y = -1;
                hover_changed = true;
            }

            if (hover_changed) {
                draw_board();
                draw_hover_effect();
                draw_game_info();
            }

            if (m.uMsg == WM_LBUTTONDOWN && current_player == PLAYER && game_running) {
                if (hover_x >= 0 && hover_x < BOARD_SIZE &&
                    hover_y >= 0 && hover_y < BOARD_SIZE) {

                    if (player_move(hover_x, hover_y)) {
                        need_redraw = true;

                        if (check_win(hover_x, hover_y, PLAYER)) {
                            game_running = false;
                        }
                        else if (step_count >= MAX_STEPS) {
                            game_running = false;
                        }
                        else {
                            current_player = AI;
                        }
                    }
                }
            }

            int undo_button_x = WINDOW_WIDTH - 120;
            int undo_button_y = MARGIN + BOARD_SIZE * GRID_SIZE + 55;
            if (m.uMsg == WM_LBUTTONDOWN &&
                m.x >= undo_button_x && m.x <= undo_button_x + 80 &&
                m.y >= undo_button_y && m.y <= undo_button_y + 30) {
                handle_undo();
            }

            int restart_button_x = WINDOW_WIDTH - 220;
            int restart_button_y = MARGIN + BOARD_SIZE * GRID_SIZE + 55;
            if (m.uMsg == WM_LBUTTONDOWN &&
                m.x >= restart_button_x && m.x <= restart_button_x + 80 &&
                m.y >= restart_button_y && m.y <= restart_button_y + 30) {
                game_running = false;
            }
        }

        if (_kbhit()) {
            int key = _getch();

            if (key == 27) {
                break;
            }
            else if (key == 'r' || key == 'R') {
                game_running = false;
            }
            else if (key == 'u' || key == 'U') {
                handle_undo();
            }
        }

        if (current_player == AI && game_running) {
            draw_game_info();
            Sleep(300);  // 减少等待时间

            ai_move(4);  // 使用固定深度搜索
            need_redraw = true;

            if (check_win(steps[step_count - 1].x, steps[step_count - 1].y, AI)) {
                game_running = false;
            }
            else if (step_count >= MAX_STEPS) {
                game_running = false;
            }
            else {
                current_player = PLAYER;
            }
        }

        Sleep(10);
    }

    draw_all();
    Sleep(1000);

    bool restart = show_game_over_dialog();
    closegraph();

    if (restart) {
        game_loop();
    }
}

int main() {
    srand(static_cast<unsigned int>(time(nullptr)));
    init_opening_book();
    init_zobrist();

    while (true) {
        int result = show_menu();
        if (result == 0) {
            break;
        }

        game_loop();
    }

    return 0;
}
