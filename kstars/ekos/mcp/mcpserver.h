/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonValue>

class QTcpSocket;

namespace Ekos
{
class Mount;
class Capture;
class Guide;
class FocusModule;
class Align;
class Scheduler;
}

namespace MCP
{

class Transport;
class ToolRegistry;

class Server : public QObject
{
    Q_OBJECT

public:
    explicit Server(QObject *parent = nullptr);

    void start(quint16 port);
    void stop();
    bool isListening() const;
    quint16 port() const;

    ToolRegistry *registry();

    // Called by Manager whenever module pointers change (INDI connect/disconnect)
    void setMount(Ekos::Mount *mount);
    void setCapture(Ekos::Capture *capture);
    void setGuide(Ekos::Guide *guide);
    void setFocus(Ekos::FocusModule *focus);
    void setAlign(Ekos::Align *align);
    void setScheduler(Ekos::Scheduler *scheduler);

private slots:
    void handleRequest(QTcpSocket *socket, const QByteArray &body);

private:
    QJsonObject makeResponse(const QJsonValue &id, const QJsonValue &result) const;
    QJsonObject makeError(const QJsonValue &id, int code, const QString &message) const;

    Transport        *m_transport { nullptr };
    ToolRegistry     *m_registry  { nullptr };

    Ekos::Mount      *m_mount     { nullptr };
    Ekos::Capture    *m_capture   { nullptr };
    Ekos::Guide      *m_guide     { nullptr };
    Ekos::FocusModule *m_focus    { nullptr };
    Ekos::Align      *m_align     { nullptr };
    Ekos::Scheduler  *m_scheduler { nullptr };
};

} // namespace MCP
