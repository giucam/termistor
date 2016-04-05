
#ifndef SCREEN_H
#define SCREEN_H
/*
 * Copyright 2013 Giulio Camuffo <giuliocamuffo@gmail.com>
 *
 * This file is part of Termistor
 *
 * Termistor is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Termistor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Termistor.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QObject>
#include <QFont>
#include <QMargins>
#include <QSize>
#include <QRect>

#include <libtsm.h>

class QPainter;
class QKeyEvent;
class QWheelEvent;
class QMouseEvent;

struct tsm_screen;

class Terminal;
class VTE;
struct Cell;

class Screen : public QObject
{
    Q_OBJECT
public:
    explicit Screen(Terminal *term, const QString &name);
    ~Screen();

    void initCells();

    QString name() const;

    void close();
    void resize(const QSize &size);
    void update();
    void render(QPainter *painter);
    void forceRedraw();

    QByteArray copy();
    void paste(const QByteArray &data);
    void keyPressEvent(QKeyEvent *ev);
    void wheelEvent(QWheelEvent *ev);
    void mousePressEvent(QMouseEvent *ev);
    void mouseMoveEvent(QMouseEvent *ev);
    void mouseDoubleClickEvent(QMouseEvent *ev);
    void focusIn();
    void focusOut();

private:
    inline QRect geometry() const { return m_geometry; }
    int drawCell(uint32_t id, const uint32_t *ch, size_t len, uint32_t width, unsigned int posx, unsigned int posy, const tsm_screen_attr *attr, tsm_age_t age);
    QPoint gridPosFromGlobal(const QPointF &pos);
    char getCharacter(int x, int y);

    Terminal *m_terminal;
    VTE *m_vte;
    int m_rows;
    int m_columns;
    QString m_name;

    Cell *m_cells;
    Cell *m_cursor;

    QPainter *m_painter;
    struct {
        int cellW;
        int cellH;
        QFont font;
        tsm_age_t age;
    } m_renderdata;

    bool m_updatePending;
    QMargins m_margins;
    QSize m_screenSize;
    QRect m_geometry;
    bool m_forceRedraw;
    bool m_hasFocus;
    QPoint m_selectionStart;
    int m_backgroundAlpha;
    double m_accumDelta;
};

#endif
