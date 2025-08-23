// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPLATFORMINTEGRATION_WEB_H
#define QPLATFORMINTEGRATION_WEB_H

#include <qpa/qplatformbackingstore.h>
#include <qpa/qplatformintegration.h>
#include <qpa/qplatformscreen.h>
#include <qpa/qplatformwindow.h>
#include <qscopedpointer.h>
#include <QtGui/QImage>
#include <QtHttpServer/QHttpServer>
#include <QtHttpServer/QHttpServerResponse>
#include <QtFbSupport/private/qfbscreen_p.h>

QT_BEGIN_NAMESPACE

QT_FORWARD_DECLARE_CLASS(QWebSocket)

class QWebServer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qsizetype clients READ clients NOTIFY clientsChanged)
public:
    QWebServer(quint16 port = 9090);
    ~QWebServer();

    qsizetype clients() const;

signals:
    void clientsChanged(qsizetype clientsRemaining);
    void geometryChanged(quint16 width, quint16 height, qreal physicalWidth, qreal physicalHeight);

public slots:
    void sendFramebuffer(QImage screen, QRegion touched);

protected:
    // Remember to keep in sync with main.js!
    enum Commands : quint8 {
        INIT = 0x01,
        FRAMEBUFFER = 0x02,
    };

private slots:
    void init();
    void processMessage(QByteArray message);
    void clientDisconnected();
    void onWebSocketConnection();

private:
    quint16 m_port;
    QList<QWebSocket *> m_wsClients;
    QHttpServer m_httpServer;
};

class QWebScreen : public QFbScreen
{
    Q_OBJECT
public:
    QWebScreen();
    ~QWebScreen();

    bool initialize() override;
    QRegion doRedraw() override;

signals:
    void redraw(QImage screen, QRegion touched);

public slots:
    void togglePower(qsizetype openConnections);
    void handleResize(const quint16 width, const quint16 height, const qreal physicalWidth, const qreal physicalHeight);
};

class QWebIntegration : public QPlatformIntegration
{
public:
    explicit QWebIntegration(const QStringList &parameters);
    ~QWebIntegration();

    void initialize() override;
    bool hasCapability(QPlatformIntegration::Capability cap) const override;
    QPlatformFontDatabase *fontDatabase() const override;
    QPlatformServices *services() const override;
    QPlatformWindow *createPlatformWindow(QWindow *window) const override;
    QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const override;
    QAbstractEventDispatcher *createEventDispatcher() const override;
    QPlatformNativeInterface *nativeInterface() const override;
    QPlatformInputContext *inputContext() const override { return m_inputContext; }

private:
    mutable QPlatformFontDatabase *m_fontDatabase;
    mutable QScopedPointer<QPlatformNativeInterface> m_nativeInterface;
    mutable QScopedPointer<QPlatformServices> m_services;
    mutable QWebServer* m_server;
    QWebScreen *m_primaryScreen;
    QPlatformInputContext *m_inputContext;
};

QT_END_NAMESPACE

#endif