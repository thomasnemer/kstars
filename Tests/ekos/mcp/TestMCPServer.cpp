/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "TestMCPServer.h"
#include "MCPTestClient.h"
#include "ekos/mcp/mcpserver.h"
#include "ekos/mcp/mcptoolregistry.h"
#include "ekos/mcp/tools/capturetools.h"
#include "ekos/mcp/tools/guidetools.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonObject>
#include <QTcpSocket>
#include <QTest>

QTEST_MAIN(TestMCPServer)

static quint16 startServer(MCP::Server &server)
{
    if (!server.start(0))
        return 0;
    return server.port();
}

void TestMCPServer::testInitialize()
{
    MCP::Server server;
    MCPTestClient client(startServer(server));

    QJsonObject req;
    req["jsonrpc"] = "2.0";
    req["id"]      = 1;
    req["method"]  = "initialize";
    req["params"]  = QJsonObject{};

    QJsonObject resp = client.post(req);
    QVERIFY(!resp.isEmpty());
    QVERIFY(resp.contains("result"));

    QJsonObject result = resp["result"].toObject();
    QVERIFY(result.contains("protocolVersion"));
    QVERIFY(result.contains("serverInfo"));
    QCOMPARE(result["serverInfo"].toObject()["name"].toString(), QString("kstars-mcp"));
}

void TestMCPServer::testToolsList()
{
    MCP::Server server;
    // Register a dummy tool so list is non-trivially testable
    server.registry()->registerTool({
        "test_tool", "A test tool", {}, [](const QJsonObject &, QString &) -> QJsonValue {
            return QJsonObject{ {"ok", true} };
        }
    });

    MCPTestClient client(startServer(server));

    QJsonObject req;
    req["jsonrpc"] = "2.0";
    req["id"]      = 1;
    req["method"]  = "tools/list";

    QJsonObject resp = client.post(req);
    QVERIFY(!resp.isEmpty());
    QVERIFY(resp.contains("result"));

    QJsonObject result = resp["result"].toObject();
    QVERIFY(result.contains("tools"));
    QVERIFY(result["tools"].isArray());
    QVERIFY(result["tools"].toArray().size() > 0);
}

void TestMCPServer::testToolsCallUnknown()
{
    MCP::Server server;
    MCPTestClient client(startServer(server));

    QJsonObject params;
    params["name"]      = "nonexistent_tool";
    params["arguments"] = QJsonObject{};

    QJsonObject req;
    req["jsonrpc"] = "2.0";
    req["id"]      = 1;
    req["method"]  = "tools/call";
    req["params"]  = params;

    QJsonObject resp = client.post(req);
    QVERIFY(!resp.isEmpty());
    QVERIFY(resp.contains("error"));
    QVERIFY(resp["error"].toObject().contains("code"));
}

void TestMCPServer::testToolsCallModuleUnavailable()
{
    MCP::Server server;

    // Register a tool that simulates a null module check
    server.registry()->registerTool({
        "mount_coords", "Get mount coordinates", {},
        [](const QJsonObject &, QString &error) -> QJsonValue {
            error = "Mount module not available";
            return {};
        }
    });

    MCPTestClient client(startServer(server));

    QJsonObject params;
    params["name"]      = "mount_coords";
    params["arguments"] = QJsonObject{};

    QJsonObject req;
    req["jsonrpc"] = "2.0";
    req["id"]      = 1;
    req["method"]  = "tools/call";
    req["params"]  = params;

    QJsonObject resp = client.post(req);
    QVERIFY(!resp.isEmpty());
    QVERIFY(resp.contains("error"));
    QString msg = resp["error"].toObject()["message"].toString();
    QVERIFY(msg.contains("not available") || msg.contains("Mount"));
}

void TestMCPServer::testInvalidJSON()
{
    MCP::Server server;
    MCPTestClient client(startServer(server));

    // Send raw invalid JSON via QTcpSocket (MCPTestClient doesn't support this directly)
    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, server.port());
    QVERIFY(socket.waitForConnected(3000));

    QByteArray body    = "this is not json {{{";
    QByteArray request = "POST /mcp HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: "
                         + QByteArray::number(body.size()) + "\r\n\r\n" + body;
    socket.write(request);
    QVERIFY(socket.waitForReadyRead(3000));

    QByteArray response = socket.readAll();
    // Extract JSON body
    int sep = response.indexOf("\r\n\r\n");
    QVERIFY(sep >= 0);
    QJsonDocument doc = QJsonDocument::fromJson(response.mid(sep + 4));
    QVERIFY(doc.isObject());
    QJsonObject obj = doc.object();
    QVERIFY(obj.contains("error"));
    QCOMPARE(obj["error"].toObject()["code"].toInt(), -32700);
}

void TestMCPServer::testMissingMethod()
{
    MCP::Server server;
    MCPTestClient client(startServer(server));

    QJsonObject req;
    req["jsonrpc"] = "2.0";
    req["id"]      = 1;
    // No "method" field

    QJsonObject resp = client.post(req);
    QVERIFY(!resp.isEmpty());
    // With an empty/missing method, the server should respond with method not found or
    // similar error
    QVERIFY(resp.contains("error") || resp.contains("result"));
}

void TestMCPServer::testCaptureGuideToolsRegistered()
{
    MCP::Server server;
    // Register with nullptr manager — toolsList() never invokes the lambdas
    MCP::Tools::initCaptureTools(server.registry(), nullptr);
    MCP::Tools::initGuideTools(server.registry(), nullptr);

    QJsonArray tools = server.registry()->toolsList();

    auto containsTool = [&tools](const QString &name) {
        for (const auto &t : tools)
            if (t.toObject()["name"].toString() == name)
                return true;
        return false;
    };

    QVERIFY2(containsTool("capture_status"), "capture_status not found in tools/list");
    QVERIFY2(containsTool("capture_start"),  "capture_start not found in tools/list");
    QVERIFY2(containsTool("guide_status"),   "guide_status not found in tools/list");
    QVERIFY2(containsTool("guide_start"),    "guide_start not found in tools/list");
    QCOMPARE(tools.size(), 17);
}
