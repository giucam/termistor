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

#ifndef VTE_H
#define VTE_H

#include <QObject>
#include <libtsm.h>

class QSocketNotifier;

class Screen;

class VTE : public QObject
{
    Q_OBJECT
public:
    explicit VTE(Screen *screen);
    ~VTE();

    void write(const QChar &ch);
    void resize(int rows, int cols);
    void paste(const QByteArray &data);

    inline tsm_vte *vte() const { return m_vte; }
    inline tsm_screen *screen() const { return m_screen; }

public:
    void keyPress(int key, Qt::KeyboardModifiers mods, const QString &string);

private slots:
    void onSocketActivated(int);

private:
    void vte_event(const char *u8, size_t len);

    tsm_screen *m_screen;
    tsm_vte *m_vte;
    int m_master;
    QSocketNotifier *m_notifier;
    Screen *m_termScreen;
};

#endif // VTE_H
