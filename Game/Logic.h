#pragma once
#include <random>
#include <vector>
#include <queue>

#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"

const int INF = 1e9;

typedef vector<vector<POS_T>> Matrix;

// Структура, описывающая один полный ход,
// который может быть простым ходом без взятия,
// а может быть ходом со взятием одной или
// нескольких фигур.
struct Turn
{
    // Список перемещений фигуры (может состоять из
    // нескольких элементов при взятии нескольких фигур)
    vector<move_pos> series;

    // Финальное состояние доски после завершения хода 
    Matrix final_mtx;

    // Возврвщает первое перемещение
    const move_pos& first() const
    {
        return series.front();
    }

    // Возврвщает последнее перемещение
    const move_pos& last() const
    {
        return series.back();
    }
};

class Logic
{
  public:
    Logic(Board *board, Config *config) : board(board), config(config)
    {
        rand_eng = std::default_random_engine (
            !((*config)("Bot", "NoRandom")) ? unsigned(time(0)) : 0);
        scoring_mode = (*config)("Bot", "BotScoringType");
        optimization = (*config)("Bot", "Optimization");
    }

    // Находит лучший ход для бота заданного цвета
    vector<move_pos> find_best_turns(const bool color)
    {        
        // Получение списка всех доступных ходов
        vector<Turn> res_turns = find_series(color, board->get_board());
        
        // Выбор лучшего хода используя алгоритм Минимакс
        // с альфа-бета отсечением
        Turn best_turn;
        double max_score = -1;
        for (const auto& turn : res_turns)
        {            
            // Найти оценку для каждого из возможных ходов
            double score = find_best_turns_rec(turn, !color, 0);
            if (score > max_score)
            {
                // выбрать лучший ход
                max_score = score;
                best_turn = turn;
            }            
        }

        // Вернуть список перемещений для лучшего хлда
        return best_turn.series;
    }
        

private:
    // Делает ход turn на доске mtx.
    // Возвращает новую доску, на которой сделан ход.
    vector<vector<POS_T>> make_turn(vector<vector<POS_T>> mtx, move_pos turn) const
    {
        // Если ход со взятием, то убрать побитую фигуру
        if (turn.xb != -1)
            mtx[turn.xb][turn.yb] = 0;
        
        // Если фигура дошла до противоположного края, то
        // сделать ее дамкой
        if ((mtx[turn.x][turn.y] == 1 && turn.x2 == 0) || (mtx[turn.x][turn.y] == 2 && turn.x2 == 7))
            mtx[turn.x][turn.y] += 2;

        // Поставить фигуру на конечное поле
        mtx[turn.x2][turn.y2] = mtx[turn.x][turn.y];
        // Убрать фигуру с начального поля
        mtx[turn.x][turn.y] = 0;

        return mtx;
    }

