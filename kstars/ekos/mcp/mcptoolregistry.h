/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <functional>

namespace MCP
{

struct ToolParam
{
    QString name;
    QString type;        // "string", "number", "boolean", "integer"
    QString description;
    bool required { true };
};

struct ToolDefinition
{
    QString name;
    QString description;
    QList<ToolParam> params;
    std::function<QJsonValue(const QJsonObject &args, QString &error)> handler;
};

class ToolRegistry : public QObject
{
    Q_OBJECT

public:
    explicit ToolRegistry(QObject *parent = nullptr);

    void registerTool(const ToolDefinition &tool);
    QJsonArray toolsList() const;
    QJsonValue dispatch(const QString &name, const QJsonObject &args, QString &error) const;

private:
    QList<ToolDefinition> m_tools;
};

} // namespace MCP
