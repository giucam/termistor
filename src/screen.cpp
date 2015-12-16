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

#include <assert.h>

#include <QColor>
#include <QFontMetrics>
#include <QPainter>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QLinkedList>
#include <QDebug>

#include "screen.h"
#include "vte.h"
#include "terminal.h"

struct Cell {
    uint32_t id;
    QString str;
    QColor color;
    QColor bgColor;
    bool bold;
    bool underline;
    bool outline;
};

class Image
{
public:
    Image()
        : prev(nullptr)
        , next(nullptr)
    {
    }

    void insert(Image *pr)
    {
        remove();
        if (pr == this) {
            return;
        }
        if (pr->next) {
            pr->next->prev = this;
        }
        pr->next = this;
        prev = pr;
    }
    void remove()
    {
        if (prev) {
            prev->next = next;
        }
        if (next) {
            next->prev = prev;
        }
        prev = nullptr;
        next = nullptr;
    }

    QImage image;
    Image *prev;
    Image *next;
    QRgb color;
    QHash<QRgb, Image *> *container;
};

class Glyph {
public:
    QHash<QRgb, Image *> normalGlyphs;
    QHash<QRgb, Image *> boldGlyphs;
    QHash<QRgb, Image *> underlineGlyphs;
    QHash<QRgb, Image *> boldAndUnderlineGlyphs;
};

class Cache {
public:
    Cache()
        : numImages(0)
        , size(0)
        , firstImg(nullptr)
        , lastImg(nullptr)
    {
    }
    ~Cache()
    {
        Image *img = firstImg;
        while (img) {
            Image *prev = img->prev;
            delete img;
            img = prev;
        }
    }

    QHash<uint32_t, Glyph> glyphs;
    int numImages;
    int size;
    Image *firstImg;
    Image *lastImg;
};

static Cache s_cache;

Screen::Screen(Terminal *t, const QString &name)
      : QObject()
      , m_terminal(t)
      , m_vte(new VTE(this))
      , m_rows(0)
      , m_columns(0)
      , m_name(name)
      , m_cells(nullptr)
      , m_margins(2, 2, 2, 2)
      , m_forceRedraw(false)
      , m_hasFocus(false)
      , m_backgroundAlpha(250)
{
    m_renderdata.font = QFont("Monospace");
    m_renderdata.font.setPixelSize(12);
    QFontMetrics metrics(m_renderdata.font);
    m_renderdata.cellW = metrics.width(' ');
    m_renderdata.cellH = metrics.height();
    m_renderdata.age = 0;
}

Screen::~Screen()
{
    delete m_vte;
    delete[] m_cells;
}

void Screen::initCells()
{
    QRect cRect = geometry().marginsRemoved(m_margins);
    int columns = cRect.width() / m_renderdata.cellW;
    int rows = cRect.height() / m_renderdata.cellH;

    QSize screenSize(columns * m_renderdata.cellW + 2, rows * m_renderdata.cellH + 1);

    m_forceRedraw = true;

    if (m_columns == columns && m_rows == rows && m_cells) {
        return;
    }

    m_columns = columns;
    m_rows = rows;
    m_renderdata.age = 0;
    m_screenSize = screenSize;


    if (m_cells) {
        delete[] m_cells;
    }

    m_cells = new Cell[m_rows * m_columns];

    tsm_screen_resize(m_vte->screen(), m_columns, m_rows);
    m_vte->resize(m_rows, m_columns);
}

QString Screen::name() const
{
    return m_name;
}