    // Оценка позиции для бота.
    // Если first_bot_color == true, то бот черного цвета (очень 
    // не очевидное название параметра).
    double calc_score(const vector<vector<POS_T>> &mtx, const bool first_bot_color) const
    {
        // color - who is max player

        // Счетчики пешек и дамок для двух цветов.
        double w = 0, wq = 0, b = 0, bq = 0;
        
        // Подсчет фигур
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                w += (mtx[i][j] == 1); // белые пешки
                wq += (mtx[i][j] == 3); // белые дамки 
                b += (mtx[i][j] == 2); // черные пешки
                bq += (mtx[i][j] == 4); // черные дамки

                if (scoring_mode == "NumberAndPotential")
                {
                    w += 0.05 * (mtx[i][j] == 1) * (7 - i);
                    b += 0.05 * (mtx[i][j] == 2) * (i);
                }
            }
        }

        // Если бот белого цвета, то поменять местами.
        if (!first_bot_color)
        {
            swap(b, w);
            swap(bq, wq);
        }

        // Если кол-во белых равно нулю, то черные выиграли, 
        // оценка максимальная.
        if (w + wq == 0)
            return INF;

        // Если кол-во черных равно нулю, то черные проиграли, 
        // оценка минимальная.
        if (b + bq == 0)
            return 0;
        
        int q_coef = 4; // усиливающий коэффициент для дамки
        if (scoring_mode == "NumberAndPotential")
        {
            q_coef = 5; // при этой настройке дамка считается сильнее
        }
        // Получить оценку в зависимости от соотношения кол-ва фигур
        return (b + bq * q_coef) / (w + wq * q_coef);
    }


    // Алгоритм Минимакс с альфа-бета отсечением
    double find_best_turns_rec(Turn turn, bool color, int depth, double alpha = -1, double beta = INF)
    {
        // Если достигнута максимальная глубина, то посчитать и вернуть оценку
        if (depth == Max_depth)
            return calc_score(turn.final_mtx, depth % 2 == color);


        // Получение списка всех доступных ходов
        vector<Turn> res_turns = find_series(color, turn.final_mtx);

        if (depth % 2)
        {
            // Максимизация для бота
            double score = -1;
            for (const auto& next_turn : res_turns)
            {
                score = max(score, find_best_turns_rec(next_turn, !color, depth + 1, alpha, beta));
                // альфа-бета отсечение
                if (score > beta)
                    break;
                alpha = max(alpha, score);
            }
            return score;
        }
        else
        {
            // Минимизация для человека
            double score = INF;
            for (const auto& next_turn : res_turns)
            {
                score = min(score, find_best_turns_rec(next_turn, !color, depth + 1, alpha, beta));
                // альфа-бета отсечение
                if (score < alpha)
                    break;
                beta = min(beta, score);
            }
            return score;
        }

        return 0;
    }

    // Функция для нахождения всех доступных ходов для игрока заданного цвета.
    // Один ход может состоять из нескольких перемещений при взятии нескольких фигур.
    vector<Turn> find_series(const bool color, vector<vector<POS_T>> mtx)
    {
        // Получение списка доступных начальных перемещений
        vector<move_pos> moves;
        find_turns(color);
        moves = turns;
        bool have_beats_now = have_beats;

        // Список полных ходов
        vector<Turn> turns_series;

        // Для каждого начального перемещения
        for (const auto& move : moves)
        {
            // Добавить начальное перемещение
            // в список перемещений полного хода
            Turn turn;
            turn.series.push_back(move);
            turn.final_mtx = make_turn(mtx, move);

            // Если доступные ходы со взятием
            if (have_beats_now)
            {
                // то нужно получить все возможные варианты взятий
                
                // Очередь неполных ходов
                queue<Turn> q;
                q.push(turn);

                while (!q.empty())
                {
                    // Извлечь неполный ход из очереди
                    Turn t = q.front(); q.pop();                    

                    // Найти для него дальнейшие возможные перемещения
                    find_turns(t.last().x2, t.last().y2, t.final_mtx);
                    if (have_beats)
                    {
                        // Эсли эти перемещения тоже со взятием
                        for (const auto& m : turns)
                        {
                            // Тогда создаем для каждого новый неполный ход и
                            // добавлем каждое перемещение к списку перемещений 
                            // этого неполного хода
                            Turn new_turn = t;
                            new_turn.series.push_back(m);
                            new_turn.final_mtx = make_turn(t.final_mtx, m);
                            q.push(new_turn);
                        }
                    }
                    else
                    {
                        // Эсли эти перемещения без взятия, то ход закончен,
                        // добавляем его в итоговый список
                        turns_series.push_back(t);
                    }
                }
            }
            else
            {
                // Эсли первые перемещения без взятия,
                // то просто добавляем ход в итоговый список
                turns_series.push_back(turn);
            }
        }

        return turns_series;
    }

public:
    // Найти допустимые ходы для игрока заданного цвета.
    // Список ходов сохраняется в поле turns.
    void find_turns(const bool color)
    {
        find_turns(color, board->get_board());
    }

    // Найти допустимые ходы для заданой фигуры.
    // Список ходов сохраняется в поле turns.
    void find_turns(const POS_T x, const POS_T y)
    {
        find_turns(x, y, board->get_board());
    }

