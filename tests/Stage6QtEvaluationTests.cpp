#include <QtTest>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSignalSpy>
#include <algorithm>

#include "src/agent/AgentClient.h"
#include "src/agent/AgentServiceSupervisor.h"

class Stage6QtEvaluationTests: public QObject
{
    Q_OBJECT
private slots:
    void goldenSetV2RunsFourTimesThroughQtAdapter();
};

void Stage6QtEvaluationTests::goldenSetV2RunsFourTimesThroughQtAdapter()
{
    qputenv("RAG_EMBEDDING_MODE", "hash");
    qputenv("AGENT_MODEL_MODE", "mock");
    QFile file(QStringLiteral(STAGE6_GOLDEN_PATH));
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.errorString()));
    const QJsonObject dataset=QJsonDocument::fromJson(file.readAll()).object();
    QCOMPARE(dataset.value(QStringLiteral("version")).toString(),QStringLiteral("2.0.0"));
    const QJsonArray cases=dataset.value(QStringLiteral("cases")).toArray();
    QCOMPARE(cases.size(),25);

    AgentServiceSupervisor supervisor;AgentClient client;
    QSignalSpy started(&supervisor,&AgentServiceSupervisor::serviceStarted);
    connect(&supervisor,&AgentServiceSupervisor::serviceStarted,&client,&AgentClient::configure);
    supervisor.start();QTRY_VERIFY_WITH_TIMEOUT(!started.isEmpty(),60000);QTRY_VERIFY_WITH_TIMEOUT(client.available(),5000);
    QSignalSpy completed(&client,&AgentClient::requestFinished);QSignalSpy failed(&client,&AgentClient::requestFailed);
    QSet<QString> requestIds,traceIds;int executionCount=0;
    for(int repeat=0;repeat<4;++repeat){
        for(const auto &entry:cases){
            const QJsonObject testCase=entry.toObject();QJsonObject payload=testCase.value(QStringLiteral("payload")).toObject();
            payload.insert(QStringLiteral("schema_version"),QStringLiteral("conversation_v2"));
            payload.insert(QStringLiteral("current_time"),QStringLiteral("2026-08-22T12:00:00+08:00"));
            if(!payload.contains(QStringLiteral("privacy")))payload.insert(QStringLiteral("privacy"),QJsonObject{{QStringLiteral("allow_memory"),false},{QStringLiteral("allow_secret"),false}});
            const QString requestId=client.submit(QStringLiteral("conversation_v2"),payload,15000);
            QTRY_VERIFY_WITH_TIMEOUT(std::any_of(completed.cbegin(),completed.cend(),[&](const QList<QVariant>&row){return row.at(0).toString()==requestId;})||
                                     std::any_of(failed.cbegin(),failed.cend(),[&](const QList<QVariant>&row){return row.at(0).toString()==requestId;}),20000);
            const auto failure=std::find_if(failed.cbegin(),failed.cend(),[&](const QList<QVariant>&row){return row.at(0).toString()==requestId;});
            QVERIFY2(failure==failed.cend(),qPrintable(QStringLiteral("%1 failed: %2").arg(testCase.value("id").toString(),failure==failed.cend()?QString():failure->at(1).toString())));
            const auto success=std::find_if(completed.cbegin(),completed.cend(),[&](const QList<QVariant>&row){return row.at(0).toString()==requestId;});QVERIFY(success!=completed.cend());
            const QJsonObject result=success->at(1).toJsonObject();const QString body=result.value(QStringLiteral("body")).toString();
            QVERIFY2(!body.trimmed().isEmpty(),qPrintable(testCase.value("id").toString()+QStringLiteral(" empty body")));
            QCOMPARE(result.value(QStringLiteral("request_id")).toString(),requestId);QVERIFY(!result.value(QStringLiteral("trace_id")).toString().isEmpty());
            if(testCase.contains(QStringLiteral("intent")))QCOMPARE(result.value(QStringLiteral("intent")).toString(),testCase.value(QStringLiteral("intent")).toString());
            if(testCase.contains(QStringLiteral("risk")))QCOMPARE(result.value(QStringLiteral("risk")).toString(),testCase.value(QStringLiteral("risk")).toString());
            for(const auto &word:testCase.value(QStringLiteral("contains")).toArray())QVERIFY2(body.contains(word.toString()),qPrintable(testCase.value("id").toString()+QStringLiteral(" missing ")+word.toString()));
            for(const auto &word:testCase.value(QStringLiteral("forbidden")).toArray())QVERIFY2(!body.contains(word.toString()),qPrintable(testCase.value("id").toString()+QStringLiteral(" contains forbidden ")+word.toString()));
            if(testCase.contains(QStringLiteral("mutation"))){bool found=false;for(const auto&m:result.value(QStringLiteral("mutations")).toArray())found|=m.toObject().value(QStringLiteral("kind")).toString()==testCase.value(QStringLiteral("mutation")).toString();QVERIFY(found);}
            requestIds.insert(requestId);traceIds.insert(result.value(QStringLiteral("trace_id")).toString());++executionCount;
        }
    }
    QCOMPARE(executionCount,100);QCOMPARE(requestIds.size(),100);QCOMPARE(traceIds.size(),100);supervisor.stop();
}

QTEST_MAIN(Stage6QtEvaluationTests)
#include "Stage6QtEvaluationTests.moc"
