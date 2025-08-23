// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwebintegration.h"

#include <qpa/qplatformfontdatabase.h>
#include <qpa/qplatforminputcontextfactory_p.h>
#include <qpa/qplatformnativeinterface.h>
#include <qpa/qplatformwindow.h>
#include <qpa/qwindowsysteminterface.h>

#include <QtConcurrent/qtconcurrentrun.h>
#include <QtCore/QRegularExpression>
#include <QtFbSupport/private/qfbbackingstore_p.h>
#include <QtFbSupport/private/qfbwindow_p.h>
#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/private/qinputdevicemanager_p_p.h>
#include <QtGui/private/qpixmap_raster_p.h>
#include <QtNetwork/QTcpServer>
#include <QtWebSockets/qwebsocketserver.h>
#include <QtWebSockets/qwebsocket.h>

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
#include <QtGui/private/qdesktopunixservices_p.h>
#else
#include <QtGui/private/qgenericunixservices_p.h>
#endif

#if defined(Q_OS_WIN)
#  include <QtGui/private/qwindowsfontdatabase_p.h>
#  if QT_CONFIG(freetype)
#    include <QtGui/private/qwindowsfontdatabase_ft_p.h>
#  endif
#elif defined(Q_OS_DARWIN)
#  include <QtGui/private/qcoretextfontdatabase_p.h>
#endif

#if QT_CONFIG(fontconfig)
#  include <QtGui/private/qgenericunixfontdatabase_p.h>
#endif

#if QT_CONFIG(freetype)
#include <QtGui/private/qfontengine_ft_p.h>
#include <QtGui/private/qfreetypefontdatabase_p.h>
#endif

#if !defined(Q_OS_WIN)
#include <QtGui/private/qgenericunixeventdispatcher_p.h>
#else
#include <QtCore/private/qeventdispatcher_win_p.h>
#endif

QT_BEGIN_NAMESPACE

using namespace Qt::StringLiterals;

class QCoreTextFontEngine;

QWebScreen::QWebScreen()
{
    initialize();
}

QWebScreen::~QWebScreen()
{
}

bool QWebScreen::initialize()
{
    mGeometry = QRect(0, 0, 1024, 768);
    mFormat = QImage::Format_ARGB32_Premultiplied;
    mDepth = 32;
    mPhysicalSize = QSizeF(mGeometry.width()/96.*25.4, mGeometry.height()/96.*25.4);

    QFbScreen::initializeCompositor();

    setPowerState(PowerStateOff);

    return true;
}

QRegion QWebScreen::doRedraw()
{
    QRegion touched = QFbScreen::doRedraw();

    if (touched.isEmpty()) {
        return touched;
    }

    emit redraw(mScreenImage, touched);

    return touched;
}

void QWebScreen::togglePower(qsizetype openConnections)
{
    switch (openConnections) {
        case 0:
            setPowerState(PowerStateOff);
            break;
        case 1:
            setPowerState(PowerStateOn);
            emit redraw(mScreenImage, QRegion(mScreenImage.rect()));
            break;
        default:
            emit redraw(mScreenImage, QRegion(mScreenImage.rect()));
            break;
    }
}

void QWebScreen::handleResize(const quint16 width, const quint16 height, const qreal physicalWidth, const qreal physicalHeight)
{
    setGeometry(QRect(0, 0, width, height));
    setPhysicalSize(QSizeF(physicalWidth, physicalHeight).toSize());
}

QWebServer::QWebServer(quint16 port)
    : m_port(port)
{
    QMetaObject::invokeMethod(this, "init", Qt::QueuedConnection);
}

QWebServer::~QWebServer()
{
    qDeleteAll(m_wsClients.begin(), m_wsClients.end());
}

qsizetype QWebServer::clients() const
{
    return m_wsClients.size();
}

