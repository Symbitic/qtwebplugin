# Copyright (C) 2025 The Qt Company Ltd.
# SPDX-License-Identifier: BSD-3-Clause

#####################################################################
## QWebIntegrationPlugin Plugin:
#####################################################################

qt_internal_add_plugin(QWebIntegrationPlugin
    OUTPUT_NAME qweb
    PLUGIN_TYPE platforms
    #DEFAULT_IF "minimal" IN_LIST QT_QPA_PLATFORMS
    SOURCES
        main.cpp
        qwebintegration.cpp qwebintegration.h
    DEFINES
        QT_NO_FOREACH
    LIBRARIES
        Qt::Core
        Qt::CorePrivate
        Qt::FbSupportPrivate
        Qt::Gui
        Qt::GuiPrivate
        Qt::Concurrent
        Qt::HttpServer
        Qt::WebSockets
)

set(resource_files
    favicon.ico
    index.html
    main.js
)

qt_internal_add_resource(QWebIntegrationPlugin "www"
    PREFIX
        "/www"
    FILES
        ${resource_files}
)

qt_internal_extend_target(QWebIntegrationPlugin CONDITION TARGET Freetype::Freetype
    LIBRARIES
        Freetype::Freetype
)
qt_internal_extend_target(QWebIntegrationPlugin CONDITION TARGET Qt::InputSupportPrivate
    LIBRARIES
        Qt::InputSupportPrivate
)
