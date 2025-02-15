#pragma once
#include <stdlib.h>

typedef int8_t POS_T;

// Структура, описывающая ход, возможно со взятием
struct move_pos
{
    POS_T x, y;             // начальная позиция
    POS_T x2, y2;           // конечная позиция
    POS_T xb = -1, yb = -1; // позиция побитой фигуры

    // Ход без взятия
    move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2) : x(x), y(y), x2(x2), y2(y2)
    {
    }

    // Ход со взятием
    move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2, const POS_T xb, const POS_T yb)
        : x(x), y(y), x2(x2), y2(y2), xb(xb), yb(yb)
    {
    }

    // Оператор эквивалентности (сравнивает начальные и конечные позиции)
    bool operator==(const move_pos &other) const
    {
        return (x == other.x && y == other.y && x2 == other.x2 && y2 == other.y2);
    }

    // Оператор неэквивалентности
    bool operator!=(const move_pos &other) const
    {
        return !(*this == other);
    }
};
