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

#ifndef TERMINAL_H
#define TERMINAL_H

#include <QWindow>

#include <libtsm.h>

class Screen;
class VTE;

class Terminal: public QWindow
{
    Q_OBJECT
public:
    explicit Terminal(QWindow *parent = 0);

    void init(VTE *vte, tsm_screen *screen);
    void render();
    void renderNow();
    void update();

    inline Screen *currentScreen() const { return m_screens.at(m_currentScreen); }
    void setScreenSize(const QSize &s);

protected:
    bool event(QEvent *event) override;

    void exposeEvent(QExposeEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    void addScreen();
    void delScreen();
    void setScreen(int i);
    QRect addScreenRect() const;
    QRect delScreenRect() const;
    QRect tabRect(int i) const;
    QRect quitRect() const;
    void paste();
    void moveScreen(int screen, int d);

    QList<Screen *> m_screens;
    int m_currentScreen;
    bool m_updatePending;
    QMargins m_borders;
    bool m_bordersDirty;
    QBackingStore *m_backingStore;
    bool m_hasFocus;
};

class Debugger
{
public:
    static void print(const char *msg);
    static void printCache(int num, int size);

private:
    static bool printedCache;
    static int cacheNum;
    static int cacheSize;
};

#endif // TERMINAL_H
