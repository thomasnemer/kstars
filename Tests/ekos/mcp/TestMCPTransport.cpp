/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "TestMCPTransport.h"
#include "MCPTestClient.h"
#include "ekos/mcp/mcptransport.h"

#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QTest>

QTEST_MAIN(TestMCPTransport)

void TestMCPTransport::testStartStop()
{
    MCP::Transport t;
    QVERIFY(t.start(0));
    QVERIFY(t.isListening());
    quint16 port = t.serverPort();
    QVERIFY(port > 0);
    t.stop();
    QVERIFY(!t.isListening());
    QVERIFY(t.start(0));
    QVERIFY(t.isListening());
}

void TestMCPTransport::testPostRequest()
{
    MCP::Transport t;
    QVERIFY(t.start(0));

    QSignalSpy spy(&t, &MCP::Transport::requestReceived);

    MCPTestClient client(t.serverPort());
    QJsonObject req;
    req["jsonrpc"] = "2.0";
    req["id"]      = 1;
    req["method"]  = "tools/list";
    client.post(req);

    QVERIFY(spy.wait(3000));
    QCOMPARE(spy.count(), 1);
    QByteArray body = spy.at(0).at(1).toByteArray();
    QVERIFY(body.contains("tools/list"));
}

void TestMCPTransport::testInvalidMethod()
{
    MCP::Transport t;
    QVERIFY(t.start(0));

    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, t.serverPort());
    QVERIFY(socket.waitForConnected(3000));

    // Send GET /mcp (not /mcp/stream) — should get 405
    QByteArray req = "GET /mcp HTTP/1.1\r\nHost: localhost\r\n\r\n";
    socket.write(req);
    QVERIFY(socket.waitForReadyRead(3000));
    QByteArray response = socket.readAll();
    QVERIFY(response.startsWith("HTTP/1.1 405"));
}

void TestMCPTransport::testUnauthorized()
{
    MCP::Transport t;
    t.setToken("secret-token");
    QVERIFY(t.start(0));

    QSignalSpy spy(&t, &MCP::Transport::requestReceived);

    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, t.serverPort());
    QVERIFY(socket.waitForConnected(3000));

    QByteArray body = R"({"jsonrpc":"2.0","id":1,"method":"tools/list"})";
    QByteArray req  = "POST /mcp HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: "
                      + QByteArray::number(body.size()) + "\r\n\r\n" + body;
    socket.write(req);
    QVERIFY(socket.waitForReadyRead(3000));
    QByteArray response = socket.readAll();
    QVERIFY(response.startsWith("HTTP/1.1 401"));
    QCOMPARE(spy.count(), 0);
}

void TestMCPTransport::testAuthorized()
{
    MCP::Transport t;
    t.setToken("secret-token");
    QVERIFY(t.start(0));

    QSignalSpy spy(&t, &MCP::Transport::requestReceived);

    MCPTestClient client(t.serverPort(), "secret-token");
    QJsonObject req;
    req["jsonrpc"] = "2.0";
    req["id"]      = 1;
    req["method"]  = "tools/list";
    client.post(req);

    QVERIFY(spy.wait(3000));
    QCOMPARE(spy.count(), 1);
}

void TestMCPTransport::testSSEConnection()
{
    MCP::Transport t;
    QVERIFY(t.start(0));

    QSignalSpy spy(&t, &MCP::Transport::sseClientConnected);

    MCPTestClient client(t.serverPort());
    QVERIFY(client.openSSE());

    QVERIFY(spy.wait(3000));
    QCOMPARE(spy.count(), 1);

    auto *sseSocket = spy.at(0).at(0).value<QTcpSocket *>();
    QVERIFY(sseSocket);

    t.broadcastSSEEvent("log", { {"module", "test"}, {"line", "hello"} });

    QList<QJsonObject> events = client.readSSEEvents(1000);
    QVERIFY(!events.isEmpty());
    QCOMPARE(events.first()["module"].toString(), QString("test"));
    QCOMPARE(events.first()["line"].toString(), QString("hello"));
}

void TestMCPTransport::testLargeBody()
{
    MCP::Transport t;
    QVERIFY(t.start(0));

    QSignalSpy spy(&t, &MCP::Transport::requestReceived);

    // Build a 64KB JSON string value
    QString longString(64 * 1024, 'x');
    QJsonObject req;
    req["jsonrpc"] = "2.0";
    req["id"]      = 1;
    req["method"]  = "tools/list";
    req["data"]    = longString;

    MCPTestClient client(t.serverPort());
    client.post(req);

    QVERIFY(spy.wait(3000));
    QCOMPARE(spy.count(), 1);

    QByteArray body = spy.at(0).at(1).toByteArray();
    QVERIFY(body.size() >= 64 * 1024);
}
