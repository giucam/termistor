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

#include <QGuiApplication>
#include <QBackingStore>
#include <QPainter>
#include <QDebug>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QClipboard>
#include <QMimeData>

#include "terminal.h"
#include "vte.h"
#include "screen.h"

static const QStringList ACCEPTED_MIMETYPES = { "text/plain;charset=utf-8", "text/plain" };

Terminal::Terminal(QWindow *parent)
        : QWindow(parent)
        , m_updatePending(false)
        , m_borders(2, 0, 2, 20)
        , m_bordersDirty(true)
        , m_backingStore(nullptr)
{
    setSurfaceType(QWindow::RasterSurface);

    m_currentScreen = 0;
    addScreen();
}

void Terminal::render()
{
    QPainter painter(m_backingStore->paintDevice());
    painter.setCompositionMode(QPainter::CompositionMode_Source);

    if (m_bordersDirty) {
        m_bordersDirty = false;

        QFont font;
        font.setBold(true);
        painter.setFont(font);

        const QRect &geom = geometry();
        painter.fillRect(QRect(QPoint(0, 0), QPoint(m_borders.left(), geom.bottom())), Qt::white);
        painter.fillRect(QRect(QPoint(geom.right() - m_borders.right(), 0), geom.bottomRight()), Qt::white);
        painter.fillRect(QRect(QPoint(0, geom.bottom() - m_borders.bottom()), geom.bottomRight()), Qt::white);
        painter.drawLine(geom.topLeft(), geom.bottomLeft());
        painter.drawLine(geom.topRight(), geom.bottomRight());
        painter.drawLine(geom.bottomLeft(), geom.bottomRight());

        painter.setBrush(Qt::NoBrush);
        painter.setPen(Qt::black);
        const QRect &ar = addScreenRect();
        painter.drawRect(ar);
        painter.drawText(ar,  Qt::AlignCenter, "+");
        const QRect &dr = delScreenRect();
        painter.drawRect(dr);
        painter.drawText(dr,  Qt::AlignCenter, "-");
        const QRect &qr = quitRect();
        painter.drawRect(qr);
        painter.drawText(qr,  Qt::AlignCenter, "x");

        for (int i = 0; i < m_screens.size(); ++i) {
            const QRect &r = tabRect(i);
            painter.drawRect(r);
            font.setBold(m_currentScreen == i);
            painter.setFont(font);
            painter.drawText(r,  Qt::AlignCenter, m_screens.at(i)->name());
        }
    }

    painter.setBrush(Qt::SolidPattern);
    painter.translate(m_borders.left(), m_borders.top());
    currentScreen()->render(&painter);
}

bool Terminal::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::UpdateRequest:
        renderNow();
        return true;
    case QEvent::KeyPress: {
        QKeyEvent *ev = static_cast<QKeyEvent *>(event);
        if (ev->modifiers() == Qt::ShiftModifier) {
            if (ev->key() == Qt::Key_Left) {
                int s = m_currentScreen - 1;
                if (s < 0) s = m_screens.size() - 1;
                setScreen(s);
                break;
            } else if (ev->key() == Qt::Key_Right) {
                int s = m_currentScreen + 1;
                if (s >= m_screens.size()) s = 0;
                setScreen(s);
                break;
            } else if (ev->key() == Qt::Key_Up) {
                addScreen();
                break;
            } else if (ev->key() == Qt::Key_Insert) {
                paste();
                break;
            }
        } else if (ev->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
            if (ev->key() == Qt::Key_Left) {
                moveScreen(m_currentScreen, -1);
                break;
            } else if (ev->key() == Qt::Key_Right) {
                moveScreen(m_currentScreen, +1);
                break;
            } else if (ev->key() == Qt::Key_C) {
                QByteArray data = currentScreen()->copy();
                QMimeData *mime = new QMimeData;
                for (auto &t: ACCEPTED_MIMETYPES) {
                    mime->setData(t, data);
                }
                QGuiApplication::clipboard()->setMimeData(mime);
                break;
            }
        }
        currentScreen()->keyPressEvent(ev);
    } break;
    case QEvent::Wheel:
        currentScreen()->wheelEvent(static_cast<QWheelEvent *>(event));
        break;
    case QEvent::MouseButtonPress: {
        QMouseEvent *ev = static_cast<QMouseEvent *>(event);
        const QPoint &p = ev->windowPos().toPoint();
        for (int i = 0; i < m_screens.size(); ++i) {
            if (tabRect(i).contains(p)) {
                setScreen(i);
                break;
            }
        }
        if (addScreenRect().contains(p)) {
            addScreen();
        } else if (delScreenRect().contains(p)) {
            delScreen();
        } else if (quitRect().contains(p)) {
            close();
        } else {
            if (ev->button() == Qt::MiddleButton) {
                paste();
                break;
            } else if (ev->button() == Qt::LeftButton) {
                currentScreen()->mousePressEvent(ev);
            }
        }
    } break;
    case QEvent::MouseMove: {
        QMouseEvent *ev = static_cast<QMouseEvent *>(event);
        if (ev->buttons() == Qt::LeftButton) {
            currentScreen()->mouseMoveEvent(ev);
        } else {
            QRect geom = geometry().marginsRemoved(m_borders);
            if (geom.contains(ev->pos())) {
                setCursor(QCursor(Qt::IBeamCursor));
            } else {
                setCursor(QCursor(Qt::ArrowCursor));
            }
        }
    } break;
    default:
        break;
    }

    return QWindow::event(event);
}

