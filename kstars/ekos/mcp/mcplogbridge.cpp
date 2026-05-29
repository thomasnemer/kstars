/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mcplogbridge.h"
#include "mcptransport.h"

namespace MCP
{

LogBridge::LogBridge(Transport *transport, QObject *parent)
    : QObject(parent), m_transport(transport)
{
}

void LogBridge::connectModule(const QString &moduleName, QObject *module)
{
    if (!module)
        return;
    m_moduleNames[module] = moduleName;
    connect(module, SIGNAL(newLog(const QString &)), this, SLOT(onNewLog(const QString &)),
            Qt::UniqueConnection);
}

void LogBridge::onNewLog(const QString &text)
{
    QObject *src = sender();
    const QString moduleName = m_moduleNames.value(src, QStringLiteral("unknown"));
    m_transport->broadcastSSEEvent(QStringLiteral("log"), {
        { QStringLiteral("module"), moduleName },
        { QStringLiteral("line"),   text       }
    });
}

} // namespace MCP