void QWebServer::init()
{
    m_httpServer.route("/", []() {
        return QtConcurrent::run([] () {
            return QHttpServerResponse::fromFile(QStringLiteral(":/www/index.html"));
        });
    });
    m_httpServer.route("/favicon.ico", []() {
        return QtConcurrent::run([] () {
            return QHttpServerResponse::fromFile(QStringLiteral(":/www/favicon.ico"));
        });
    });
    m_httpServer.route("/main.js", []() {
        return QtConcurrent::run([] () {
            return QHttpServerResponse::fromFile(QStringLiteral(":/www/main.js"));
        });
    });
    m_httpServer.route("/ws", []() {
        return QFuture<QHttpServerResponse>();
    });
    m_httpServer.addWebSocketUpgradeVerifier(&m_httpServer, []() {
        return QHttpServerWebSocketUpgradeResponse::accept();
    });
    connect(&m_httpServer, &QAbstractHttpServer::newWebSocketConnection, this, &QWebServer::onWebSocketConnection);

    auto tcpserver = std::make_unique<QTcpServer>();
    if (!tcpserver->listen(QHostAddress::Any, m_port) || !m_httpServer.bind(tcpserver.get())) {
        qWarning() << "QWebServer could not open HTTP server on port" << m_port;
    } else {
        qDebug() << "HTTP server listening on" << QString("http://127.0.0.1:%1/").arg(tcpserver->serverPort());
    }
    tcpserver.release();
}

void QWebServer::onWebSocketConnection()
{
    while (m_httpServer.hasPendingWebSocketConnections()) {
        auto ptr = m_httpServer.nextPendingWebSocketConnection();
        QWebSocket *socket = ptr.get();
        ptr.release();

        connect(socket, &QWebSocket::binaryMessageReceived, this, &QWebServer::processMessage);
        connect(socket, &QWebSocket::disconnected, this, &QWebServer::clientDisconnected);
        m_wsClients << socket;

        emit clientsChanged(m_wsClients.size());
    }
}

void QWebServer::processMessage(QByteArray message)
{
    QWebSocket *socket = qobject_cast<QWebSocket *>(sender());
    Q_UNUSED(socket);

    quint8 command;
    QDataStream stream(&message, QIODevice::ReadOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream >> command;

    switch (command) {
        case Commands::INIT: {
            quint16 width;
            quint16 height;
            qreal physicalWidth;
            qreal physicalHeight;
            stream >> width >> height >> physicalWidth >> physicalHeight;
            emit geometryChanged(width, height, physicalWidth, physicalHeight);
            return;
        }
        case Commands::FRAMEBUFFER:
            qWarning() << "FRAMEBUFFER command should never be sent from client";
            return;
        default:
            qWarning() << "Unrecognized command:" << command;
            return;
    }
}

void QWebServer::clientDisconnected()
{
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    if (client) {
        m_wsClients.removeAll(client);
        client->deleteLater();
        emit clientsChanged(m_wsClients.size());
    }
}

void QWebServer::sendFramebuffer(QImage screen, QRegion touched)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << Commands::FRAMEBUFFER << quint8(screen.format()) << quint16(touched.rectCount());

    for (const QRect &rect : touched) {
        QRect clipped = rect.intersected(QRect(QPoint(0,0), screen.size()));

        stream << quint16(clipped.x());
        stream << quint16(clipped.y());
        stream << quint16(clipped.width());
        stream << quint16(clipped.height());

        QByteArray tileBytes;
        tileBytes.resize(clipped.width() * clipped.height() * 4);
        for (int y = 0; y < clipped.height(); ++y) {
            const uchar *src = screen.constScanLine(clipped.y() + y) + clipped.x() * 4;
            uchar *dst = reinterpret_cast<uchar *>(tileBytes.data() + y * clipped.width() * 4);
            memcpy(dst, src, clipped.width() * 4);
        }

        stream << quint32(tileBytes.size());
        data.append(tileBytes);
    }

    for (qsizetype i = 0; i < m_wsClients.size(); ++i) {
        m_wsClients.at(i)->sendBinaryMessage(data);
    }
}

