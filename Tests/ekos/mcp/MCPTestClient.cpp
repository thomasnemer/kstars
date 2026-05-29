/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "MCPTestClient.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QTcpSocket>

MCPTestClient::MCPTestClient(quint16 port, const QString &token)
    : m_port(port), m_token(token)
{
}

MCPTestClient::~MCPTestClient()
{
    if (m_sseSocket)
    {
        m_sseSocket->disconnectFromHost();
        delete m_sseSocket;
    }
}

QByteArray MCPTestClient::buildRequest(const QString &method, const QString &path,
                                       const QByteArray &body) const
{
    QByteArray req;
    req += method.toLatin1() + " " + path.toLatin1() + " HTTP/1.1\r\n";
    req += "Host: localhost\r\n";
    if (!m_token.isEmpty())
        req += "Authorization: Bearer " + m_token.toLatin1() + "\r\n";
    if (!body.isEmpty())
    {
        req += "Content-Type: application/json\r\n";
        req += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    }
    if (method == "GET")
        req += "Accept: text/event-stream\r\n";
    req += "\r\n";
    req += body;
    return req;
}

QByteArray MCPTestClient::extractBody(const QByteArray &response) const
{
    int sep = response.indexOf("\r\n\r\n");
    if (sep < 0)
        return {};
    return response.mid(sep + 4);
}

QJsonObject MCPTestClient::post(const QJsonObject &request)
{
    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, m_port);
    if (!socket.waitForConnected(3000))
        return {};

    QByteArray body = QJsonDocument(request).toJson(QJsonDocument::Compact);
    socket.write(buildRequest("POST", "/mcp", body));

    QByteArray response;
    while (socket.waitForReadyRead(3000))
    {
        response += socket.readAll();
        if (response.contains("\r\n\r\n"))
        {
            // Check if we have a Content-Length and enough body
            int sep = response.indexOf("\r\n\r\n");
            QByteArray headers = response.left(sep);
            int clPos = headers.toLower().indexOf("content-length:");
            if (clPos >= 0)
            {
                int end = headers.indexOf("\r\n", clPos);
                int cl  = headers.mid(clPos + 15, end - clPos - 15).trimmed().toInt();
                if (response.size() >= sep + 4 + cl)
                    break;
            }
            else
            {
                break;
            }
        }
    }
    socket.disconnectFromHost();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(extractBody(response), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return {};
    return doc.object();
}

bool MCPTestClient::openSSE()
{
    m_sseSocket = new QTcpSocket();
    m_sseSocket->connectToHost(QHostAddress::LocalHost, m_port);
    if (!m_sseSocket->waitForConnected(3000))
    {
        delete m_sseSocket;
        m_sseSocket = nullptr;
        return false;
    }
    m_sseSocket->write(buildRequest("GET", "/mcp/stream"));
    m_sseSocket->waitForReadyRead(3000);
    return true;
}

QList<QJsonObject> MCPTestClient::readSSEEvents(int waitMs)
{
    if (!m_sseSocket)
        return {};

    m_sseSocket->waitForReadyRead(waitMs);
    QByteArray data = m_sseSocket->readAll();

    QList<QJsonObject> events;
    // SSE format: lines starting with "data: "
    for (const QByteArray &line : data.split('\n'))
    {
        QByteArray trimmed = line.trimmed();
        if (trimmed.startsWith("data: "))
        {
            QByteArray json = trimmed.mid(6);
            QJsonDocument doc = QJsonDocument::fromJson(json);
            if (doc.isObject())
                events.append(doc.object());
        }
    }
    return events;
}
