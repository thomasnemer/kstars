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

    template <typename T>
    void connectModule(const QString &name, T *module)
    {
        if (!module)
            return;
        m_moduleNames[module] = name;
        connect(module, &T::newLog, this, &LogBridge::onNewLog, Qt::UniqueConnection);
    }

private Q_SLOTS:
    void onNewLog(const QString &text);

private:
    Transport                *m_transport { nullptr };
    QHash<QObject *, QString> m_moduleNames;
};

} // namespace MCP
