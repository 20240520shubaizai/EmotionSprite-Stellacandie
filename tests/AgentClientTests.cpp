#include <QtTest>
#include <QSignalSpy>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>
#include <algorithm>

#include "src/agent/AgentClient.h"
#include "src/agent/AgentServiceSupervisor.h"

class AgentClientTests:public QObject
{
    Q_OBJECT
private slots:
    void managedServiceRequestAndCrashRecovery();
    void unavailableServiceDegradesWithoutCrash();
};

void AgentClientTests::managedServiceRequestAndCrashRecovery()
{
    qputenv("RAG_EMBEDDING_MODE","hash");
    qputenv("AGENT_MODEL_MODE","mock");
    qputenv("AGENT_SUPERVISOR_LOG",QFile::encodeName(QCoreApplication::applicationDirPath()+QStringLiteral("/agent-core-test.log")));
    AgentServiceSupervisor supervisor;
    AgentClient client;
    QSignalSpy started(&supervisor,&AgentServiceSupervisor::serviceStarted);
    QSignalSpy restarting(&supervisor,&AgentServiceSupervisor::serviceRestarting);
    connect(&supervisor,&AgentServiceSupervisor::serviceStarted,&client,&AgentClient::configure);
    supervisor.start();
    QTRY_VERIFY_WITH_TIMEOUT(!started.isEmpty(),60000);
    QTRY_VERIFY_WITH_TIMEOUT(client.available(),5000);

    QSignalSpy completed(&client,&AgentClient::requestFinished);
    const QString requestId=client.submit(QStringLiteral("echo"),QJsonObject{{QStringLiteral("value"),42}},5000);
    QTRY_VERIFY_WITH_TIMEOUT(!completed.isEmpty(),8000);
    QCOMPARE(completed.first().at(0).toString(),requestId);
    QCOMPARE(completed.first().at(1).toJsonObject().value(QStringLiteral("echo")).toInt(),42);

    QSignalSpy graphDone(&client,&AgentClient::requestFinished);
    QSignalSpy graphEvents(&client,&AgentClient::requestEvent);
    QSignalSpy graphFailed(&client,&AgentClient::requestFailed);
    const QString graphId=client.submit(QStringLiteral("conversation_v2"),QJsonObject{{QStringLiteral("schema_version"),QStringLiteral("conversation_v2")},{QStringLiteral("text"),QStringLiteral("我收拾一下准备去开会")},{QStringLiteral("current_time"),QStringLiteral("2026-08-18T14:30:00+08:00")},{QStringLiteral("privacy"),QJsonObject{{QStringLiteral("allow_memory"),true}}}},5000);
    QTRY_VERIFY_WITH_TIMEOUT(std::any_of(graphDone.cbegin(),graphDone.cend(),[&](const QList<QVariant>&row){return row.at(0).toString()==graphId;})||
                             std::any_of(graphFailed.cbegin(),graphFailed.cend(),[&](const QList<QVariant>&row){return row.at(0).toString()==graphId;}),12000);
    const auto failedIt=std::find_if(graphFailed.cbegin(),graphFailed.cend(),[&](const QList<QVariant>&row){return row.at(0).toString()==graphId;});
    if(failedIt!=graphFailed.cend())QFAIL(qPrintable(QStringLiteral("conversation_v2 failed: %1 / %2").arg(failedIt->at(1).toString(),failedIt->at(2).toString())));
    const auto graphIt=std::find_if(graphDone.cbegin(),graphDone.cend(),[&](const QList<QVariant>&row){return row.at(0).toString()==graphId;});
    QVERIFY(graphIt!=graphDone.cend());
    const auto graphResult=graphIt->at(1).toJsonObject();
    QVERIFY2(graphResult.value(QStringLiteral("request_id")).toString()==graphId,
             qPrintable(QStringLiteral("unexpected conversation result: %1")
                            .arg(QString::fromUtf8(QJsonDocument(graphResult).toJson(QJsonDocument::Compact)))));
    QVERIFY(graphResult.value(QStringLiteral("node_trace")).toArray().contains(QStringLiteral("response_verifier")));
    QVERIFY(graphEvents.count()>=8);

    QSignalSpy synced(&client,&AgentClient::syncBatchFinished);
    QSignalSpy syncFailed(&client,&AgentClient::syncBatchFailed);
    const QString unique=QUuid::createUuid().toString(QUuid::WithoutBraces);
    client.submitSyncBatch(QJsonArray{QJsonObject{{"idempotency_key",QStringLiteral("qt-sync-")+unique},{"user_id","local-single-user"},{"entity_type","pet_state"},{"entity_uuid",unique},{"revision",1},{"operation","upsert"},{"privacy_level","normal"},{"payload",QJsonObject{{"mood",70}}}}});
    QTRY_VERIFY_WITH_TIMEOUT(!synced.isEmpty()||!syncFailed.isEmpty(),12000);
    // The bundled local Agent must never pretend its development SQLite is cloud MySQL.
    QVERIFY(synced.isEmpty());
    QCOMPARE(syncFailed.last().at(0).toString(),QStringLiteral("http_503"));

    QSignalSpy ragBuilt(&client,&AgentClient::requestFinished);
    const QJsonObject memory{{"record_id",QStringLiteral("qt-memory-")+unique},{"source_type","user_memory"},{"fact_type","confirmed_fact"},{"subject",QStringLiteral("橘猫")},{"content",QStringLiteral("用户喜欢胖胖的橘猫。")} ,{"revision",1}};
    const QString rebuildId=client.submit(QStringLiteral("rag_rebuild_v1"),QJsonObject{{"documents",QJsonArray{memory}}},10000,QStringLiteral("background"));
    QTRY_VERIFY_WITH_TIMEOUT(!ragBuilt.isEmpty(),12000);
    QCOMPARE(ragBuilt.last().at(0).toString(),rebuildId);

    QSignalSpy retrieved(&client,&AgentClient::requestFinished);
    QSignalSpy retrieveFailed(&client,&AgentClient::requestFailed);
    const QString retrieveId=client.submit(QStringLiteral("memory_retrieve_v1"),QJsonObject{{"query",QStringLiteral("喜欢什么宠物")},{"limit",3}},10000);
    QTRY_VERIFY_WITH_TIMEOUT(!retrieved.isEmpty()||!retrieveFailed.isEmpty(),12000);
    if(!retrieveFailed.isEmpty()){
        const QString detail=QStringLiteral("retrieve failed: %1 / %2").arg(retrieveFailed.last().at(1).toString(),retrieveFailed.last().at(2).toString());
        QVERIFY2(retrieveFailed.isEmpty(),qPrintable(detail));
    }
    QCOMPARE(retrieved.last().at(0).toString(),retrieveId);
    const auto ragResult=retrieved.last().at(1).toJsonObject();
    QCOMPARE(ragResult.value("results").toArray().first().toObject().value("record_id").toString(),QStringLiteral("qt-memory-")+unique);
    QVERIFY(!QJsonDocument(ragResult.value("trace").toArray()).toJson().contains("胖胖的橘猫"));

    QSignalSpy catalogDone(&client,&AgentClient::requestFinished);
    QSignalSpy catalogFailed(&client,&AgentClient::requestFailed);
    const QString catalogId=client.submit(QStringLiteral("tool_catalog_v1"),{},5000);
    QTRY_VERIFY_WITH_TIMEOUT(!catalogDone.isEmpty()||!catalogFailed.isEmpty(),12000);
    if(!catalogFailed.isEmpty()){
        const QString detail=QStringLiteral("catalog failed: %1 / %2").arg(catalogFailed.last().at(1).toString(),catalogFailed.last().at(2).toString());
        QVERIFY2(catalogFailed.isEmpty(),qPrintable(detail));
    }
    QCOMPARE(catalogDone.last().at(0).toString(),catalogId);
    const auto tools=catalogDone.last().at(1).toJsonObject().value("tools").toArray();QCOMPARE(tools.size(),3);

    QSignalSpy deniedDone(&client,&AgentClient::requestFinished);
    const QString deniedId=client.submit(QStringLiteral("tool_execute_v1"),QJsonObject{{"name","data.delete_all"},{"arguments",QJsonObject{{"scope","all_user_data"}}}},5000);
    QTRY_VERIFY_WITH_TIMEOUT(!deniedDone.isEmpty(),8000);
    QCOMPARE(deniedDone.last().at(0).toString(),deniedId);
    const auto denied=deniedDone.last().at(1).toJsonObject();
    QCOMPARE(denied.value("status").toString(),QStringLiteral("denied"));
    QCOMPARE(denied.value("error_code").toString(),QStringLiteral("tool_not_found"));

    QSignalSpy diagnosticsDone(&client,&AgentClient::requestFinished);
    const QString diagnosticsId=client.submit(QStringLiteral("diagnostics_snapshot_v1"),{},5000,QStringLiteral("background"));
    QTRY_VERIFY_WITH_TIMEOUT(!diagnosticsDone.isEmpty(),8000);
    QCOMPARE(diagnosticsDone.last().at(0).toString(),diagnosticsId);
    QVERIFY(diagnosticsDone.last().at(1).toJsonObject().contains("metrics"));

    supervisor.simulateCrashForTest();
    QTRY_VERIFY_WITH_TIMEOUT(!restarting.isEmpty(),5000);
    QTRY_VERIFY_WITH_TIMEOUT(started.count()>=2,60000);
    QTRY_VERIFY_WITH_TIMEOUT(client.available(),5000);
    supervisor.stop();
}

void AgentClientTests::unavailableServiceDegradesWithoutCrash()
{
    AgentClient client;
    client.configure(QUrl(QStringLiteral("http://127.0.0.1:1")),QStringLiteral("invalid"));
    QTRY_COMPARE_WITH_TIMEOUT(client.state(),QStringLiteral("offline"),5000);
    QVERIFY(!client.available());
    QSignalSpy failed(&client,&AgentClient::requestFailed);
    client.submit(QStringLiteral("ping"),{},500);
    QTRY_COMPARE_WITH_TIMEOUT(failed.count(),1,1000);
    QCOMPARE(failed.first().at(1).toString(),QStringLiteral("agent_offline"));
}

QTEST_MAIN(AgentClientTests)
#include "AgentClientTests.moc"
