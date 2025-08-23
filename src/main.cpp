// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <qpa/qplatformintegrationplugin.h>
#include "qwebintegration.h"

QT_BEGIN_NAMESPACE

using namespace Qt::StringLiterals;

class QWebIntegrationPlugin : public QPlatformIntegrationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QPlatformIntegrationFactoryInterface_iid FILE "web.json")
public:
    QPlatformIntegration *create(const QString&, const QStringList&) override;
};

QPlatformIntegration *QWebIntegrationPlugin::create(const QString& system, const QStringList& paramList)
{
    if (!system.compare("web"_L1, Qt::CaseInsensitive)) {
        return new QWebIntegration(paramList);
    }

    return nullptr;
}

QT_END_NAMESPACE

#include "main.moc"