#include "AppController.h"
#include "StorageService.h"
#include "modules/ProactiveBehaviorModule.h"
#include "modules/MorningLollipopModule.h"
#include "agent/AgentClient.h"
#include "agent/AgentServiceSupervisor.h"
#include "agent/RagIndexCoordinator.h"
#include "sync/SyncCoordinator.h"
#include "AiCredentialStore.h"

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QCryptographicHash>
#include <QSettings>
#include <QLockFile>
#include <QMessageBox>
#include <memory>
#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

int main(int argc, char *argv[])
{
    QStringList startupArguments;
#ifdef Q_OS_WIN
    int wideArgc=0;LPWSTR *wideArgv=CommandLineToArgvW(GetCommandLineW(),&wideArgc);
    if(wideArgv){for(int i=0;i<wideArgc;++i)startupArguments<<QString::fromWCharArray(wideArgv[i]);LocalFree(wideArgv);}
#else
    for(int i=0;i<argc;++i)startupArguments<<QString::fromLocal8Bit(argv[i]);
#endif
    const auto hasArgument=[&](const QString &value){return startupArguments.contains(value);};
    QApplication app(argc, argv);
    const bool stage1UiAudit=hasArgument(QStringLiteral("--stage1-ui-audit"));
    const bool stage2FormalChatAudit=hasArgument(QStringLiteral("--stage2-formal-chat-audit"));
    const bool startupDiagnostics=hasArgument(QStringLiteral("--startup-diagnostics"));
    const QString startupDiagnosticsPath=QDir::current().filePath(QStringLiteral("stellacandie-startup-diagnostics.txt"));
    const auto startupMark=[&](const QString &value){if(!startupDiagnostics)return;QFile marker(startupDiagnosticsPath);if(marker.open(QIODevice::Append|QIODevice::Text)){marker.write((QDateTime::currentDateTime().toString(Qt::ISODateWithMs)+QStringLiteral(" ")+value+QStringLiteral("\n")).toUtf8());}};
    startupMark(QStringLiteral("application_created"));
    QApplication::setOrganizationName(QStringLiteral("EmotionSprite"));
    QApplication::setOrganizationDomain(QStringLiteral("local.emotionsprite"));
    QApplication::setApplicationName(QStringLiteral("Stellacandie"));
    QApplication::setApplicationVersion(QStringLiteral("0.9.0-rc.1"));
    QApplication::setQuitOnLastWindowClosed(false);
    const bool diagnosticRun=stage1UiAudit||stage2FormalChatAudit
        ||hasArgument(QStringLiteral("--proactive-self-test"))
        ||hasArgument(QStringLiteral("--cognitive-self-test"))
        ||hasArgument(QStringLiteral("--state-engine-self-test"))
        ||hasArgument(QStringLiteral("--lollipop-self-test"))
        ||hasArgument(QStringLiteral("--self-test-reset"));
    std::unique_ptr<QLockFile> instanceLock;
    if(!diagnosticRun){
        const QString dataDirectory=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dataDirectory);
        instanceLock=std::make_unique<QLockFile>(QDir(dataDirectory).filePath(QStringLiteral("stellacandie.instance.lock")));
        instanceLock->setStaleLockTime(0);
        if(!instanceLock->tryLock(100)){
            QMessageBox::information(nullptr,QStringLiteral("Stellacandie 已在运行"),
                QStringLiteral("检测到另一个 Stellacandie 实例。为保护聊天和记忆数据，本次不会重复启动。\n\n请从任务栏或系统托盘打开已经运行的精灵。"));
            return 3;
        }
    }
    if(stage1UiAudit||stage2FormalChatAudit||hasArgument(QStringLiteral("--proactive-self-test"))||hasArgument(QStringLiteral("--cognitive-self-test"))||hasArgument(QStringLiteral("--state-engine-self-test"))||hasArgument(QStringLiteral("--lollipop-self-test"))||hasArgument(QStringLiteral("--self-test-reset"))){QStandardPaths::setTestModeEnabled(true);
        QFile::remove(QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(QStringLiteral("emotion_sprite.db")));}

    if(hasArgument(QStringLiteral("--cognitive-self-test"))){
        QStringList failures;StorageService storage;if(!storage.initialize())failures<<storage.lastError();else{
            ProactiveBehaviorModule module(&storage,&storage);module.setEnabled(true);module.setDoNotDisturb(false);module.setQuietHours(0,0);QString reply;
            if(!module.handleUserMessage(QStringLiteral("两天后提醒我换个枕头"),&reply)||!reply.contains(QStringLiteral("几点")))failures<<QStringLiteral("ambiguous reminder confirmation failed");
            auto pending=storage.loadCognitiveRecords({QStringLiteral("awaiting_confirmation")});if(pending.size()!=1||pending.first().deliveryPriority!=100)failures<<QStringLiteral("explicit reminder priority/persistence failed");
            if(!module.handleUserMessage(QStringLiteral("晚上七点"),&reply))failures<<QStringLiteral("confirmation time failed");bool planned=false;for(const auto&r:storage.loadCognitiveRecords())if(r.status==QStringLiteral("planned")&&r.subject.contains(QStringLiteral("枕头"))&&r.reminderId>0)planned=true;if(!planned)failures<<QStringLiteral("reminder was not scheduled");
            if(!module.handleUserMessage(QStringLiteral("1分钟后提醒我开会"),&reply))failures<<QStringLiteral("meeting route failed");bool event=false;for(const auto&r:storage.loadCognitiveRecords())if(r.status==QStringLiteral("planned")&&r.recordType==QStringLiteral("event")&&r.maxFollowUps==1&&r.memoryImportance<50)event=true;if(!event)failures<<QStringLiteral("meeting event lifecycle failed");
            CognitiveRecord f;f.recordType=QStringLiteral("event");f.subject=QStringLiteral("测试会议");f.status=QStringLiteral("awaiting_followup");f.followUpAt=QDateTime::currentDateTime().addSecs(-1);f.expiresAt=QDateTime::currentDateTime().addDays(1);f.maxFollowUps=1;const qint64 id=storage.addCognitiveRecord(f);int notices=0;QObject::connect(&module,&ProactiveBehaviorModule::notificationRequested,[&](const QString&,const QString&){notices++;});module.evaluateNow();bool archived=false;for(const auto&r:storage.loadCognitiveRecords())if(r.id==id&&r.status==QStringLiteral("archived")&&r.followUpCount==1)archived=true;if(notices!=1||!archived)failures<<QStringLiteral("follow-up was not one-shot archived");
            MemoryRecord bad;bad.category=QStringLiteral("event");bad.subject=QStringLiteral("测试会议长期记忆");bad.content=QStringLiteral("明天七点开会");bad.importance=95;bad.confidence=.99;storage.upsertMemory(bad);storage.removeTimeBoundMemories();for(const auto&m:storage.loadMemories())if(m.subject==bad.subject)failures<<QStringLiteral("time-bound event polluted long-term memory");
        }
        const QJsonObject report{{QStringLiteral("passed"),failures.isEmpty()},{QStringLiteral("failures"),QJsonArray::fromStringList(failures)}};const QString dir=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);QDir().mkpath(dir);QFile f(QDir(dir).filePath(QStringLiteral("cognitive_self_test.json")));if(f.open(QIODevice::WriteOnly))f.write(QJsonDocument(report).toJson(QJsonDocument::Indented));qInfo().noquote()<<QJsonDocument(report).toJson(QJsonDocument::Compact);return failures.isEmpty()?0:2;
    }

    if(hasArgument(QStringLiteral("--lollipop-self-test"))){QStringList failures;StorageService storage;AiService ai;if(!storage.initialize())failures<<storage.lastError();else{MorningLollipopModule module(&storage,&ai);const auto aug=module.planForTest(QDate(2026,8,8));if(aug.acquisitionType!="memorial"||!aug.flavorName.contains("蜜桃")||aug.memorialKey!="important_0808_2026")failures<<"August 8 sweet memorial failed";const auto mar=module.planForTest(QDate(2027,3,11));if(mar.acquisitionType!="memorial"||!mar.flavorName.contains("青梅黑巧"))failures<<"March 11 bittersour memorial failed";const auto birthday=module.planForTest(QDate(2026,10,10));if(birthday.memorialKey!="user_birthday_2026")failures<<"user birthday failed";const auto sprite=module.planForTest(QDate(2027,6,19));if(sprite.memorialKey!="sprite_birthday_2027")failures<<"sprite birthday failed";const auto first=module.planForTest(QDate(2026,11,30));if(first.memorialKey!="first_chat_2026")failures<<"first chat anniversary failed";const auto joint=module.planForTest(QDate(2027,3,13));if(joint.memorialKey!="diary_star_2027"||!joint.story.contains("反向日记")||!joint.story.contains("星星纸"))failures<<"joint diary and star memorial failed";storage.upsertMorningLollipop(aug);const auto loaded=storage.loadMorningLollipop(aug.giftDate);if(loaded.memorialKey!=aug.memorialKey||loaded.acquisitionType!="memorial"||loaded.shape!="star")failures<<"metadata persistence failed";}qInfo().noquote()<<QJsonDocument(QJsonObject{{"passed",failures.isEmpty()},{"failures",QJsonArray::fromStringList(failures)}}).toJson(QJsonDocument::Compact);return failures.isEmpty()?0:2;}

    startupMark(QStringLiteral("controller_begin"));
    AppController controller;
    startupMark(QStringLiteral("controller_ready"));
    AgentClient agentClient;
    controller.setAgentClient(&agentClient);
    AgentServiceSupervisor agentSupervisor;
    QObject::connect(&agentSupervisor,&AgentServiceSupervisor::serviceStarted,&agentClient,&AgentClient::configure);
    QObject::connect(&agentSupervisor,&AgentServiceSupervisor::serviceUnavailable,&agentClient,[&agentClient](const QString&){agentClient.configure(QUrl(),QString());});
    QObject::connect(&controller,&AppController::agentConfigurationChanged,&agentSupervisor,&AgentServiceSupervisor::restart);
    agentSupervisor.start();
    startupMark(QStringLiteral("agent_supervisor_started"));
    if(stage2FormalChatAudit){
        const QString reportPath=QDir::current().filePath(QStringLiteral("stage2_formal_chat_audit.json"));
        QFile::remove(reportPath);
        auto writeReport=[&](bool passed,const QString&failure){
            QAbstractItemModel*model=controller.chatModel();QString replyHash;QString sender;
            if(model&&model->rowCount()>0){const QModelIndex last=model->index(model->rowCount()-1,0);sender=model->data(last,ChatMessageModel::SenderRole).toString();const QString reply=model->data(last,ChatMessageModel::TextRole).toString();replyHash=QString::fromLatin1(QCryptographicHash::hash(reply.toUtf8(),QCryptographicHash::Sha256).toHex());}
            const QJsonObject result{{QStringLiteral("passed"),passed},{QStringLiteral("route_mode"),controller.chatRouteMode()},
                {QStringLiteral("agent_available"),agentClient.available()},{QStringLiteral("status"),controller.aiStatus()},
                {QStringLiteral("last_sender"),sender},{QStringLiteral("reply_sha256"),replyHash},
                {QStringLiteral("message_count"),model?model->rowCount():0},{QStringLiteral("failure"),failure},
                {QStringLiteral("chat_body_persisted_in_report"),false}};
            QFile file(reportPath);if(file.open(QIODevice::WriteOnly))file.write(QJsonDocument(result).toJson(QJsonDocument::Indented));
        };
        bool *submitted=new bool(false);bool *finished=new bool(false);
        auto *readinessPoll=new QTimer(&app);readinessPoll->setInterval(100);
        QObject::connect(readinessPoll,&QTimer::timeout,&app,[&,submitted,readinessPoll]{if(*submitted||!agentClient.available())return;*submitted=true;readinessPoll->stop();controller.setChatRouteMode(QStringLiteral("agent_main"));controller.sendMessage(QStringLiteral("今天路边的晚霞很好看，我停下来多看了一会儿。"));});
        readinessPoll->start();
        QObject::connect(&controller,&AppController::aiStateChanged,&app,[&,finished]{if(*finished||!controller.aiStatus().startsWith(QStringLiteral("Agent主链路 · 完成")))return;*finished=true;writeReport(true,{});QTimer::singleShot(0,&app,&QCoreApplication::quit);});
        QTimer::singleShot(65000,&app,[&,finished]{if(*finished)return;*finished=true;writeReport(false,QStringLiteral("formal_chat_timeout_or_fallback"));app.exit(2);});
        return app.exec();
    }
    AgentClient syncClient;
    {QSettings syncSettings;const QUrl syncUrl(syncSettings.value(QStringLiteral("sync/cloudUrl")).toString());const QString syncToken=AiCredentialStore::loadSyncToken();if(syncUrl.isValid()&&!syncToken.isEmpty())syncClient.configure(syncUrl,syncToken);}
    SyncCoordinator syncCoordinator(controller.syncRepository(),&syncClient);
    syncCoordinator.start();
    RagIndexCoordinator ragIndex(controller.storageService(),&agentClient);
    QObject::connect(&controller,&AppController::memoryStateChanged,&ragIndex,&RagIndexCoordinator::scheduleRebuild);
    QObject::connect(&controller,&AppController::proactiveStateChanged,&ragIndex,&RagIndexCoordinator::scheduleRebuild);
    QObject::connect(&controller,&AppController::reverseDiaryStateChanged,&ragIndex,&RagIndexCoordinator::scheduleRebuild);
    QObject::connect(&controller,&AppController::dreamChanged,&ragIndex,&RagIndexCoordinator::scheduleRebuild);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("agentClient"), &agentClient);
    engine.rootContext()->setContextProperty(QStringLiteral("syncController"), &syncCoordinator);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { QCoreApplication::exit(EXIT_FAILURE); },
                     Qt::QueuedConnection);

    engine.loadFromModule(QStringLiteral("EmotionSprite"), QStringLiteral("Main"));
    startupMark(QStringLiteral("qml_loaded"));
    if (engine.rootObjects().isEmpty()) {
        return EXIT_FAILURE;
    }

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (!window) {
        return EXIT_FAILURE;
    }

    controller.attachWindow(window);
    controller.createTrayIcon();
    window->show();
    startupMark(QStringLiteral("window_shown"));
    if(stage1UiAudit){QTimer::singleShot(300,&controller,[&controller]{emit controller.requestChatWindow();});}

    if (hasArgument(QStringLiteral("--self-test-reset"))) {
        QTimer::singleShot(250, &controller, &AppController::resetPetStats);
        QTimer::singleShot(900, &app, &QCoreApplication::quit);
    }
    if (hasArgument(QStringLiteral("--self-test-ai"))) {
        QTimer::singleShot(250, &controller, &AppController::testAiConnection);
        QTimer::singleShot(8000, &app, &QCoreApplication::quit);
    }
    if (hasArgument(QStringLiteral("--force-diary-test"))) {
        QTimer::singleShot(300, &controller, &AppController::forceReverseDiaryTest);
        QTimer::singleShot(60000, &app, &QCoreApplication::quit);
    }
    if (hasArgument(QStringLiteral("--force-memory-test"))) {
        QTimer::singleShot(300, &controller, &AppController::forceMemoryTest);
        QTimer::singleShot(60000, &app, &QCoreApplication::quit);
    }
    if (hasArgument(QStringLiteral("--force-memory-recall-test"))) {
        QTimer::singleShot(300, &controller, &AppController::forceMemoryRecallTest);
        QTimer::singleShot(60000, &app, &QCoreApplication::quit);
    }
    if (hasArgument(QStringLiteral("--personality-training"))) {
        QTimer::singleShot(300, &controller, &AppController::runPersonalityTraining);
        QTimer::singleShot(12 * 60 * 1000, &app, &QCoreApplication::quit);
    }
    if(hasArgument(QStringLiteral("--ai-format-smoke-test"))){QTimer::singleShot(300,&controller,&AppController::runAiFormatSmokeTest);QTimer::singleShot(4*60*1000,&app,&QCoreApplication::quit);}
    if(hasArgument(QStringLiteral("--meme-smoke-test"))){QTimer::singleShot(300,&controller,&AppController::forceMemeTest);QTimer::singleShot(60000,&app,&QCoreApplication::quit);}
    if (hasArgument(QStringLiteral("--memory-integrity-test"))) {
        QTimer::singleShot(300, &controller, &AppController::runMemoryIntegrityTest);
        QTimer::singleShot(1500, &app, &QCoreApplication::quit);
    }
    if(hasArgument(QStringLiteral("--proactive-self-test"))){QTimer::singleShot(300,&controller,&AppController::runProactiveSelfTest);QTimer::singleShot(1800,&app,&QCoreApplication::quit);}
    if(hasArgument(QStringLiteral("--state-engine-self-test"))){QTimer::singleShot(300,&controller,&AppController::runStateEngineSelfTest);QTimer::singleShot(1200,&app,&QCoreApplication::quit);}
    if(hasArgument(QStringLiteral("--summary-window-test"))){QTimer::singleShot(300,&controller,&AppController::openSummaryMagic);QTimer::singleShot(2200,&app,&QCoreApplication::quit);}
    if(hasArgument(QStringLiteral("--dream-window-test"))){QTimer::singleShot(300,&controller,&AppController::openDreamBottle);QTimer::singleShot(2600,&app,&QCoreApplication::quit);}
    if(hasArgument(QStringLiteral("--lollipop-window-test"))){QTimer::singleShot(300,&controller,&AppController::openMorningLollipop);QTimer::singleShot(3500,&app,&QCoreApplication::quit);}
    if(hasArgument(QStringLiteral("--diary-window-test"))){QTimer::singleShot(300,&controller,&AppController::openDiary);QTimer::singleShot(3500,&app,&QCoreApplication::quit);}
    if(hasArgument(QStringLiteral("--memory-window-test"))){QTimer::singleShot(300,&controller,&AppController::openMemory);}
    if(hasArgument(QStringLiteral("--chat-window-test"))){QTimer::singleShot(300,&controller,&AppController::requestChatWindow);QTimer::singleShot(3500,&app,&QCoreApplication::quit);}
    return app.exec();
}
