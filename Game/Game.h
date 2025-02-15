#pragma once
#include <chrono>
#include <thread>

#include "../Models/Project_path.h"
#include "Board.h"
#include "Config.h"
#include "Hand.h"
#include "Logic.h"

class Game
{
  public:
    Game() : board(config("WindowSize", "Width"), config("WindowSize", "Hight")), hand(&board), logic(&board, &config)
    {
        ofstream fout(project_path + "log.txt", ios_base::trunc);
        fout.close();
    }

    // to start checkers
    int play()
    {
        auto start = chrono::steady_clock::now(); // текущее время 

        if (is_replay)
        {
            // если нужно начать сначала
            // то перезагрузить настройки
            // и перерисовать доску
            logic = Logic(&board, &config);
            config.reload();
            board.redraw();
        }
        else
        {
            // нарисовать доску
            board.start_draw();
        }
        is_replay = false; // сбрасываем этот флаг

        int turn_num = -1; // номер хода
        bool is_quit = false; // флаг выхода из программы
        const int Max_turns = config("Game", "MaxNumTurns"); // получение макс. кол-ва ходов из настроек
        
        // пока номер текущего хода меньше макс.
        while (++turn_num < Max_turns)
        {
            beat_series = 0;
            // найти доступные ходы
            logic.find_turns(turn_num % 2);
            // если нет доступных ходов, то закончить
            if (logic.turns.empty())
                break;
            // прочитать из настроек глубину рекурсии для бота текущего цвета
            logic.Max_depth = config("Bot", string((turn_num % 2) ? "Black" : "White") + string("BotLevel"));
            
            // проверка того, является ли игрок текущего цвета ботом
            if (!config("Bot", string("Is") + string((turn_num % 2) ? "Black" : "White") + string("Bot")))
            {
                // Ход человека

                // получить ответ от UI
                auto resp = player_turn(turn_num % 2);
                
                if (resp == Response::QUIT)
                {
                    // Выход
                    is_quit = true;
                    break;
                }
                else if (resp == Response::REPLAY)
                {
                    // Играть заново
                    is_replay = true;
                    break;
                }
                else if (resp == Response::BACK)
                {
                    // Один ход назад
                    if (config("Bot", string("Is") + string((1 - turn_num % 2) ? "Black" : "White") + string("Bot")) &&
                        !beat_series && board.history_mtx.size() > 2)
                    {
                        board.rollback();
                        --turn_num;
                    }
                    if (!beat_series)
                        --turn_num;

                    board.rollback();
                    --turn_num;
                    beat_series = 0;
                }
            }
            else
                bot_turn(turn_num % 2); // Ход бота
        }

        // Запись в лог общего времени игры
        auto end = chrono::steady_clock::now();
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Game time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();

        // Либо играть заново либо выйти из игры
        if (is_replay)
            return play();
        if (is_quit)
            return 0;
        
        // Определение победителя
        int res = 2; // черные
        if (turn_num == Max_turns)
        {
            res = 0; // ничья
        }
        else if (turn_num % 2)
        {
            res = 1; // белые
        }
        board.show_final(res);

        // Либо играть заново либо выйти из игры
        auto resp = hand.wait();
        if (resp == Response::REPLAY)
        {
            is_replay = true;
            return play();
        }
        return res;
    }

  private:
    // Ход бота
    void bot_turn(const bool color)
    {
        auto start = chrono::steady_clock::now(); // текущее время 

        // Чтение времени задержки хода из настроек.
        // Задержка хода используется, чтобы бот не 
        // делал ходы слишком быстро, а имитировал 
        // "обдумывание". Кроме того, без задержки человеку 
        // сложно понять, что сделал бот, если был ход,
        // состоящий из многих взятий.
        auto delay_ms = config("Bot", "BotDelayMS");
        
        // Запуск задержки в отдельном потоке.
        // Основной поток в это время выполняет поиск лучшего хода.
        thread th(SDL_Delay, delay_ms);
        // Определение лучшего хода для бота.
        // Один ход может состоять из серии взятий 
        auto turns = logic.find_best_turns(color);
        th.join(); // ожидание завершения потока задержки

        bool is_first = true;
        // Поочередное выполнение каждой фазы хода.
        // Один ход может состоять из серии взятий.
        for (auto turn : turns)
        {
            if (!is_first)
            {
                // Делать задержку перед каждой фазой хода, кроме
                // первой, для которой задержка уже была реализована
                SDL_Delay(delay_ms);
            }
            is_first = false;
            // увеличение счетчика взятых фигур
            beat_series += (turn.xb != -1);
            // перемещение фигуры
            board.move_piece(turn, beat_series);
        }

        // Запись в лог общего времени хода бота
        auto end = chrono::steady_clock::now();
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Bot turn time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();
    }

