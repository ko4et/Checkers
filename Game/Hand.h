#pragma once
#include <tuple>

#include "../Models/Move.h"
#include "../Models/Response.h"
#include "Board.h"

// methods for hands
class Hand
{
  public:
    Hand(Board *board) : board(board)
    {
    }

    // Ожидание отклика в ходе партии.
    // Возвращает отклик игрока-человеа в виде кортежа,
    // где первый элемент это отклик (Response), а два следующих
    // содержат координаты клетки для Response::CELL
    tuple<Response, POS_T, POS_T> get_cell() const
    {
        SDL_Event windowEvent;
        Response resp = Response::OK;
        int x = -1, y = -1;
        int xc = -1, yc = -1;
        while (true)
        {
            // Опрос событий окна
            if (SDL_PollEvent(&windowEvent))
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT:
                    // Человек закрыл окно игры
                    resp = Response::QUIT;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    // Человек нажал кнопку мыши

                    // Координаты в пространстве окна
                    x = windowEvent.motion.x;
                    y = windowEvent.motion.y;

                    // Координаты в пространстве доски
                    xc = int(y / (board->H / 10) - 1);
                    yc = int(x / (board->W / 10) - 1);

                    if (xc == -1 && yc == -1 && board->history_mtx.size() > 1)
                    {
                        // Человек нажал кнопку "BACK"
                        resp = Response::BACK;
                    }
                    else if (xc == -1 && yc == 8)
                    {
                        // Человек нажал кнопку "REPLAY"
                        resp = Response::REPLAY;
                    }
                    else if (xc >= 0 && xc < 8 && yc >= 0 && yc < 8)
                    {
                        // Человек нажал клетку на доске
                        resp = Response::CELL;
                    }
                    else
                    {
                        xc = -1;
                        yc = -1;
                    }
                    break;
                case SDL_WINDOWEVENT:
                    // Если изменен размер окна
                    if (windowEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                    {
                        // перерисовать доску
                        board->reset_window_size();
                        break;
                    }
                }
                if (resp != Response::OK)
                    break;
            }
        }
        return {resp, xc, yc};
    }

    // Ожидание отклика после завершения партии,
    // аналогично предыдущей функции, но выбор клетки не является 
    // валидным откликом здесь.
    Response wait() const
    {
        SDL_Event windowEvent;
        Response resp = Response::OK;
        while (true)
        {
            if (SDL_PollEvent(&windowEvent))
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT:
                    resp = Response::QUIT;
                    break;
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    board->reset_window_size();
                    break;
                case SDL_MOUSEBUTTONDOWN: {
                    int x = windowEvent.motion.x;
                    int y = windowEvent.motion.y;
                    int xc = int(y / (board->H / 10) - 1);
                    int yc = int(x / (board->W / 10) - 1);
                    if (xc == -1 && yc == 8)
                        resp = Response::REPLAY;
                }
                break;
                }
                if (resp != Response::OK)
                    break;
            }
        }
        return resp;
    }

  private:
    Board *board;
};