QWebIntegration::QWebIntegration(const QStringList &parameters)
{
    QRegularExpression portRx("port=(\\d+)"_L1);
    quint16 port = 9090;
    for (const QString &arg : parameters) {
        QRegularExpressionMatch match;
        if (arg.contains(portRx, &match)) {
            port = match.captured(1).toInt();
        }
    }

    m_server = new QWebServer(port);
    m_primaryScreen = new QWebScreen();

    // Order is important.
    QObject::connect(m_primaryScreen, &QWebScreen::redraw, m_server, &QWebServer::sendFramebuffer);
    QObject::connect(m_server, &QWebServer::geometryChanged, m_primaryScreen, &QWebScreen::handleResize);
    QObject::connect(m_server, &QWebServer::clientsChanged, m_primaryScreen, &QWebScreen::togglePower);
}

QWebIntegration::~QWebIntegration()
{
    QWindowSystemInterface::handleScreenRemoved(m_primaryScreen);
}

void QWebIntegration::initialize()
{
    if (m_primaryScreen->initialize()) {
        QWindowSystemInterface::handleScreenAdded(m_primaryScreen);
    } else {
        qWarning("Failed to initialize screen");
    }

    m_inputContext = QPlatformInputContextFactory::create();

    m_nativeInterface.reset(new QPlatformNativeInterface);

#if defined(Q_OS_WIN)
#  if QT_CONFIG(freetype)
    m_fontDatabase = new QWindowsFontDatabaseFT;
#  else
    m_fontDatabase = new QWindowsFontDatabase;
#  endif
#elif defined(Q_OS_DARWIN)
#  if QT_CONFIG(freetype)
    m_fontDatabase = new QCoreTextFontDatabaseEngineFactory<QFontEngineFT>;
#  else
    m_fontDatabase = new QCoreTextFontDatabaseEngineFactory<QCoreTextFontEngine>;
#  endif
#else
#  if QT_CONFIG(fontconfig)
    m_fontDatabase = new QGenericUnixFontDatabase;
#  else
    m_fontDatabase = QPlatformIntegration::fontDatabase();
#  endif
#endif

    QInputDeviceManagerPrivate::get(QGuiApplicationPrivate::inputDeviceManager())->setDeviceCount(
        QInputDeviceManager::DeviceTypePointer, 1);
    QInputDeviceManagerPrivate::get(QGuiApplicationPrivate::inputDeviceManager())->setDeviceCount(
        QInputDeviceManager::DeviceTypeKeyboard, 1);

}

bool QWebIntegration::hasCapability(QPlatformIntegration::Capability cap) const
{
    switch (cap) {
        case ThreadedPixmaps:
            return true;
        case MultipleWindows:
        case RhiBasedRendering:
            return false;
        default:
            return QPlatformIntegration::hasCapability(cap);
    }
}

QPlatformFontDatabase *QWebIntegration::fontDatabase() const
{
    return m_fontDatabase;
}

QPlatformWindow *QWebIntegration::createPlatformWindow(QWindow *window) const
{
    return new QFbWindow(window);
}

QPlatformBackingStore *QWebIntegration::createPlatformBackingStore(QWindow *window) const
{
    return new QFbBackingStore(window);
}

QAbstractEventDispatcher *QWebIntegration::createEventDispatcher() const
{
#ifdef Q_OS_WIN
    return new QEventDispatcherWin32;
#else
    return createUnixEventDispatcher();
#endif
}

QPlatformNativeInterface *QWebIntegration::nativeInterface() const
{
    if (!m_nativeInterface) {
        m_nativeInterface.reset(new QPlatformNativeInterface);
    }
    return m_nativeInterface.get();
}

QPlatformServices *QWebIntegration::services() const
{
    if (m_services.isNull()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
        m_services.reset(new QDesktopUnixServices);
#else
        m_services.reset(new QGenericUnixServices);
#endif
    }
    return m_services.data();
}

QT_END_NAMESPACE