    // Ход человека
    Response player_turn(const bool color)
    {
        // return 1 if quit

        // фигуры, которые могут ходить
        vector<pair<POS_T, POS_T>> cells;
        for (auto turn : logic.turns)
        {
            cells.emplace_back(turn.x, turn.y);
        }

        // Подстетка фигур, которые могут ходить
        board.highlight_cells(cells);

        move_pos pos = {-1, -1, -1, -1}; // ход
        POS_T x = -1, y = -1; // активная фигура

        // пробуем сделать первый ход
        while (true)
        {
            // Ожидаем отклика от UI
            auto resp = hand.get_cell();
            
            // Если отклик был не нажатие на клетку,
            // то вернуть его
            if (get<0>(resp) != Response::CELL)
                return get<0>(resp);
            
            // Нажатая клетка
            pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)};

            // Определение результата нажатия на клетку
            bool is_correct = false;
            for (auto turn : logic.turns) // для каждого хода из допустимых
            {
                if (turn.x == cell.first && turn.y == cell.second)
                {
                    // выбрана допустимая фигура - выбор корректный
                    is_correct = true;
                    break;
                }

                if (turn == move_pos{x, y, cell.first, cell.second})
                {
                    // выбран допустимый ход - выбор корректный
                    pos = turn;
                    break;
                }
            }

            if (pos.x != -1)
                break; // выбран допустимый ход - выбор корректный

            if (!is_correct)
            {
                // если выбор некорректный
                if (x != -1)
                {
                    // если была активная фигура, то деактивировать ее
                    // и перерисовать доску
                    board.clear_active();
                    board.clear_highlight();
                    board.highlight_cells(cells);
                }
                x = -1;
                y = -1;
                continue; // продолжаем пробовать сделать первый ход
            }

            // активация фигуры
            x = cell.first;
            y = cell.second;
            board.clear_highlight();
            board.set_active(x, y);

            // Подсветить допустимые ходы для активной фигуры
            vector<pair<POS_T, POS_T>> cells2;
            for (auto turn : logic.turns)
            {
                if (turn.x == x && turn.y == y)
                {
                    cells2.emplace_back(turn.x2, turn.y2);
                }
            }
            board.highlight_cells(cells2);
        }

        // Очистить подсветку
        board.clear_highlight();
        board.clear_active();
        // Передвинуть фигуру
        board.move_piece(pos, pos.xb != -1);
        // Если не было взятия, то закончить
        if (pos.xb == -1)
            return Response::OK;
        
        // иначе продолжать делать взятия пока возможно
        beat_series = 1; // счетчик взятий
        while (true)
        {
            // найти дальнейшие допустимые ходы
            logic.find_turns(pos.x2, pos.y2);
            // если среди них нет взятия, то закончить
            if (!logic.have_beats)
                break;

            // Подсветить допустимые ходы и активную фигуру
            vector<pair<POS_T, POS_T>> cells;
            for (auto turn : logic.turns)
            {
                cells.emplace_back(turn.x2, turn.y2);
            }
            board.highlight_cells(cells);
            board.set_active(pos.x2, pos.y2);

            // пытаемя сделать ход
            while (true)
            {                
                // Ожидаем отклика от UI
                auto resp = hand.get_cell();
                // Если отклик был не нажатие на клетку,
                // то вернуть его
                if (get<0>(resp) != Response::CELL)
                    return get<0>(resp);

                // Нажатая клетка
                pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)};

                // Проверяем корректность выбора
                bool is_correct = false;
                for (auto turn : logic.turns) // для каждого хода из допустимых
                {
                    if (turn.x2 == cell.first && turn.y2 == cell.second)
                    {
                        // выбран допустимый ход - выбор корректный
                        is_correct = true;
                        pos = turn;
                        break;
                    }
                }
                if (!is_correct)
                    continue; // если выбор некорректный, то повторить все заново

                // Если выбор корректный (взята очередная фигура), то
                // очистить подсветку
                board.clear_highlight();
                board.clear_active();
                // увеличить счетчик взятий
                beat_series += 1;
                // передвинуть фигуру
                board.move_piece(pos, beat_series);
                break;
            }
        }

        // Игрок завершил ход
        return Response::OK;
    }

  private:
    Config config;
    Board board;
    Hand hand;
    Logic logic;
    int beat_series = 0;
    bool is_replay = false;
};