private:
    // Найти допустимые ходы для игрока заданного цвета.
    // Список ходов сохраняется в поле turns.
    void find_turns(const bool color, const vector<vector<POS_T>> &mtx)
    {
        vector<move_pos> res_turns; // Список ходов

        // Если в ходе нахождения ходов обнаружился ход со взятием,
        // то допустимыми ходами являются только ходы со взятием.
        bool have_beats_before = false;

        // Проверка каждой клетки доски
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                // Здесь очень путаное определение цвета фигуры на клетке.
                // 0 означает нет фигуры, 1 - белая пешка, 2 - черная пешка,
                // 3 - белая дамка, 4 - черная дамка. 
                // А color == false означает белый цвет.
                if (mtx[i][j] && mtx[i][j] % 2 != color)
                {
                    // Если на клетке фигура заданного цвета, то найти возможные
                    // ходы для этой фигуры
                    find_turns(i, j, mtx);

                    // если нашелся ход со взятием, а перед этим не было ходов
                    // со взятием, то ходы, найденные ранее, становятся не валидными
                    if (have_beats && !have_beats_before)
                    {
                        have_beats_before = true;
                        res_turns.clear();
                    }

                    // Добавить найденные ходы в итоговый список
                    if ((have_beats_before && have_beats) || !have_beats_before)
                    {
                        res_turns.insert(res_turns.end(), turns.begin(), turns.end());
                    }
                }
            }
        }
        // Сохранить ходы в поле turns
        turns = res_turns;
        // Перемешать список ходов
        shuffle(turns.begin(), turns.end(), rand_eng);
        // Ходы со взятием или простые
        have_beats = have_beats_before;
    }

    // Найти допустимые ходы для заданой фигуры.
    // Список ходов сохраняется в поле turns.
    void find_turns(const POS_T x, const POS_T y, const vector<vector<POS_T>> &mtx)
    {
        // Очистить список ходов и флаг взятия
        turns.clear();
        have_beats = false;

        // Тип фигуры на клетке.
        // 0 означает нет фигуры, 1 - белая пешка, 2 - черная пешка,
        // 3 - белая дамка, 4 - черная дамка. 
        POS_T type = mtx[x][y];

        // Проверка на возможность взятия
        switch (type)
        {
        case 1:
        case 2:
            // Проверка для пешки.
            // Можно ли походить через одну клетку по диагонали,
            // и есть ли на промежуточной клетке фигура 
            // противоположного цвета
            for (POS_T i = x - 2; i <= x + 2; i += 4)
            {
                for (POS_T j = y - 2; j <= y + 2; j += 4)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7)
                        continue;
                    POS_T xb = (x + i) / 2, yb = (y + j) / 2;
                    if (mtx[i][j] || !mtx[xb][yb] || mtx[xb][yb] % 2 == type % 2)
                        continue;
                    turns.emplace_back(x, y, i, j, xb, yb);
                }
            }
            break;
        default:
            // Проверка для дамки.
            // Проверка всех возможных позиций по диагоналям,
            // и есть ли на промежуточной клетке фигура 
            // противоположного цвета
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    POS_T xb = -1, yb = -1;
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                        {
                            if (mtx[i2][j2] % 2 == type % 2 || (mtx[i2][j2] % 2 != type % 2 && xb != -1))
                            {
                                break;
                            }
                            xb = i2;
                            yb = j2;
                        }
                        if (xb != -1 && xb != i2)
                        {
                            turns.emplace_back(x, y, i2, j2, xb, yb);
                        }
                    }
                }
            }
            break;
        }

        // Если найдены ходы со взятиями, то простые ходы
        // проверять не нужно
        if (!turns.empty())
        {
            have_beats = true;
            return;
        }

        // Проверка простых ходов
        switch (type)
        {
        case 1:
        case 2:
            // Проверка для пешки.
            // Допустимый ход - вперед по диагонали на соседнюю пустую клетку
            {
                POS_T i = ((type % 2) ? x - 1 : x + 1);
                for (POS_T j = y - 1; j <= y + 1; j += 2)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7 || mtx[i][j])
                        continue;
                    turns.emplace_back(x, y, i, j);
                }
                break;
            }
        default:
            // Проверка для дамки.
            // Допустимый ход - в любую сторону по диагонали на любую пустую клетку,
            // между начальной и конечной позицией не должно быть других фигур.
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                            break;
                        turns.emplace_back(x, y, i2, j2);
                    }
                }
            }
            break;
        }
    }

  public:
    // Список ходов, найденных при вызове find_turns().
    vector<move_pos> turns;
    // Флаг, обозначающий что найденные ходы со взятием. 
    bool have_beats = false;
    // Максимальная глубина рекурсии.
    int Max_depth = 0;

  private:
    // ГПСЧ
    default_random_engine rand_eng;
    // Режим оценки силы позиции
    string scoring_mode;
    // Оптимизация алгоритма определения лучшего хода для бота
    string optimization;
    // Следующий ход для бота
    vector<move_pos> next_move;
    // Список лучших состояний
    vector<int> next_best_state;
    // Ссылка на объект игрового поля
    Board *board;
    // Ссылка на настройки
    Config *config;
};