int Screen::drawCell(uint32_t id, const uint32_t *ch, size_t len, uint32_t width, unsigned int posx, unsigned int posy, const tsm_screen_attr *attr, tsm_age_t age)
{
    if (age && m_renderdata.age && age <= m_renderdata.age && !m_forceRedraw) {
        return 0;
    }

    Cell &cell = m_cells[posy * m_columns + posx];
    bool outline = attr->inverse && m_cursor == &cell && !m_hasFocus;

    uint8_t fr, fg, fb, br, bg, bb;
    if (attr->inverse && !outline) {
        fr = attr->br;
        fg = attr->bg;
        fb = attr->bb;
        br = attr->fr;
        bg = attr->fg;
        bb = attr->fb;
    } else {
        fr = attr->fr;
        fg = attr->fg;
        fb = attr->fb;
        br = attr->br;
        bg = attr->bg;
        bb = attr->bb;
    }
    QColor  c(fr, fg, fb);
    QColor bgc(br, bg, bb, m_backgroundAlpha);

    if (cell.id != id || cell.color != c || cell.bgColor != bgc ||
        cell.bold != attr->bold || cell.underline != attr->underline || outline != cell.outline || m_forceRedraw) {
        cell.id = id;
        cell.color = c;
        cell.bgColor = bgc;
        cell.bold = attr->bold;
        cell.underline = attr->underline;
        cell.outline = outline;
        if (len) {
            cell.str = QString::fromUcs4(ch, len);
        }

        QRect rect(posx * m_renderdata.cellW, posy * m_renderdata.cellH, width * m_renderdata.cellW, m_renderdata.cellH);

        m_painter->setCompositionMode(QPainter::CompositionMode_Source);
        m_painter->fillRect(rect, bgc);
        if (outline) {
            m_painter->setPen(c);
            m_painter->drawRect(rect.x(), rect.y(), rect.width() - 1, rect.height() - 1);
        }
        if (len) {
            QRgb crgb = c.rgb();

            Glyph &glyph = s_cache.glyphs[id];
            QHash<QRgb, Image *> *hash = &glyph.normalGlyphs;
            if (!cell.underline && cell.bold) {
                hash = &glyph.boldGlyphs;
            } else if (cell.underline && !cell.bold) {
                hash = &glyph.underlineGlyphs;
            } else if (cell.underline && cell.bold) {
                hash = &glyph.boldAndUnderlineGlyphs;
            }
            Image *img = nullptr;
            if (!hash->contains(crgb)) {
                img = new Image;
                img->image = QImage(m_renderdata.cellW, m_renderdata.cellH, QImage::Format_ARGB6666_Premultiplied);
                img->image.fill(Qt::transparent);
                QPainter painter(&img->image);

                QFont font = m_renderdata.font;
                if (cell.bold) font.setBold(true);
                if (cell.underline) font.setUnderline(true);
                painter.setFont(font);
                painter.setPen(c);
                painter.drawText(0, 0, m_renderdata.cellW, m_renderdata.cellH, 0, cell.str);
                painter.end();

                img->container = hash;
                img->color = crgb;
                hash->insert(crgb, img);
                if (s_cache.firstImg) {
                    img->insert(s_cache.firstImg);
                }
                s_cache.firstImg = img;
                if (!s_cache.lastImg) {
                    s_cache.lastImg = img;
                }

                ++s_cache.numImages;
                s_cache.size += img->image.byteCount() + sizeof(QImage) + sizeof(Image);
            } else {
                img = hash->value(crgb);
                if (s_cache.lastImg == img) {
                    s_cache.lastImg = img->next;
                }
                if (s_cache.firstImg != img) {
                    img->insert(s_cache.firstImg);
                }
                if (!s_cache.lastImg) {
                    s_cache.lastImg = img;
                }
                s_cache.firstImg = img;
            }

            m_painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
            m_painter->drawImage(posx * m_renderdata.cellW, posy * m_renderdata.cellH, img->image);
        }
    }

    return 0;
}

void Screen::resize(const QSize &s)
{
    m_geometry.setSize(s);
    initCells();
}

void Screen::render(QPainter *painter)
{
    if (!m_cells) {
        return;
    }

    m_painter = painter;
    painter->setFont(m_renderdata.font);

    const QRect &geom = geometry();
    tsm_screen_attr attr;
    tsm_vte_get_def_attr(m_vte->vte(), &attr);

    QColor bg(attr.br, attr.bg, attr.bb, m_backgroundAlpha);
    float wm = geom.width() - m_screenSize.width();
    float hm = geom.height() - m_screenSize.height();
    painter->fillRect(QRect(QPoint(0, 0), QPoint(geom.width() - m_margins.right(), m_margins.top())), bg);
    painter->fillRect(QRect(QPoint(0, 0), QPoint(m_margins.left(), geom.bottom())), bg);
    painter->fillRect(QRect(QPoint(geom.right() - wm + m_margins.left(), 0), geom.bottomRight()), bg);
    painter->fillRect(QRect(QPoint(0, geom.bottom() - hm + m_margins.top()), geom.bottomRight()), bg);

    painter->translate(m_margins.left() + 1, m_margins.top());

    unsigned int flags = tsm_screen_get_flags(m_vte->screen());
    if (flags & TSM_SCREEN_HIDE_CURSOR) {
        m_cursor = nullptr;
    } else {
        const unsigned int x = tsm_screen_get_cursor_x(m_vte->screen());
        const unsigned int y = tsm_screen_get_cursor_y(m_vte->screen());
        m_cursor = &m_cells[y * m_columns + x];
    }

    m_renderdata.age =  tsm_screen_draw(m_vte->screen(),
                                        [](tsm_screen *screen, uint32_t id,
                                           const uint32_t *ch, size_t len,
                                           unsigned int cwidth, unsigned int posx,
                                           unsigned int posy,
                                           const tsm_screen_attr *attr,
                                           tsm_age_t age, void *data) -> int {
                                               return static_cast<Screen *>(data)->drawCell(id, ch, len, cwidth, posx, posy, attr, age); }, this);

    m_painter = nullptr;
    m_forceRedraw = false;

    painter->translate(-m_margins.left() - 1, -m_margins.top());

    while (s_cache.numImages > 1000) {
        Image *img = s_cache.lastImg;
        assert(img != 0);
        assert(img->next != 0);
        s_cache.lastImg = img->next;
        img->remove();
        img->container->remove(img->color);

        --s_cache.numImages;
        s_cache.size -= (img->image.byteCount() + sizeof(QImage) + sizeof(Image));
        delete img;
    }

    Debugger::printCache(s_cache.numImages, s_cache.size);
}