void Terminal::exposeEvent(QExposeEvent *event)
{
    Q_UNUSED(event);
    renderNow();
}

void Terminal::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);

    if (m_backingStore) {
        m_backingStore->resize(size());
    }

    currentScreen()->resize(size() - QSize(m_borders.left() + m_borders.right(), m_borders.top() + m_borders.bottom()));
    m_bordersDirty = true;
    renderNow();
}

void Terminal::focusInEvent(QFocusEvent *event)
{
    currentScreen()->focusIn();
    m_hasFocus = true;
}

void Terminal::focusOutEvent(QFocusEvent *event)
{
    currentScreen()->focusOut();
    m_hasFocus = false;
}

void Terminal::setScreenSize(const QSize &s)
{
    resize(s + QSize(m_borders.left() + m_borders.right(), m_borders.top() + m_borders.bottom()));
}

void Terminal::update()
{
    if (!m_updatePending) {
        m_updatePending = true;
        QCoreApplication::postEvent(this, new QEvent(QEvent::UpdateRequest));
    }
}

void Terminal::renderNow()
{
    if (!isExposed())
        return;

    m_updatePending = false;

    if (!m_backingStore) {
        m_backingStore = new QBackingStore(this);
        m_backingStore->resize(size());
    }

    m_backingStore->beginPaint(geometry());

    render();

    m_backingStore->endPaint();
    m_backingStore->flush(QRect(0, 0, width(),height()), this);
}

void Terminal::addScreen()
{
    static int i = 1;
    m_screens << new Screen(this, QString("Shell %1").arg(i++));
    setScreen(m_screens.size() - 1);
}

void Terminal::delScreen()
{
    if (m_screens.size() == 1) {
        return;
    }

    Screen *screen = currentScreen();
    m_screens.removeOne(screen);
    delete screen;

    int s = m_currentScreen - 1;
    if (s < 0) s = m_screens.size() - 1;
    setScreen(s);
}

void Terminal::setScreen(int i)
{
    currentScreen()->focusOut();
    m_currentScreen = i;
    currentScreen()->forceRedraw();
    currentScreen()->resize(size() - QSize(m_borders.left() + m_borders.right(), m_borders.top() + m_borders.bottom()));
    m_bordersDirty = true;
    m_hasFocus ? currentScreen()->focusIn() : currentScreen()->focusOut();
    update();
}

static const int buttonsHeight = 16;

QRect Terminal::addScreenRect() const
{
    return QRect(5, geometry().bottom() - m_borders.bottom() + (m_borders.bottom() - buttonsHeight) / 2, buttonsHeight, buttonsHeight);
}

QRect Terminal::delScreenRect() const
{
    return QRect(26, geometry().bottom() - m_borders.bottom() + (m_borders.bottom() - buttonsHeight) / 2, buttonsHeight, buttonsHeight);
}

QRect Terminal::tabRect(int i) const
{
    const int start = 54;
    const int margin = 5;
    float bwidth = 70.f;
    int maxwidth = width() - start - buttonsHeight - 5;
    if ((bwidth + margin) * m_screens.size() > maxwidth) {
        bwidth = (maxwidth - margin * m_screens.size()) / (float)m_screens.size();
    }
    return QRect(start + (bwidth + margin) * i, geometry().bottom() - m_borders.bottom() + (m_borders.bottom() - buttonsHeight) / 2, bwidth, buttonsHeight);
}

QRect Terminal::quitRect() const
{
    return QRect(width() - buttonsHeight - 5, geometry().bottom() - m_borders.bottom() + (m_borders.bottom() - buttonsHeight) / 2, buttonsHeight, buttonsHeight);
}

void Terminal::paste()
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    const QMimeData *data = clipboard->mimeData();
    if (!data) {
        return;
    }
    QStringList availableFormats = data->formats();
    for (auto &mime: ACCEPTED_MIMETYPES) {
        if (availableFormats.contains(mime)) {
            currentScreen()->paste(data->data(mime));
            break;
        }
    }
}

void Terminal::moveScreen(int cur, int d)
{
    int index = cur + d;
    int size = m_screens.size();
    while (index < 0) index += size;
    while (index >= size) index -= size;

    Screen *screen = m_screens.takeAt(cur);
    m_screens.insert(index, screen);
    m_currentScreen = index;

    m_bordersDirty = true;
    update();
}

// trick to put a newline at app close
static struct A { ~A() { fprintf(stderr, "\n"); } } a;

bool Debugger::printedCache = false;
int Debugger::cacheNum = 0;
int Debugger::cacheSize = 0;

void Debugger::print(const char *msg)
{
    fprintf(stderr, "\033[2K%s\n",msg);
    printedCache = false;
    printCache(cacheNum, cacheSize);
}

void Debugger::printCache(int num, int size)
{
    fprintf(stderr, "\033[2KCache: %d images taking approximately %gkB\r", num, size / 1000.f);
    printedCache = true;
    cacheNum = num;
    cacheSize = size;
}
