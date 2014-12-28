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
#include <QScreen>
#include <QDebug>
#include <QDir>
#include <qpa/qplatformnativeinterface.h>

#include <wayland-client.h>

#include "vte.h"
#include "terminal.h"
#include "wayland-dropdown-client-protocol.h"

class Term
{
public:
    Term(bool window)
        : m_display(nullptr)
        , m_registry(nullptr)
        , m_dropdown(nullptr)
    {
        if (QGuiApplication::platformName().contains("wayland")) {
            QPlatformNativeInterface *native = QGuiApplication::platformNativeInterface();
            m_display = static_cast<wl_display *>(native->nativeResourceForIntegration("display"));
            m_registry = wl_display_get_registry(m_display);

            if (!window) {
                wl_registry_add_listener(m_registry, &s_registryListener, this);
                wl_display_roundtrip(m_display);
                if (!m_dropdown) {
                    window = true;
                }
            }
        } else {
            window = true;
        }

        m_term = new Terminal;
        m_term->setTitle("Termistor");
        if (!window) {
            m_term->setFlags(Qt::BypassWindowManagerHint);
        }

        QSurfaceFormat format;
        format.setSamples(4);
        format.setAlphaBufferSize(8);
        m_term->setFormat(format);
        QScreen *screen = m_term->screen();
        if (!window) {
            m_term->resize(screen->size().width() * 0.9, screen->size().height() * 0.5);
        } else {
            m_term->resize(500, 400);
        }
        m_term->show();

        if (!window) {
            wl_surface *wlSurface = static_cast<wl_surface *>(QGuiApplication::platformNativeInterface()->nativeResourceForWindow("surface", m_term));
            orbital_dropdown_surface *surface = orbital_dropdown_get_dropdown_surface(m_dropdown, wlSurface);

            static const orbital_dropdown_surface_listener listener = {
                [](void *d, orbital_dropdown_surface *surface, int w, int h) {
                    Term *term = static_cast<Term *>(d);
                    term->m_term->resize(w * 0.9, h * 0.5);
                    term->m_term->update();
                }
            };
            orbital_dropdown_surface_add_listener(surface, &listener, this);
        }
    }

    wl_display *m_display;
    wl_registry *m_registry;
    orbital_dropdown *m_dropdown;
    orbital_dropdown_surface *m_surface;
    Terminal *m_term;
    static const wl_registry_listener s_registryListener;
};

const wl_registry_listener Term::s_registryListener = {
    [](void *data, wl_registry *registry, uint32_t id, const char *interface, uint32_t version) {
        Term *t = static_cast<Term *>(data);

        if (strcmp(interface, "orbital_dropdown") == 0) {
            t->m_dropdown = static_cast<orbital_dropdown *>(wl_registry_bind(registry, id, &orbital_dropdown_interface, 1));
        }
    },
    [](void *, wl_registry *registry, uint32_t id) {}
};

void usage()
{
    printf("Usage: termistor [-w]\n\n");
    printf("  -w    run in a normal window\n");
    printf("  -h    show this help\n");
}

int main(int argc, char *argv[])
{
    QDir::setCurrent(QDir::homePath());
    QGuiApplication app(argc, argv);

    bool window = false;
    for (int i = 1; i < app.arguments().count(); ++i) {
        QString arg = app.arguments().at(i);
        if (arg == "-w") {
            window = true;
        } else if (arg == "-h") {
            usage();
            return 0;
        } else {
            printf("Invalid option \"%s\"\n", qPrintable(arg));
            usage();
            return 1;
        }
    }

    Term t(window);
    return app.exec();
}