void Screen::update()
{
    if (m_terminal->currentScreen() == this) {
        m_terminal->update();
    }
}

QByteArray Screen::copy()
{
    char *out;
    int len = tsm_screen_selection_copy(m_vte->screen(), &out);
    if (len <= 0) {
        return QByteArray();
    }

    QByteArray data;
    int startline = 0;
    int lastnonspace = -1;
    for (int i = 0; i < len; ++i) {
        if (out[i] == 0) {
            out[i] = ' ';
        }

        if (out[i] == '\n' || i == len - 1) {
            if (lastnonspace == len - 1 || lastnonspace < 0) {
                startline = i + 1;
                continue;
            }

            if (i != len - 1) {
                out[lastnonspace + 1] = '\n';
            }

            data.append(out + startline, lastnonspace + 2 - startline);
            lastnonspace = i;
            startline = i + 1;
        } else if (out[i] != ' ') {
            lastnonspace = i;
        }
    }

    free(out);
    return data;
}

void Screen::paste(const QByteArray &data)
{
    m_vte->paste(data);
}

void Screen::keyPressEvent(QKeyEvent *ev)
{
    m_vte->keyPress(ev->key(), ev->modifiers(), ev->text());
    ev->accept();
}

void Screen::wheelEvent(QWheelEvent *ev)
{
    int delta = ev->angleDelta().y() / 40;
    if (delta > 0) {
        tsm_screen_sb_up(m_vte->screen(), delta);
    } else {
        tsm_screen_sb_down(m_vte->screen(), -delta);
    }
    update();
    ev->accept();
}

QPoint Screen::gridPosFromGlobal(const QPointF &pos)
{
    int col = qBound<int>(1, pos.x() / (double)m_renderdata.cellW + 0.5, m_columns + 1);
    int row = qBound<int>(0, pos.y() / (double)m_renderdata.cellH, m_rows);
    int index = row * m_columns + col - 1;
    col = index % m_columns;
    row = index / m_columns;
    return QPoint(col, row);
}

void Screen::mousePressEvent(QMouseEvent *ev)
{
    m_selectionStart = gridPosFromGlobal(ev->pos());
    tsm_screen_selection_reset(m_vte->screen());
    ev->accept();
    update();
}

void Screen::mouseMoveEvent(QMouseEvent *ev)
{
    if (m_selectionStart.x() >= 0) {
        tsm_screen_selection_start(m_vte->screen(), m_selectionStart.x(), m_selectionStart.y());
        m_selectionStart.setX(-1);
    }
    QPoint p = gridPosFromGlobal(ev->pos());
    tsm_screen_selection_target(m_vte->screen(), p.x(), p.y());
    update();
}

static inline bool isValidChar(char c)
{
    QChar cc(c);
    if (cc.isSpace())
        return false;

    switch (c) {
        case '(':
        case ')':
        case '[':
        case ']':
        case '{':
        case '}':
        case '\0':
            return false;
        default:
            break;
    }

    return true;
}

void Screen::mouseDoubleClickEvent(QMouseEvent *ev)
{
    if (ev->button() != Qt::LeftButton) {
        return;
    }

    QPoint p = gridPosFromGlobal(ev->pos());
    if (isValidChar(getCharacter(p.x(), p.y()))) {
        int left = p.x();
        int right = p.x();
        for (; left >= 0; -- left) {
            if (!isValidChar(getCharacter(left, p.y())))
                break;
        }
        for (; right < m_columns; ++right) {
            if (!isValidChar(getCharacter(right, p.y())))
                break;
        }

        tsm_screen_selection_start(m_vte->screen(), left + 1, p.y());
        tsm_screen_selection_target(m_vte->screen(), right - 1, p.y());
    }
}

void Screen::focusIn()
{
    m_hasFocus = true;
    m_forceRedraw = true;
    update();
}

void Screen::focusOut()
{
    m_hasFocus = false;
    m_forceRedraw = true;
    update();
}

void Screen::forceRedraw()
{
    m_forceRedraw = true;
}

char Screen::getCharacter(int x, int y)
{
    tsm_screen_selection_start(m_vte->screen(), x, y);
    tsm_screen_selection_target(m_vte->screen(), x, y);
    char c, *d;
    tsm_screen_selection_copy(m_vte->screen(), &d);
    c = d[0];
    free(d);
    tsm_screen_selection_reset(m_vte->screen());
    return c;
}
