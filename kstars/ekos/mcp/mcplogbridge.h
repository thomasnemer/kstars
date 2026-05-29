/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QHash>
#include <QObject>
#include <QString>

namespace MCP
{

class Transport;

class LogBridge : public QObject
{
    Q_OBJECT

public:
    explicit LogBridge(Transport *transport, QObject *parent = nullptr);

    void connectModule(const QString &moduleName, QObject *module);

private Q_SLOTS:
    void onNewLog(const QString &text);

private:
    Transport              *m_transport { nullptr };
    QHash<QObject *, QString> m_moduleNames;
};

} // namespace MCP
