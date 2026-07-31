#include "AppController.h"
#include "StorageService.h"
#include "modules/ProactiveBehaviorModule.h"
#include "modules/MorningLollipopModule.h"

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

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("EmotionSprite"));
    QApplication::setOrganizationDomain(QStringLiteral("local.emotionsprite"));
    QApplication::setApplicationName(QStringLiteral("Stellacandie"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QApplication::setQuitOnLastWindowClosed(false);
    if(app.arguments().contains(QStringLiteral("--proactive-self-test"))||app.arguments().contains(QStringLiteral("--cognitive-self-test"))||app.arguments().contains(QStringLiteral("--state-engine-self-test"))||app.arguments().contains(QStringLiteral("--lollipop-self-test"))){QStandardPaths::setTestModeEnabled(true);
        QFile::remove(QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(QStringLiteral("emotion_sprite.db")));}

    if(app.arguments().contains(QStringLiteral("--cognitive-self-test"))){
        QStringList failures;StorageService storage;if(!storage.initialize())failures<<storage.lastError();else{
            ProactiveBehaviorModule module(&storage);module.setEnabled(true);module.setDoNotDisturb(false);module.setQuietHours(0,0);QString reply;
            if(!module.handleUserMessage(QStringLiteral("两天后提醒我换个枕头"),&reply)||!reply.contains(QStringLiteral("几点")))failures<<QStringLiteral("ambiguous reminder confirmation failed");
            auto pending=storage.loadCognitiveRecords({QStringLiteral("awaiting_confirmation")});if(pending.size()!=1||pending.first().deliveryPriority!=100)failures<<QStringLiteral("explicit reminder priority/persistence failed");
            if(!module.handleUserMessage(QStringLiteral("晚上七点"),&reply))failures<<QStringLiteral("confirmation time failed");bool planned=false;for(const auto&r:storage.loadCognitiveRecords())if(r.status==QStringLiteral("planned")&&r.subject.contains(QStringLiteral("枕头"))&&r.reminderId>0)planned=true;if(!planned)failures<<QStringLiteral("reminder was not scheduled");
            if(!module.handleUserMessage(QStringLiteral("1分钟后提醒我开会"),&reply))failures<<QStringLiteral("meeting route failed");bool event=false;for(const auto&r:storage.loadCognitiveRecords())if(r.status==QStringLiteral("planned")&&r.recordType==QStringLiteral("event")&&r.maxFollowUps==1&&r.memoryImportance<50)event=true;if(!event)failures<<QStringLiteral("meeting event lifecycle failed");
            CognitiveRecord f;f.recordType=QStringLiteral("event");f.subject=QStringLiteral("测试会议");f.status=QStringLiteral("awaiting_followup");f.followUpAt=QDateTime::currentDateTime().addSecs(-1);f.expiresAt=QDateTime::currentDateTime().addDays(1);f.maxFollowUps=1;const qint64 id=storage.addCognitiveRecord(f);int notices=0;QObject::connect(&module,&ProactiveBehaviorModule::notificationRequested,[&](const QString&,const QString&){notices++;});module.evaluateNow();bool archived=false;for(const auto&r:storage.loadCognitiveRecords())if(r.id==id&&r.status==QStringLiteral("archived")&&r.followUpCount==1)archived=true;if(notices!=1||!archived)failures<<QStringLiteral("follow-up was not one-shot archived");
            MemoryRecord bad;bad.category=QStringLiteral("event");bad.subject=QStringLiteral("测试会议长期记忆");bad.content=QStringLiteral("明天七点开会");bad.importance=95;bad.confidence=.99;storage.upsertMemory(bad);storage.removeTimeBoundMemories();for(const auto&m:storage.loadMemories())if(m.subject==bad.subject)failures<<QStringLiteral("time-bound event polluted long-term memory");
        }
        const QJsonObject report{{QStringLiteral("passed"),failures.isEmpty()},{QStringLiteral("failures"),QJsonArray::fromStringList(failures)}};const QString dir=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);QDir().mkpath(dir);QFile f(QDir(dir).filePath(QStringLiteral("cognitive_self_test.json")));if(f.open(QIODevice::WriteOnly))f.write(QJsonDocument(report).toJson(QJsonDocument::Indented));qInfo().noquote()<<QJsonDocument(report).toJson(QJsonDocument::Compact);return failures.isEmpty()?0:2;
    }

    if(app.arguments().contains(QStringLiteral("--lollipop-self-test"))){QStringList failures;StorageService storage;AiService ai;if(!storage.initialize())failures<<storage.lastError();else{MorningLollipopModule module(&storage,&ai);const auto aug=module.planForTest(QDate(2026,8,8));if(aug.acquisitionType!="memorial"||!aug.flavorName.contains("蜜桃")||aug.memorialKey!="important_0808_2026")failures<<"August 8 sweet memorial failed";const auto mar=module.planForTest(QDate(2027,3,11));if(mar.acquisitionType!="memorial"||!mar.flavorName.contains("青梅黑巧"))failures<<"March 11 bittersour memorial failed";const auto birthday=module.planForTest(QDate(2026,10,10));if(birthday.memorialKey!="user_birthday_2026")failures<<"user birthday failed";const auto sprite=module.planForTest(QDate(2027,6,19));if(sprite.memorialKey!="sprite_birthday_2027")failures<<"sprite birthday failed";const auto first=module.planForTest(QDate(2026,11,30));if(first.memorialKey!="first_chat_2026")failures<<"first chat anniversary failed";const auto joint=module.planForTest(QDate(2027,3,13));if(joint.memorialKey!="diary_star_2027"||!joint.story.contains("反向日记")||!joint.story.contains("星星纸"))failures<<"joint diary and star memorial failed";storage.upsertMorningLollipop(aug);const auto loaded=storage.loadMorningLollipop(aug.giftDate);if(loaded.memorialKey!=aug.memorialKey||loaded.acquisitionType!="memorial"||loaded.shape!="star")failures<<"metadata persistence failed";}qInfo().noquote()<<QJsonDocument(QJsonObject{{"passed",failures.isEmpty()},{"failures",QJsonArray::fromStringList(failures)}}).toJson(QJsonDocument::Compact);return failures.isEmpty()?0:2;}

    AppController controller;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &controller);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { QCoreApplication::exit(EXIT_FAILURE); },
                     Qt::QueuedConnection);

    engine.loadFromModule(QStringLiteral("EmotionSprite"), QStringLiteral("Main"));
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

    if (app.arguments().contains(QStringLiteral("--self-test-reset"))) {
        QTimer::singleShot(250, &controller, &AppController::resetPetStats);
        QTimer::singleShot(900, &app, &QCoreApplication::quit);
    }
    if (app.arguments().contains(QStringLiteral("--self-test-ai"))) {
        QTimer::singleShot(250, &controller, &AppController::testAiConnection);
        QTimer::singleShot(8000, &app, &QCoreApplication::quit);
    }
    if (app.arguments().contains(QStringLiteral("--force-diary-test"))) {
        QTimer::singleShot(300, &controller, &AppController::forceReverseDiaryTest);
        QTimer::singleShot(60000, &app, &QCoreApplication::quit);
    }
    if (app.arguments().contains(QStringLiteral("--force-memory-test"))) {
        QTimer::singleShot(300, &controller, &AppController::forceMemoryTest);
        QTimer::singleShot(60000, &app, &QCoreApplication::quit);
    }
    if (app.arguments().contains(QStringLiteral("--force-memory-recall-test"))) {
        QTimer::singleShot(300, &controller, &AppController::forceMemoryRecallTest);
        QTimer::singleShot(60000, &app, &QCoreApplication::quit);
    }
    if (app.arguments().contains(QStringLiteral("--personality-training"))) {
        QTimer::singleShot(300, &controller, &AppController::runPersonalityTraining);
        QTimer::singleShot(12 * 60 * 1000, &app, &QCoreApplication::quit);
    }
    if(app.arguments().contains(QStringLiteral("--ai-format-smoke-test"))){QTimer::singleShot(300,&controller,&AppController::runAiFormatSmokeTest);QTimer::singleShot(4*60*1000,&app,&QCoreApplication::quit);}
    if(app.arguments().contains(QStringLiteral("--meme-smoke-test"))){QTimer::singleShot(300,&controller,&AppController::forceMemeTest);QTimer::singleShot(60000,&app,&QCoreApplication::quit);}
    if (app.arguments().contains(QStringLiteral("--memory-integrity-test"))) {
        QTimer::singleShot(300, &controller, &AppController::runMemoryIntegrityTest);
        QTimer::singleShot(1500, &app, &QCoreApplication::quit);
    }
    if(app.arguments().contains(QStringLiteral("--proactive-self-test"))){QTimer::singleShot(300,&controller,&AppController::runProactiveSelfTest);QTimer::singleShot(1800,&app,&QCoreApplication::quit);}
    if(app.arguments().contains(QStringLiteral("--state-engine-self-test"))){QTimer::singleShot(300,&controller,&AppController::runStateEngineSelfTest);QTimer::singleShot(1200,&app,&QCoreApplication::quit);}
    if(app.arguments().contains(QStringLiteral("--summary-window-test"))){QTimer::singleShot(300,&controller,&AppController::openSummaryMagic);QTimer::singleShot(2200,&app,&QCoreApplication::quit);}
    if(app.arguments().contains(QStringLiteral("--dream-window-test"))){QTimer::singleShot(300,&controller,&AppController::openDreamBottle);QTimer::singleShot(2600,&app,&QCoreApplication::quit);}
    if(app.arguments().contains(QStringLiteral("--lollipop-window-test"))){QTimer::singleShot(300,&controller,&AppController::openMorningLollipop);QTimer::singleShot(3500,&app,&QCoreApplication::quit);}
    if(app.arguments().contains(QStringLiteral("--diary-window-test"))){QTimer::singleShot(300,&controller,&AppController::openDiary);QTimer::singleShot(3500,&app,&QCoreApplication::quit);}
    if(app.arguments().contains(QStringLiteral("--memory-window-test"))){QTimer::singleShot(300,&controller,&AppController::openMemory);}
    if(app.arguments().contains(QStringLiteral("--chat-window-test"))){QTimer::singleShot(300,&controller,&AppController::requestChatWindow);QTimer::singleShot(3500,&app,&QCoreApplication::quit);}
    return app.exec();
}
