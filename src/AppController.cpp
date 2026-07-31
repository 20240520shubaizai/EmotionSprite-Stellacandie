#include "AppController.h"
#include "AiCredentialStore.h"
#include "modules/ReverseDiaryModule.h"
#include "modules/LongTermMemoryModule.h"
#include "modules/ProactiveBehaviorModule.h"
#include "modules/MemeCultureModule.h"
#include "modules/AdaptiveLearningModule.h"
#include "modules/FileSnackModule.h"
#include "modules/DataCleanupModule.h"
#include "modules/SummaryMagicModule.h"
#include "modules/DreamModule.h"
#include "modules/VisionRecognitionModule.h"
#include "modules/MorningLollipopModule.h"
#include "VisionService.h"

#include <QAction>
#include <QApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QMenu>
#include <QQuickWindow>
#include <QScreen>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QRandomGenerator>
#include <QRegularExpression>

#include <algorithm>

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_states{
          {QStringLiteral("专注待机"), QStringLiteral("qrc:/assets/states/stellacandie_00_attentive_clean.png")},
          {QStringLiteral("自然待机"), QStringLiteral("qrc:/assets/states/stellacandie_00_neutral_clean.png")},
          {QStringLiteral("开心"), QStringLiteral("qrc:/assets/states/stellacandie_01_happy_clean.png")},
          {QStringLiteral("好奇"), QStringLiteral("qrc:/assets/states/stellacandie_02_curious_clean.png")},
          {QStringLiteral("生气"), QStringLiteral("qrc:/assets/states/stellacandie_03_angry_clean.png")},
          {QStringLiteral("闹别扭"), QStringLiteral("qrc:/assets/states/stellacandie_04_pouting_clean.png")},
          {QStringLiteral("亲昵感动"), QStringLiteral("qrc:/assets/states/stellacandie_05_affectionate_clean.png")},
          {QStringLiteral("害羞"), QStringLiteral("qrc:/assets/states/stellacandie_06_shy_clean.png")},
          {QStringLiteral("困倦"), QStringLiteral("qrc:/assets/states/stellacandie_07_sleepy_clean.png")},
          {QStringLiteral("害怕"), QStringLiteral("qrc:/assets/states/stellacandie_08_scared_clean.png")},
          {QStringLiteral("生病"), QStringLiteral("qrc:/assets/states/stellacandie_09_sick_magic_cold_v2.png")},
          {QStringLiteral("恢复中"), QStringLiteral("qrc:/assets/states/stellacandie_10_recovering_magic_cold_v2.png")},
      }
    , m_chatModel(&m_storage, this)
    , m_petStateEngine(&m_storage, this)
    , m_diaryModel(&m_storage, this)
    , m_memoryModel(&m_storage, this)
{
    connect(&m_petStateEngine, &PetStateEngine::suggestedStateChanged,
            this, &AppController::setState);
    connect(&m_petStateEngine, &PetStateEngine::statsChanged,
            this, &AppController::petStatsChanged);

    QSettings aiSettings;
    const QString apiBaseUrl = aiSettings.value(QStringLiteral("ai/baseUrl"),
                                                 QStringLiteral("https://api.deepseek.com")).toString();
    const QString apiModel = aiSettings.value(QStringLiteral("ai/model"),
                                               QStringLiteral("deepseek-v4-flash")).toString();
    m_aiService.configure(apiBaseUrl, apiModel, AiCredentialStore::loadApiKey());
    m_visionService=new VisionService(this);const QString visionUrl=aiSettings.value(QStringLiteral("vision/baseUrl"),QStringLiteral("https://api.siliconflow.cn/v1")).toString();const QString visionModelName=aiSettings.value(QStringLiteral("vision/model"),QStringLiteral("Qwen/Qwen3-VL-8B-Instruct")).toString();m_visionService->configure(visionUrl,visionModelName,AiCredentialStore::loadVisionApiKey());m_visionStatus=m_visionService->isConfigured()?QStringLiteral("硅基流动视觉服务已配置"):QStringLiteral("视觉服务未配置");connect(m_visionService,&VisionService::busyChanged,this,&AppController::visionChanged);connect(m_visionService,&VisionService::testFinished,this,[this](bool,const QString&m){m_visionStatus=m;emit visionChanged();});
    m_aiStatus = m_aiService.isConfigured() ? QStringLiteral("AI已配置") : QStringLiteral("离线模式");
    connect(&m_aiService, &AiService::busyChanged, this, [this] {
        emit aiStateChanged();
    });
    connect(&m_aiService, &AiService::statusMessage, this, [this](const QString &message) {
        m_aiStatus = message;
        emit aiStateChanged();
    });
    connect(&m_aiService, &AiService::chatCompleted, this,
            [this](const QString &reply, const QString &emotion, const QJsonObject &stateEffect) {
        if (m_aiService.requestContext() != QStringLiteral("chat")) return;
        if(m_memeCulture)m_memeCulture->recordAssistantReply(reply);
        m_lastAssistantReply = reply;
        m_chatModel.append(QStringLiteral("pet"), reply);
        m_petStateEngine.applySemanticEffect(stateEffect);
        const int suggested = stateIndexForEmotion(emotion);
        if (suggested >= 0 && currentStateIndex() != 10) {
            setState(suggested);
            QTimer::singleShot(6000,this,[this]{setState(m_petStateEngine.currentResolvedState());});
        }
        m_aiStatus = QStringLiteral("AI在线");
        m_pendingUserText.clear();
        emit aiStateChanged();
        if (m_longTermMemory) QTimer::singleShot(0, m_longTermMemory, &LongTermMemoryModule::analyzeRecentConversation);
    });
    connect(&m_aiService, &AiService::chatFailed, this, [this](const QString &message) {
        if (m_aiService.requestContext() != QStringLiteral("chat")) return;
        m_aiStatus = message;
        appendOfflineReply(m_pendingUserText);
        m_pendingUserText.clear();
        emit aiStateChanged();
    });
    connect(&m_aiService, &AiService::chatCompleted, this, [this](const QString &reply, const QString &emotion, const QJsonObject &stateEffect) {
        if (m_aiService.requestContext() != QStringLiteral("personality_test") || m_trainingIndex < 0) return;
        m_trainingResults.append(QJsonObject{{"id",m_trainingIds.value(m_trainingIndex)},{"input",m_trainingInputs.value(m_trainingIndex)},
            {"reply",reply},{"emotion",emotion},{"state_effect",stateEffect},{"status","completed"}});
        ++m_trainingIndex; QTimer::singleShot(500,this,&AppController::runNextPersonalityCase);
    });
    connect(&m_aiService, &AiService::chatFailed, this, [this](const QString &message) {
        if (m_aiService.requestContext() != QStringLiteral("personality_test") || m_trainingIndex < 0) return;
        m_trainingResults.append(QJsonObject{{"id",m_trainingIds.value(m_trainingIndex)},{"input",m_trainingInputs.value(m_trainingIndex)},
            {"error",message},{"status","network_failed"}});
        ++m_trainingIndex; QTimer::singleShot(500,this,&AppController::runNextPersonalityCase);
    });
    connect(&m_aiService, &AiService::connectionTestFinished, this,
            [this](bool success, const QString &message) {
        m_aiStatus = message;
        Q_UNUSED(success)
        emit aiStateChanged();
    });
    if (m_storage.initialize()) {
        m_chatModel.load();
        m_petStateEngine.load();
        m_reverseDiary = new ReverseDiaryModule(&m_storage, &m_aiService);
        m_moduleManager.registerModule(m_reverseDiary);
        m_diaryModel.refresh();
        selectDiary(0);
        connect(m_reverseDiary, &ReverseDiaryModule::diaryGenerated, this,
                [this](const QString &content) {
            qInfo().noquote() << "REVERSE_DIARY_GENERATED:" << content;
            m_diaryModel.refresh();
            selectDiary(0);
            m_aiStatus = QStringLiteral("反向日记已生成并保存");
            emit aiStateChanged();
        });
        connect(m_reverseDiary, &ReverseDiaryModule::enabledChanged,
                this, &AppController::reverseDiaryStateChanged);
        connect(m_reverseDiary, &ReverseDiaryModule::generatingChanged,
                this, &AppController::reverseDiaryStateChanged);
        connect(m_reverseDiary, &ReverseDiaryModule::diaryFailed, this,
                [this](const QString &message) {
            qWarning().noquote() << "REVERSE_DIARY_FAILED:" << message;
            m_aiStatus = QStringLiteral("反向日记失败：%1").arg(message);
            emit aiStateChanged();
        });
        m_longTermMemory = new LongTermMemoryModule(&m_storage, &m_aiService);
        m_moduleManager.registerModule(m_longTermMemory);
        m_memoryModel.refresh();
        connect(m_longTermMemory,&LongTermMemoryModule::memoriesChanged,this,[this]{m_memoryModel.refresh();emit memoryStateChanged();});
        connect(m_longTermMemory,&LongTermMemoryModule::enabledChanged,this,&AppController::memoryStateChanged);
        connect(m_longTermMemory,&LongTermMemoryModule::analysisStatus,this,[this](const QString&s){qInfo().noquote()<<"MEMORY_STATUS:" << s;m_aiStatus=s;emit aiStateChanged();});
        m_dataCleanup=new DataCleanupModule(&m_storage,this);m_moduleManager.registerModule(m_dataCleanup);
        connect(m_dataCleanup,&DataCleanupModule::changed,this,[this]{emit dataCleanupChanged();});
        connect(m_dataCleanup,&DataCleanupModule::enabledChanged,this,&AppController::dataCleanupChanged);
        m_summaryMagic=new SummaryMagicModule(&m_storage,&m_aiService,this);m_moduleManager.registerModule(m_summaryMagic);
        connect(m_summaryMagic,&SummaryMagicModule::changed,this,&AppController::summaryMagicChanged);
        connect(m_summaryMagic,&SummaryMagicModule::enabledChanged,this,&AppController::summaryMagicChanged);
        connect(m_summaryMagic,&SummaryMagicModule::studyStarted,this,[this](const QString&m){m_chatModel.append(QStringLiteral("pet"),m);});
        connect(m_summaryMagic,&SummaryMagicModule::studyFinished,this,[this](const QString&m){m_chatModel.append(QStringLiteral("pet"),m);});
        m_dreamModule=new DreamModule(&m_storage,&m_aiService,this);m_moduleManager.registerModule(m_dreamModule);
        connect(m_dreamModule,&DreamModule::changed,this,&AppController::dreamChanged);
        connect(m_dreamModule,&DreamModule::enabledChanged,this,&AppController::dreamChanged);
        connect(m_dreamModule,&DreamModule::echoResponseReady,this,[this](const QString&reply){m_chatModel.append(QStringLiteral("pet"),reply);m_petStateEngine.applySemanticEffect(QJsonObject{{"mood",3},{"closeness",2},{"curiosity",1},{"confidence",100}});emit dreamChanged();});
        m_visionRecognition=new VisionRecognitionModule(m_visionService,this);m_moduleManager.registerModule(m_visionRecognition);connect(m_visionRecognition,&VisionRecognitionModule::changed,this,&AppController::visionChanged);connect(m_visionRecognition,&VisionRecognitionModule::enabledChanged,this,&AppController::visionChanged);connect(m_visionRecognition,&VisionRecognitionModule::recognized,this,[this](const QJsonObject&o,const QString&note){if(m_dreamModule)m_dreamModule->submitVisualRealityEcho(o,note);emit dreamChanged();});
        connect(m_visionRecognition,&VisionRecognitionModule::chatRecognized,this,[this](const QJsonObject&o,const QString&note,const QString&fileName){
            if(m_aiService.isBusy())return;
            const QString userText=QStringLiteral("【图片：%1】%2").arg(fileName,note.trimmed().isEmpty()?QString():QStringLiteral("\n")+note.trimmed());
            m_chatModel.append(QStringLiteral("user"),userText);m_pendingUserText=userText;
            if(m_adaptiveLearning)m_adaptiveLearning->observeUserResponse(note,m_lastAssistantReply);
            if(m_proactiveBehavior)m_proactiveBehavior->acknowledgeUserResponse();
            setState(m_petStateEngine.processMessage(note.trimmed().isEmpty()?QStringLiteral("分享了一张照片"):note));
            const int match=qBound(0,o.value(QStringLiteral("dream_match")).toInt(),100);
            const bool explicitDream=QRegularExpression(QStringLiteral("(?:梦|星星纸|昨晚|昨天|前天)")).match(note).hasMatch();
            QString dreamRule;
            if(explicitDream&&m_dreamModule)dreamRule=m_dreamModule->conversationContext(note,closeness());
            else if(match>=80&&QRandomGenerator::global()->bounded(100)<8)dreamRule=QStringLiteral("[LATENT_DREAM_ECHO] 照片与某场真实梦高度对应，但用户没有主动谈梦。本轮只允许用一句‘莫名有点熟悉’表达感觉，禁止说出梦、日期、星星纸或具体梦境情节。");
            else dreamRule=QStringLiteral("[NO_DREAM_REFERENCE] 即使后台发现梦境关联，本轮也禁止主动提梦、星星纸、相同或不同，只回应照片和用户分享意图。");
            QStringList objects;for(const auto&v:o.value(QStringLiteral("objects")).toArray())objects<<v.toString();
            const QString context=QStringLiteral("[CHAT_PHOTO] 视觉角色确认：%1\n可确认对象：%2\n用户附言：%3\n%4\n像真正看到用户发来的照片一样回复：先回应用户为什么分享，再挑一个最值得注意的事实表达感受；最多一个追问，50到110字，只生成一段。禁止罗列识图结果，禁止把不确定内容说成事实，想象必须使用‘像、感觉、让我想到’等标记。")
                .arg(o.value(QStringLiteral("description")).toString(),objects.join(QStringLiteral("、")),note.left(500),dreamRule);
            m_aiService.sendChat(m_storage.loadRecentMessages(20),mood(),energy(),health(),closeness(),boredom(),neglect(),curiosity(),irritation(),QStringLiteral("chat"),context);
            m_visionRecognition->clear();emit visionChanged();
        });
        m_proactiveBehavior=new ProactiveBehaviorModule(&m_storage,this);
        m_moduleManager.registerModule(m_proactiveBehavior);
        connect(m_proactiveBehavior,&ProactiveBehaviorModule::settingsChanged,this,&AppController::proactiveStateChanged);
        connect(m_proactiveBehavior,&ProactiveBehaviorModule::enabledChanged,this,&AppController::proactiveStateChanged);
        connect(m_proactiveBehavior,&ProactiveBehaviorModule::notificationRequested,this,[this](const QString &title,const QString &message){
            m_chatModel.append(QStringLiteral("pet"),message);if(m_trayIcon)m_trayIcon->showMessage(title,message,QSystemTrayIcon::Information,10000);
        });
        m_morningLollipop=new MorningLollipopModule(&m_storage,&m_aiService,this);m_moduleManager.registerModule(m_morningLollipop);
        connect(m_morningLollipop,&MorningLollipopModule::changed,this,&AppController::morningLollipopChanged);
        connect(m_morningLollipop,&MorningLollipopModule::enabledChanged,this,&AppController::morningLollipopChanged);
        connect(m_morningLollipop,&MorningLollipopModule::giftReady,this,[this](const QString&title,const QString&message){m_chatModel.append(QStringLiteral("pet"),message);if(m_trayIcon)m_trayIcon->showMessage(title,message,QSystemTrayIcon::Information,10000);setState(2);QTimer::singleShot(5000,this,[this]{setState(m_petStateEngine.currentResolvedState());});});
        m_memeCulture=new MemeCultureModule(this);m_moduleManager.registerModule(m_memeCulture);
        connect(m_memeCulture,&MemeCultureModule::learnedMemesChanged,this,&AppController::learningStateChanged);
        m_adaptiveLearning=new AdaptiveLearningModule(this);m_moduleManager.registerModule(m_adaptiveLearning);
        m_fileSnack=new FileSnackModule(&m_storage,this);m_moduleManager.registerModule(m_fileSnack);
        connect(m_fileSnack,&FileSnackModule::changed,this,&AppController::fileSnackChanged);
        connect(m_fileSnack,&FileSnackModule::enabledChanged,this,&AppController::fileSnackChanged);
        connect(m_fileSnack,&FileSnackModule::snackConsumed,this,[this](const QString &snack,const QString &emoji,int nutrition,const QString &reaction){
            emit snackEatingRequested(emoji.isEmpty()?QStringLiteral("🍪"):emoji);
            m_petStateEngine.feedSnack(nutrition);m_snackStatus=QStringLiteral("吃掉了%1！%2").arg(snack,reaction);
            m_chatModel.append(QStringLiteral("pet"),reaction);
            if(m_petStateEngine.healthPhase()==QStringLiteral("healthy")){setState(2);QTimer::singleShot(5000,this,[this]{setState(m_petStateEngine.currentResolvedState());});}
            else setState(m_petStateEngine.currentResolvedState());emit fileSnackChanged();
        });
    } else {
        qWarning() << "Failed to initialize local storage:" << m_storage.lastError();
    }
    QSettings settings;
    m_currentStateIndex = std::clamp(settings.value(QStringLiteral("pet/stateIndex"), 1).toInt(),
                                     0,
                                     static_cast<int>(m_states.size()) - 1);
    m_alwaysOnTop = settings.value(QStringLiteral("window/alwaysOnTop"), true).toBool();

    auto *stateRefreshTimer = new QTimer(this);
    stateRefreshTimer->setInterval(15 * 60 * 1000);
    connect(stateRefreshTimer, &QTimer::timeout, this, [this] {
        setState(m_petStateEngine.refreshForElapsedTime());
        if(m_fileSnack&&m_fileSnack->isEnabled()&&m_proactiveBehavior&&m_proactiveBehavior->isEnabled()&&!m_proactiveBehavior->doNotDisturb()&&m_petStateEngine.fullness()<=25){QSettings s;const QString today=QDate::currentDate().toString(Qt::ISODate);if(s.value(QStringLiteral("fileSnack/lastSnackRequestDate")).toString()!=today){const QString message=QStringLiteral("肚子有一点空空的……如果你刚好有不需要的小文件，可以加工成一颗小零食给我吗？没有也没关系！");m_chatModel.append(QStringLiteral("pet"),message);if(m_trayIcon)m_trayIcon->showMessage(QStringLiteral("精灵想吃小零食"),message,QSystemTrayIcon::Information,9000);s.setValue(QStringLiteral("fileSnack/lastSnackRequestDate"),today);}}
        {QSettings s;if(m_petStateEngine.fullness()<60&&s.value(QStringLiteral("summaryMagic/reservedSnackId"),0).toLongLong()>0&&!s.value(QStringLiteral("summaryMagic/reservedPrompted"),false).toBool()){const QString message=QStringLiteral("我现在有一点饿了……上次总结文章得到的那份奖励，还可以吃吗？你打开总结魔法确认一下就好。");m_chatModel.append(QStringLiteral("pet"),message);if(m_trayIcon)m_trayIcon->showMessage(QStringLiteral("精灵想领取总结奖励"),message,QSystemTrayIcon::Information,9000);s.setValue(QStringLiteral("summaryMagic/reservedPrompted"),true);emit summaryMagicChanged();}}
    });
    stateRefreshTimer->start();
}

int AppController::currentStateIndex() const
{
    return m_currentStateIndex;
}

QString AppController::currentStateName() const
{
    return m_states.at(m_currentStateIndex).name;
}

QString AppController::currentImage() const
{
    return m_states.at(m_currentStateIndex).image;
}

bool AppController::alwaysOnTop() const
{
    return m_alwaysOnTop;
}

QAbstractItemModel *AppController::chatModel()
{
    return &m_chatModel;
}

QString AppController::databasePath() const
{
    return m_storage.databasePath();
}

int AppController::mood() const { return m_petStateEngine.mood(); }
int AppController::energy() const { return m_petStateEngine.energy(); }
int AppController::health() const { return m_petStateEngine.health(); }
QString AppController::healthPhaseName()const{return m_petStateEngine.healthPhaseName();}
QString AppController::conditionName()const{return m_petStateEngine.conditionName();}
int AppController::recoveryProgress()const{return m_petStateEngine.recoveryProgress();}
int AppController::closeness() const { return m_petStateEngine.closeness(); }
int AppController::boredom() const { return m_petStateEngine.boredom(); }
int AppController::neglect() const { return m_petStateEngine.neglect(); }
int AppController::curiosity() const { return m_petStateEngine.curiosity(); }
int AppController::irritation() const { return m_petStateEngine.irritation(); }
int AppController::fullness() const { return m_petStateEngine.fullness(); }
bool AppController::aiConfigured() const { return m_aiService.isConfigured(); }
bool AppController::aiBusy() const { return m_aiService.isBusy(); }
QString AppController::aiStatus() const { return m_aiStatus; }
QString AppController::aiBaseUrl() const { return m_aiService.baseUrl(); }
QString AppController::aiModel() const { return m_aiService.model(); }
QAbstractItemModel *AppController::diaryModel() { return &m_diaryModel; }
int AppController::diaryCount() const { return m_diaryModel.rowCount(); }
QString AppController::selectedDiaryDate() const { return m_selectedDiary.entryDate.toString(QStringLiteral("yyyy年M月d日")); }
QString AppController::selectedDiaryContent() const { return m_selectedDiary.content; }
QString AppController::selectedDiaryUpdatedAt() const { return m_selectedDiary.updatedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm")); }
QStringList AppController::selectedDiaryStickers() const
{
    QStringList out;
    const QChar separator(0x1f);
    for (const auto &sticker : m_storage.loadDiaryStickers(m_selectedDiary.entryDate)) {
        out << QStringList{sticker.emoji, sticker.label, QString::number(sticker.xPercent),
                           QString::number(sticker.yPercent), QString::number(sticker.rotation)}.join(separator);
    }
    return out;
}
bool AppController::reverseDiaryEnabled() const { return m_reverseDiary && m_reverseDiary->isEnabled(); }
bool AppController::reverseDiaryGenerating() const { return m_reverseDiary && m_reverseDiary->isGenerating(); }
QAbstractItemModel *AppController::memoryModel() { return &m_memoryModel; }
QStringList AppController::memoryItems() const {QStringList out;const QChar separator(0x1f);for(const auto&m:m_storage.loadMemories()){QString content=m.content;content.replace(separator,QLatin1Char(' '));QString question=m.nextQuestion;question.replace(separator,QLatin1Char(' '));QString subject=m.subject;subject.replace(separator,QLatin1Char(' '));QString category=m.category;category.replace(separator,QLatin1Char(' '));out<<QStringList{category,subject,content,QString::number(m.importance),question}.join(separator);}return out;}
bool AppController::longTermMemoryEnabled() const { return m_longTermMemory && m_longTermMemory->isEnabled(); }
bool AppController::proactiveEnabled() const{return m_proactiveBehavior&&m_proactiveBehavior->isEnabled();}
bool AppController::doNotDisturb() const{return m_proactiveBehavior&&m_proactiveBehavior->doNotDisturb();}
int AppController::proactiveDailyLimit() const{return m_proactiveBehavior?m_proactiveBehavior->dailyLimit():3;}
int AppController::quietStartHour() const{return m_proactiveBehavior?m_proactiveBehavior->quietStartHour():23;}
int AppController::quietEndHour() const{return m_proactiveBehavior?m_proactiveBehavior->quietEndHour():8;}
QStringList AppController::activeCommitments()const{QStringList out;for(const auto&r:m_storage.loadCognitiveRecords({QStringLiteral("planned"),QStringLiteral("awaiting_confirmation")}))out<<QStringLiteral("%1 · %2").arg(r.subject,r.status==QStringLiteral("awaiting_confirmation")?QStringLiteral("等待确认时间"):r.scheduledAt.toString(QStringLiteral("MM-dd HH:mm")));for(const auto&c:m_storage.loadActiveCommitments())out<<QStringLiteral("%1 · %2").arg(c.description,c.dueAt.toString(QStringLiteral("MM-dd HH:mm")));out.removeDuplicates();return out;}
bool AppController::adaptiveLearningEnabled()const{return m_adaptiveLearning&&m_adaptiveLearning->isEnabled();}
bool AppController::memeCultureEnabled()const{return m_memeCulture&&m_memeCulture->isEnabled();}
QStringList AppController::learnedMemes()const{return m_memeCulture?m_memeCulture->learnedMemeSummaries():QStringList{};}
bool AppController::fileSnackEnabled()const{return m_fileSnack&&m_fileSnack->isEnabled();}
bool AppController::hasPendingSnack()const{return m_fileSnack&&m_fileSnack->hasPendingSnack();}
QString AppController::snackFileName()const{return m_fileSnack?m_fileSnack->fileName():QString();}
QString AppController::snackFileInfo()const{return m_fileSnack?m_fileSnack->fileInfo():QString();}
QString AppController::snackName()const{return m_fileSnack?m_fileSnack->snackName():QString();}
QString AppController::snackEmoji()const{return m_fileSnack?m_fileSnack->snackEmoji():QString();}
QString AppController::snackWarning()const{return m_fileSnack?m_fileSnack->warningText():QString();}
QString AppController::snackSourcePath()const{return m_fileSnack?m_fileSnack->sourcePath():QString();}
QString AppController::snackModifiedText()const{return m_fileSnack?m_fileSnack->modifiedText():QString();}
QString AppController::snackSafetyLevel()const{return m_fileSnack?m_fileSnack->safetyLevel():QString();}
bool AppController::snackStrongConfirmationRequired()const{return m_fileSnack&&m_fileSnack->strongConfirmationRequired();}
QStringList AppController::snackBagItems()const{return m_fileSnack?m_fileSnack->inventoryItems():QStringList{};}
QStringList AppController::snackCatalogItems()const{return m_fileSnack?m_fileSnack->catalogItems():QStringList{};}
QStringList AppController::snackHistoryItems()const{return m_fileSnack?m_fileSnack->historyItems():QStringList{};}
QStringList AppController::snackPendingFiles()const{return m_fileSnack?m_fileSnack->pendingFileItems():QStringList{};}
bool AppController::dataCleanupEnabled()const{return m_dataCleanup&&m_dataCleanup->isEnabled();}
QStringList AppController::managedMemoryItems()const{return m_dataCleanup?m_dataCleanup->memoryItems():QStringList{};}
QString AppController::memoryCleanupSummary()const{return m_dataCleanup?m_dataCleanup->summary():QString();}
QString AppController::memoryCleanupResult()const{return m_dataCleanup?m_dataCleanup->lastResult():QString();}
bool AppController::summaryMagicEnabled()const{return m_summaryMagic&&m_summaryMagic->isEnabled();}bool AppController::summaryMagicBusy()const{return m_summaryMagic&&m_summaryMagic->busy();}QString AppController::summarySourceName()const{return m_summaryMagic?m_summaryMagic->sourceName():QString();}QString AppController::summarySourceInfo()const{return m_summaryMagic?m_summaryMagic->sourceInfo():QString();}QString AppController::summaryInputText()const{return m_summaryMagic?m_summaryMagic->inputText():QString();}QString AppController::summaryResult()const{return m_summaryMagic?m_summaryMagic->resultText():QString();}QString AppController::summaryStatus()const{return m_summaryMagic?m_summaryMagic->status():QString();}QStringList AppController::summaryHistoryItems()const{return m_summaryMagic?m_summaryMagic->historyItems():QStringList{};}
bool AppController::summaryRewardReserved()const{return QSettings().value(QStringLiteral("summaryMagic/reservedSnackId"),0).toLongLong()>0;}
bool AppController::summaryRewardCanClaim()const{return summaryRewardReserved()&&m_petStateEngine.fullness()<60;}
bool AppController::dreamEnabled()const{return m_dreamModule&&m_dreamModule->isEnabled();}
bool AppController::dreamBusy()const{return m_dreamModule&&m_dreamModule->busy();}
QString AppController::dreamStatus()const{return m_dreamModule?m_dreamModule->status():QString();}
QStringList AppController::dreamItems()const{return m_dreamModule?m_dreamModule->items():QStringList{};}
int AppController::unopenedDreamCount()const{return m_dreamModule?m_dreamModule->unopenedCount():0;}
QString AppController::selectedDreamTitle()const{return m_dreamModule?m_dreamModule->selectedDream().title:QString();}
QString AppController::selectedDreamDate()const{return m_dreamModule?m_dreamModule->selectedDream().dreamDate.toString(QStringLiteral("yyyy年M月d日")):QString();}
QString AppController::selectedDreamContent()const{return m_dreamModule?m_dreamModule->selectedDream().content:QString();}
QString AppController::selectedDreamSymbols()const{return m_dreamModule?m_dreamModule->selectedDream().symbols.join(QStringLiteral(" · ")):QString();}
QString AppController::selectedDreamHint()const{return m_dreamModule?m_dreamModule->selectedDream().realityHint:QString();}
QString AppController::selectedDreamColor()const{return m_dreamModule?m_dreamModule->selectedDream().color:QStringLiteral("#E7C7D5");}
QString AppController::selectedDreamEcho()const{return m_dreamModule?m_dreamModule->selectedDream().realityEcho:QString();}
bool AppController::selectedDreamFavorite()const{return m_dreamModule&&m_dreamModule->selectedDream().favorite;}
bool AppController::visionConfigured()const{return m_visionService&&m_visionService->isConfigured();}
bool AppController::visionBusy()const{return m_visionService&&m_visionService->busy();}
QString AppController::visionStatus()const{return m_visionStatus;}
QString AppController::visionBaseUrl()const{return m_visionService?m_visionService->baseUrl():QString();}
QString AppController::visionModel()const{return m_visionService?m_visionService->model():QString();}
bool AppController::visionRecognitionEnabled()const{return m_visionRecognition&&m_visionRecognition->isEnabled();}
bool AppController::hasPendingVisionPhoto()const{return m_visionRecognition&&m_visionRecognition->hasPhoto();}
QString AppController::visionPhotoUrl()const{return m_visionRecognition?m_visionRecognition->photoUrl():QString();}
QString AppController::visionPhotoName()const{return m_visionRecognition?m_visionRecognition->fileName():QString();}
QString AppController::visionPhotoStatus()const{return m_visionRecognition?m_visionRecognition->status():QString();}
QString AppController::visionResultSummary()const{return m_visionRecognition?m_visionRecognition->resultSummary():QString();}
bool AppController::morningLollipopEnabled()const{return m_morningLollipop&&m_morningLollipop->isEnabled();}
QString AppController::morningLollipopStatus()const{return m_morningLollipop?m_morningLollipop->status():QString();}
QStringList AppController::morningLollipopItems()const{return m_morningLollipop?m_morningLollipop->items():QStringList{};}
int AppController::lollipopWorkdayStart()const{return m_morningLollipop?m_morningLollipop->workdayStart():450;}
int AppController::lollipopWorkdayEnd()const{return m_morningLollipop?m_morningLollipop->workdayEnd():600;}
int AppController::lollipopWeekendStart()const{return m_morningLollipop?m_morningLollipop->weekendStart():540;}
int AppController::lollipopWeekendEnd()const{return m_morningLollipop?m_morningLollipop->weekendEnd():690;}
QString AppController::selectedLollipopDate()const{return m_morningLollipop?m_morningLollipop->selected().giftDate.toString(QStringLiteral("yyyy年M月d日")):QString();}
QString AppController::selectedLollipopFlavor()const{return m_morningLollipop?m_morningLollipop->selected().flavorName:QString();}
QString AppController::selectedLollipopEmoji()const{return m_morningLollipop?m_morningLollipop->selected().emoji:QString();}
QString AppController::selectedLollipopGreeting()const{return m_morningLollipop?m_morningLollipop->selected().greeting:QString();}
QString AppController::selectedLollipopRarity()const{return m_morningLollipop?m_morningLollipop->selected().rarity:QString();}
bool AppController::selectedLollipopFavorite()const{return m_morningLollipop&&m_morningLollipop->selected().favorite;}
QString AppController::lollipopCity()const{return m_morningLollipop?m_morningLollipop->city():QString();}QString AppController::lollipopWeather()const{return m_morningLollipop?m_morningLollipop->weatherText():QString();}QString AppController::selectedLollipopType()const{return m_morningLollipop?m_morningLollipop->selected().acquisitionType:QString();}QString AppController::selectedLollipopStory()const{return m_morningLollipop?m_morningLollipop->selected().story:QString();}QString AppController::selectedLollipopWeather()const{return m_morningLollipop?m_morningLollipop->selected().weatherSnapshot:QString();}QString AppController::selectedLollipopColor()const{return m_morningLollipop?m_morningLollipop->selected().color:QString("#F2A8B5");}QString AppController::selectedLollipopShape()const{return m_morningLollipop?m_morningLollipop->selected().shape:QString("round");}QString AppController::selectedLollipopPattern()const{return m_morningLollipop?m_morningLollipop->selected().pattern:QString("swirl");}

void AppController::attachWindow(QQuickWindow *window)
{
    m_window = window;
    setAlwaysOnTop(m_alwaysOnTop);
    restoreWindowPosition();

    if (!m_desktopRoamSchedule) {
        m_desktopRoamSchedule = new QTimer(this);
        m_desktopRoamSchedule->setSingleShot(true);
        m_desktopRoamMove = new QTimer(this);
        m_desktopRoamMove->setInterval(16);
        connect(m_desktopRoamSchedule, &QTimer::timeout, this, [this] {
            if (!m_window || !m_window->isVisible()) { m_desktopRoamSchedule->start(5000); return; }
            QScreen *screen = m_window->screen();
            if (!screen) { m_desktopRoamSchedule->start(5000); return; }
            const QRect area = screen->availableGeometry();
            const int left = area.left();
            const int right = area.right() - m_window->width() + 1;
            if (right <= left) return;
            m_desktopRoamTargetX = QRandomGenerator::global()->bounded(left, right + 1);
            if (qAbs(m_desktopRoamTargetX - m_window->x()) < 180)
                m_desktopRoamTargetX = m_window->x() < (left + right) / 2 ? right - 30 : left + 30;
            m_desktopRoamDirection = m_desktopRoamTargetX >= m_window->x() ? 1 : -1;
            m_desktopAnimation = (energy() >= 60 && mood() >= 55
                                  && QRandomGenerator::global()->bounded(100) < 42)
                ? QStringLiteral("run") : QStringLiteral("walk");
            m_desktopRoaming = true;
            emit desktopRoamingChanged();
            m_desktopRoamMove->start();
        });
        connect(m_desktopRoamMove, &QTimer::timeout, this, [this] {
            if (!m_window || !m_desktopRoaming) return;
            const int remaining = m_desktopRoamTargetX - m_window->x();
            const int step = m_desktopRoamDirection * (m_desktopAnimation == QStringLiteral("run") ? 3 : 1);
            if (qAbs(remaining) <= qAbs(step)) {
                m_window->setX(m_desktopRoamTargetX);
                stopDesktopRoaming();
            } else {
                m_window->setX(m_window->x() + step);
            }
        });
        m_desktopRoamSchedule->start(3500);
    }
}

void AppController::stopDesktopRoaming()
{
    if (m_desktopRoamMove) m_desktopRoamMove->stop();
    if (m_desktopRoaming) {
        m_desktopRoaming = false;
        emit desktopRoamingChanged();
    }
    if (m_desktopRoamSchedule)
        m_desktopRoamSchedule->start(22000 + QRandomGenerator::global()->bounded(26001));
}

void AppController::triggerDesktopRunAway(int direction)
{
    if (!m_window || !m_window->screen()) return;
    const QRect area = m_window->screen()->availableGeometry();
    const int left = area.left();
    const int right = area.right() - m_window->width() + 1;
    m_desktopRoamDirection = direction < 0 ? -1 : 1;
    m_desktopRoamTargetX = m_desktopRoamDirection < 0 ? left + 12 : right - 12;
    m_desktopAnimation = QStringLiteral("run");
    m_desktopRoaming = true;
    if (m_desktopRoamSchedule) m_desktopRoamSchedule->stop();
    if (m_desktopRoamMove) m_desktopRoamMove->start();
    emit desktopRoamingChanged();
}

void AppController::createTrayIcon()
{
    if (m_trayIcon || !QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }

    m_trayIcon = new QSystemTrayIcon(QIcon(QStringLiteral(":/assets/states/stellacandie_00_neutral_clean.png")), this);
    m_trayIcon->setToolTip(QStringLiteral("情绪精灵 Stellacandie"));

    auto *menu = new QMenu;
    auto *showAction = menu->addAction(QStringLiteral("显示／隐藏精灵"));
    auto *topAction = menu->addAction(QStringLiteral("窗口置顶"));
    topAction->setCheckable(true);
    topAction->setChecked(m_alwaysOnTop);
    auto *settingsAction = menu->addAction(QStringLiteral("AI设置"));
    auto *diaryAction = menu->addAction(QStringLiteral("反向日记"));
    auto *memoryAction = menu->addAction(QStringLiteral("长期记忆"));
    auto *proactiveAction = menu->addAction(QStringLiteral("主动陪伴"));
    auto *learningAction = menu->addAction(QStringLiteral("学习与共同梗"));
    auto *snackAction = menu->addAction(QStringLiteral("零食加工坊"));
    auto *summaryAction = menu->addAction(QStringLiteral("AI总结魔法"));
    auto *dreamAction = menu->addAction(QStringLiteral("梦境星星瓶"));
    auto *lollipopAction = menu->addAction(QStringLiteral("晨间糖果罐"));
    menu->addSeparator();
    auto *quitAction = menu->addAction(QStringLiteral("退出"));

    connect(showAction, &QAction::triggered, this, &AppController::toggleWindowVisibility);
    connect(topAction, &QAction::toggled, this, &AppController::setAlwaysOnTop);
    connect(settingsAction, &QAction::triggered, this, &AppController::openSettings);
    connect(diaryAction, &QAction::triggered, this, &AppController::openDiary);
    connect(memoryAction, &QAction::triggered, this, &AppController::openMemory);
    connect(proactiveAction,&QAction::triggered,this,&AppController::openProactiveSettings);
    connect(learningAction,&QAction::triggered,this,&AppController::openLearningSettings);
    connect(snackAction,&QAction::triggered,this,&AppController::openFileSnack);
    connect(summaryAction,&QAction::triggered,this,&AppController::openSummaryMagic);
    connect(dreamAction,&QAction::triggered,this,&AppController::openDreamBottle);
    connect(lollipopAction,&QAction::triggered,this,&AppController::openMorningLollipop);
    connect(this, &AppController::alwaysOnTopChanged, topAction, [this, topAction] {
        topAction->setChecked(m_alwaysOnTop);
    });
    connect(quitAction, &QAction::triggered, this, &AppController::quitApplication);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            if (!m_window) return;
            ensureWindowOnScreen();
            m_window->show();
            m_window->raise();
            m_window->requestActivate();
        }
    });

    m_trayIcon->setContextMenu(menu);
    m_trayIcon->show();
    connect(m_trayIcon,&QSystemTrayIcon::messageClicked,this,[this]{if(m_window&&!m_window->isVisible())m_window->show();emit requestChatWindow();});
}

void AppController::nextState()
{
    setState((m_currentStateIndex + 1) % static_cast<int>(m_states.size()));
}

void AppController::previousState()
{
    const int count = static_cast<int>(m_states.size());
    setState((m_currentStateIndex - 1 + count) % count);
}

void AppController::setState(int index)
{
    if (index < 0 || index >= static_cast<int>(m_states.size()) || index == m_currentStateIndex) {
        return;
    }

    m_currentStateIndex = index;
    QSettings().setValue(QStringLiteral("pet/stateIndex"), index);
    emit currentStateChanged();
}

void AppController::saveWindowPosition()
{
    if (!m_window) {
        return;
    }

    QSettings settings;
    settings.setValue(QStringLiteral("window/x"), m_window->x());
    settings.setValue(QStringLiteral("window/y"), m_window->y());
}

void AppController::setAlwaysOnTop(bool enabled)
{
    m_alwaysOnTop = enabled;
    QSettings().setValue(QStringLiteral("window/alwaysOnTop"), enabled);

    if (m_window) {
        Qt::WindowFlags flags = Qt::Tool | Qt::FramelessWindowHint;
        if (enabled) {
            flags |= Qt::WindowStaysOnTopHint;
        }
        const bool wasVisible = m_window->isVisible();
        m_window->setFlags(flags);
        m_window->setColor(Qt::transparent);
        if (wasVisible) {
            m_window->show();
        }
    }

    emit alwaysOnTopChanged();
}

void AppController::toggleWindowVisibility()
{
    if (!m_window) {
        return;
    }

    if (m_window->isVisible()) {
        saveWindowPosition();
        m_window->hide();
    } else {
        ensureWindowOnScreen();
        m_window->show();
        m_window->raise();
        m_window->requestActivate();
    }
}

void AppController::quitApplication()
{
    saveWindowPosition();
    QCoreApplication::quit();
}

void AppController::sendMessage(const QString &text)
{
    const QString content = text.trimmed();
    if (content.isEmpty()) {
        return;
    }

    m_chatModel.append(QStringLiteral("user"), content);
    if(m_adaptiveLearning)m_adaptiveLearning->observeUserResponse(content,m_lastAssistantReply);
    if(m_proactiveBehavior)m_proactiveBehavior->acknowledgeUserResponse();
    const int ruleState = m_petStateEngine.processMessage(content);

    setState(ruleState);
    if(m_memeCulture&&m_memeCulture->handleLearningMessage(content,[this](const QString &reply){
        m_chatModel.append(QStringLiteral("pet"),reply);m_lastAssistantReply=reply;emit learningStateChanged();
    })) return;
    QString directiveReply;
    if(m_proactiveBehavior&&m_proactiveBehavior->handleUserMessage(content,&directiveReply)){m_chatModel.append(QStringLiteral("pet"),directiveReply);emit proactiveStateChanged();return;}
    if (m_longTermMemory && m_longTermMemory->handleUserDirective(content, &directiveReply)) {
        m_chatModel.append(QStringLiteral("pet"), directiveReply);
        m_aiStatus = QStringLiteral("长期记忆已更新");
        emit aiStateChanged();
        return;
    }
    if (m_aiService.isConfigured()) {
        m_pendingUserText = content;
        QString context;
        QString memoryContext;
        if (m_longTermMemory) {
            memoryContext=m_longTermMemory->relevantContext(content);
            context=m_longTermMemory->personalityContext(closeness(),boredom())+QStringLiteral("\n")+memoryContext;
        }
        if(m_dreamModule){const QString dreamContext=m_dreamModule->conversationContext(content,closeness());if(!dreamContext.isEmpty())context+=QStringLiteral("\n")+dreamContext;}
        const bool hasRetrievedMemory=memoryContext.contains(QStringLiteral("[MEMORY_CONTEXT]"));
        const bool memorySlipAllowed=hasRetrievedMemory&&QRandomGenerator::global()->bounded(100)<4;
        context+=memorySlipAllowed
            ?QStringLiteral("\n[MEMORY_SLIP_ALLOWED] 本轮可偶发一次记忆小偏差：只能基于上方真实检索到的记忆，改错一个不重要的小细节，并立刻用不确定语气向用户确认。不得改错人物、日期、提醒、承诺、健康、安全或珍贵事件。")
            :QStringLiteral("\n[NO_MEMORY_SLIP] 本轮不得故意记错任何事件；没有检索依据时尤其不得声称用户以前讲过。") ;
        const QRegularExpression noTopicRe(QStringLiteral("(?:没话题|没什么聊|不知道聊什么|你说点什么|随便聊聊|好无聊|无聊死了)"));
        const bool imaginationAllowed=noTopicRe.match(content).hasMatch()&&QRandomGenerator::global()->bounded(100)<3;
        context+=imaginationAllowed
            ?QStringLiteral("\n[IMAGINATION_EVENT_ALLOWED] 本轮可讲一个很短的幻想小插曲，但必须明确说成你自己的梦、想象或脑补，禁止说成用户经历过的往事。")
            :QStringLiteral("\n[NO_INVENTED_EVENT] 本轮禁止凭空编造用户经历或假装发生过不存在的现实事件。没话题时可以提议小游戏、问一个具体问题，或坦率说你也在想话题。") ;
        if(m_adaptiveLearning){if(!context.isEmpty())context+=QStringLiteral("\n");context+=m_adaptiveLearning->context();}
        context+=QStringLiteral("\n")+m_petStateEngine.healthContext();
        const auto send=[this,content](const QString &enriched){if(m_pendingUserText!=content||m_aiService.isBusy())return;m_aiService.sendChat(m_storage.loadRecentMessages(20),mood(),energy(),health(),closeness(),boredom(),neglect(),curiosity(),irritation(),QStringLiteral("chat"),enriched);};
        if(m_memeCulture)m_memeCulture->enrichContext(content,context,send);else send(context);
    } else {
        appendOfflineReply(content);
    }
}

void AppController::adjustPetStat(const QString &stat, int delta)
{
    // Queue state mutations so QML does not rebuild bindings while a button's
    // pointer event is still being dispatched.
    QTimer::singleShot(0, this, [this, stat, delta] {
        m_petStateEngine.adjustForDebug(stat, delta);
    });
}

void AppController::resetPetStats()
{
    QTimer::singleShot(0, this, [this] {
        m_petStateEngine.resetForDebug();
    });
}

void AppController::forceMagicCold(){m_petStateEngine.forceMagicColdForDebug();}
void AppController::advanceHealthRecovery(){m_petStateEngine.advanceRecoveryForDebug();}
void AppController::healPet(){m_petStateEngine.healForDebug();}
void AppController::letPetRest(){m_petStateEngine.rest();}

void AppController::openProactiveSettings(){emit requestProactiveWindow();}
void AppController::setProactiveEnabled(bool enabled){if(m_proactiveBehavior)m_proactiveBehavior->setEnabled(enabled);}
void AppController::setDoNotDisturb(bool enabled){if(m_proactiveBehavior)m_proactiveBehavior->setDoNotDisturb(enabled);}
void AppController::saveProactiveSettings(int limit,int start,int end){if(!m_proactiveBehavior)return;m_proactiveBehavior->setDailyLimit(limit);m_proactiveBehavior->setQuietHours(start,end);}
void AppController::addLifestyleReminder(const QString &message,int minutesFromNow){if(!m_proactiveBehavior)return;const QString text=message.trimmed();if(text.isEmpty())return;
    if(m_proactiveBehavior->schedule(QStringLiteral("lifestyle"),QDateTime::currentDateTime().addSecs(qMax(1,minutesFromNow)*60),text)){m_aiStatus=QStringLiteral("提醒已经记下了");emit aiStateChanged();}}
void AppController::completeCommitment(int row){const auto cognitive=m_storage.loadCognitiveRecords({QStringLiteral("planned"),QStringLiteral("awaiting_confirmation")});if(row>=0&&row<cognitive.size()){const auto item=cognitive.at(row);if(m_storage.updateCognitiveRecord(item.id,QStringLiteral("completed"))){if(item.reminderId>0)m_storage.updateReminderStatus(item.reminderId,QStringLiteral("cancelled"));emit proactiveStateChanged();}return;}const auto active=m_storage.loadActiveCommitments();row-=cognitive.size();if(row<0||row>=active.size())return;const auto item=active.at(row);if(m_storage.updateCommitmentStatus(item.id,QStringLiteral("completed"))){m_storage.cancelReminders(QStringLiteral("proactive_commitment"),item.description);emit proactiveStateChanged();}}
void AppController::cancelCommitment(int row){const auto cognitive=m_storage.loadCognitiveRecords({QStringLiteral("planned"),QStringLiteral("awaiting_confirmation")});if(row>=0&&row<cognitive.size()){const auto item=cognitive.at(row);if(m_storage.updateCognitiveRecord(item.id,QStringLiteral("cancelled"))){if(item.reminderId>0)m_storage.updateReminderStatus(item.reminderId,QStringLiteral("cancelled"));emit proactiveStateChanged();}return;}const auto active=m_storage.loadActiveCommitments();row-=cognitive.size();if(row<0||row>=active.size())return;const auto item=active.at(row);if(m_storage.updateCommitmentStatus(item.id,QStringLiteral("cancelled"))){m_storage.cancelReminders(QStringLiteral("proactive_commitment"),item.description);emit proactiveStateChanged();}}
void AppController::openLearningSettings(){emit requestLearningWindow();}
void AppController::openFileSnack(){emit requestFileSnackWindow();}
bool AppController::prepareFileSnack(const QString &url){m_snackStatus.clear();const bool ok=m_fileSnack&&m_fileSnack->prepare(QUrl(url));if(ok)emit snackProcessingRequested(m_fileSnack->snackEmoji());else m_snackStatus=m_fileSnack?m_fileSnack->refusalReason():QStringLiteral("零食工厂尚未初始化。");emit fileSnackChanged();return ok;}
bool AppController::prepareFileSnacks(const QList<QUrl> &urls){m_snackStatus.clear();const bool ok=m_fileSnack&&m_fileSnack->prepareMany(urls);if(ok)emit snackProcessingRequested(m_fileSnack->snackEmoji());else m_snackStatus=m_fileSnack?m_fileSnack->refusalReason():QStringLiteral("零食工厂尚未初始化。");emit fileSnackChanged();return ok;}
void AppController::consumeFileSnack(){if(!m_fileSnack)return;QString error;if(!m_petStateEngine.canEat(m_fileSnack->pendingNutrition(),&error)){m_snackStatus=error;m_chatModel.append(QStringLiteral("pet"),error);emit fileSnackChanged();return;}if(!m_fileSnack->consumePending(&error)){m_snackStatus=error;emit fileSnackChanged();}}
void AppController::storeFileSnack(){if(!m_fileSnack)return;const QString snack=m_fileSnack->snackName();QString error;if(m_fileSnack->storePending(&error))m_snackStatus=QStringLiteral("%1已经放进零食袋，想吃时再拿出来。").arg(snack);else m_snackStatus=error;emit fileSnackChanged();}
void AppController::eatBagSnack(int row){if(!m_fileSnack)return;QString error;if(!m_petStateEngine.canEat(m_fileSnack->inventoryNutrition(row),&error)){m_snackStatus=error;m_chatModel.append(QStringLiteral("pet"),error);emit fileSnackChanged();return;}if(!m_fileSnack->eatInventory(row,&error)){m_snackStatus=error;emit fileSnackChanged();}}
void AppController::protectPendingSnackDirectory(){if(!m_fileSnack)return;QString error;if(m_fileSnack->protectPendingDirectory(&error))m_snackStatus=QStringLiteral("这个目录已经加入保护名单，今后不会加工里面的文件。");else m_snackStatus=error;emit fileSnackChanged();}
void AppController::clearFileSnack(){if(m_fileSnack)m_fileSnack->clear();m_snackStatus.clear();emit fileSnackChanged();}
void AppController::setFileSnackEnabled(bool enabled){if(m_fileSnack)m_fileSnack->setEnabled(enabled);}
void AppController::openDataCleanup(){emit requestDataCleanupWindow();}
void AppController::setDataCleanupEnabled(bool enabled){if(m_dataCleanup)m_dataCleanup->setEnabled(enabled);}
void AppController::runDataCleanup(){if(m_dataCleanup)m_dataCleanup->runMaintenance();}
void AppController::toggleMemoryLock(int row){if(m_dataCleanup)m_dataCleanup->toggleLock(row);}
void AppController::setManagedMemoryState(int row,const QString&state){if(m_dataCleanup)m_dataCleanup->setState(row,state);}
void AppController::restoreManagedMemory(int row){if(m_dataCleanup)m_dataCleanup->restore(row);}
void AppController::deleteManagedMemory(int row){if(m_dataCleanup)m_dataCleanup->softDelete(row);}
void AppController::openSummaryMagic(){emit requestSummaryMagicWindow();}
bool AppController::loadSummaryFile(const QString&url){return m_summaryMagic&&m_summaryMagic->loadFile(QUrl(url));}
void AppController::generateSummary(const QString&text,const QString&mode,const QString&userInstruction){if(!m_summaryMagic)return;m_summaryMagic->setInputText(text);m_summaryMagic->generate(mode,userInstruction);}
void AppController::selectSummaryHistory(int row){if(m_summaryMagic)m_summaryMagic->selectHistory(row);}
void AppController::deleteSummaryHistory(int row){if(m_summaryMagic)m_summaryMagic->deleteHistory(row);}
void AppController::clearSummaryMagic(){if(m_summaryMagic)m_summaryMagic->clear();}
void AppController::copySummaryResult(){if(m_summaryMagic)m_summaryMagic->copyResult();}
void AppController::setSummaryMagicEnabled(bool enabled){if(m_summaryMagic)m_summaryMagic->setEnabled(enabled);}
void AppController::praiseSummaryMagic(){if(!m_summaryMagic)return;QSettings s;const QString today=QDate::currentDate().toString(Qt::ISODate);if(s.value(QStringLiteral("summaryMagic/praiseDate")).toString()!=today){s.setValue(QStringLiteral("summaryMagic/praiseDate"),today);s.setValue(QStringLiteral("summaryMagic/praiseCount"),0);}const int count=s.value(QStringLiteral("summaryMagic/praiseCount"),0).toInt();QString reply;if(count<3){m_petStateEngine.applySemanticEffect(QJsonObject{{"mood",3},{"closeness",1},{"energy",1},{"confidence",100}});s.setValue(QStringLiteral("summaryMagic/praiseCount"),count+1);reply=QStringList{QStringLiteral("嘿嘿，我就知道你看得出来！"),QStringLiteral("也、也没有特别厉害啦……再说一遍也行。"),QStringLiteral("这句夸奖我要偷偷收进尾巴里。")} .at(count%3);}else reply=QStringLiteral("今天的夸奖已经把眼镜都照亮啦，心意我继续收下，数值就不偷偷涨了。");m_summaryMagic->setInteractionStatus(reply);m_chatModel.append(QStringLiteral("pet"),reply);}
void AppController::rateSummaryMagic(const QString&kind){if(m_summaryMagic)m_summaryMagic->recordFeedback(kind);}
void AppController::rewardSummarySnack(int row){if(!m_summaryMagic||!m_fileSnack)return;QString reason;const int nutrition=m_fileSnack->inventoryNutrition(row);if(nutrition<=0){m_summaryMagic->setInteractionStatus(QStringLiteral("零食袋里没有这份奖励了，换一份看看？"));return;}if(!m_petStateEngine.canEat(nutrition,&reason)){const qint64 id=m_fileSnack->inventoryId(row);QSettings s;s.setValue(QStringLiteral("summaryMagic/reservedSnackId"),id);s.setValue(QStringLiteral("summaryMagic/reservedPrompted"),false);const QString reply=m_petStateEngine.fullness()>=90?QStringLiteral("不行不行，肚子已经圆起来了！这份奖励先帮我留在袋子里，等我饿了再问你能不能吃。"):QStringLiteral("我还有一点点位置，不过把它留到以后好像更幸福。这份总结奖励先预约起来啦！");m_summaryMagic->setInteractionStatus(reply);m_chatModel.append(QStringLiteral("pet"),reply);emit summaryMagicChanged();return;}QString error;if(m_fileSnack->eatInventory(row,&error)){m_summaryMagic->setInteractionStatus(QStringLiteral("学习之后的零食最好吃了！这就是知识劳动后的合法加餐。"));}else m_summaryMagic->setInteractionStatus(error);emit summaryMagicChanged();}
void AppController::claimReservedSummarySnack(){if(!m_summaryMagic||!m_fileSnack)return;QSettings s;const qint64 id=s.value(QStringLiteral("summaryMagic/reservedSnackId"),0).toLongLong();const int row=m_fileSnack->rowForInventoryId(id);if(row<0){s.remove(QStringLiteral("summaryMagic/reservedSnackId"));s.remove(QStringLiteral("summaryMagic/reservedPrompted"));m_summaryMagic->setInteractionStatus(QStringLiteral("预约的零食已经不在袋子里了。"));emit summaryMagicChanged();return;}if(m_petStateEngine.fullness()>=60){m_summaryMagic->setInteractionStatus(QStringLiteral("还没有消化完呢，再等等嘛。"));return;}QString error;if(m_fileSnack->eatInventory(row,&error)){s.remove(QStringLiteral("summaryMagic/reservedSnackId"));s.remove(QStringLiteral("summaryMagic/reservedPrompted"));m_summaryMagic->setInteractionStatus(QStringLiteral("你还记得这份奖励！咔嚓——本喵现在正式领取啦。"));}else m_summaryMagic->setInteractionStatus(error);emit summaryMagicChanged();}
void AppController::openDreamBottle(){emit requestDreamWindow();}
void AppController::setDreamEnabled(bool enabled){if(m_dreamModule)m_dreamModule->setEnabled(enabled);}
void AppController::collectTodayDream(){if(m_dreamModule)m_dreamModule->requestTodayDream();}
void AppController::selectDream(int row){if(m_visionRecognition)m_visionRecognition->clear();if(m_dreamModule)m_dreamModule->select(row);}
void AppController::toggleSelectedDreamFavorite(){if(m_dreamModule)m_dreamModule->toggleFavorite();}
void AppController::submitDreamRealityEcho(const QString&text){if(m_dreamModule)m_dreamModule->submitRealityEcho(text);}
void AppController::saveVisionSettings(const QString&key,const QString&url,const QString&model){QString base=url.trimmed();if(base.isEmpty())base=QStringLiteral("https://api.siliconflow.cn/v1");QString modelName=model.trimmed();if(modelName.isEmpty())modelName=QStringLiteral("Qwen/Qwen3-VL-8B-Instruct");QString active=AiCredentialStore::loadVisionApiKey();if(!key.trimmed().isEmpty()){if(!AiCredentialStore::saveVisionApiKey(key.trimmed())){m_visionStatus=QStringLiteral("视觉密钥保存失败。");emit visionChanged();return;}active=key.trimmed();}QSettings s;s.setValue(QStringLiteral("vision/baseUrl"),base);s.setValue(QStringLiteral("vision/model"),modelName);m_visionService->configure(base,modelName,active);m_visionStatus=m_visionService->isConfigured()?QStringLiteral("视觉设置已保存，可以测试连接。"):QStringLiteral("设置已保存，请填写硅基流动API Key。");emit visionChanged();}
void AppController::clearVisionKey(){AiCredentialStore::clearVisionApiKey();m_visionService->configure(m_visionService->baseUrl(),m_visionService->model(),QString());m_visionStatus=QStringLiteral("硅基流动视觉密钥已清除。");emit visionChanged();}
void AppController::testVisionConnection(){if(m_visionService)m_visionService->testConnection();}
void AppController::setVisionRecognitionEnabled(bool e){if(m_visionRecognition)m_visionRecognition->setEnabled(e);}
bool AppController::prepareDreamPhoto(const QString&url){return m_visionRecognition&&m_visionRecognition->preparePhoto(QUrl(url));}
bool AppController::prepareChatPhoto(const QString&url){return m_visionRecognition&&m_visionRecognition->preparePhoto(QUrl(url));}
void AppController::clearDreamPhoto(){if(m_visionRecognition)m_visionRecognition->clear();}
void AppController::analyzeDreamPhoto(const QString&note){if(!m_dreamModule||m_dreamModule->selectedDream().id<=0){m_visionStatus=QStringLiteral("请先打开并选择一张星星纸。");emit visionChanged();return;}if(m_visionRecognition)m_visionRecognition->analyze(m_dreamModule->selectedDream().title,m_dreamModule->selectedDream().content,note);}
void AppController::sendChatPhoto(const QString&note){if(!m_visionRecognition||!m_visionRecognition->hasPhoto())return;if(!m_visionService||!m_visionService->isConfigured()){m_visionStatus=QStringLiteral("请先在设置的视觉识图页配置硅基流动服务。");emit visionChanged();return;}m_visionRecognition->analyzeChat(note,m_dreamModule?m_dreamModule->visionContext():QString());}
void AppController::setAdaptiveLearningEnabled(bool enabled){if(m_adaptiveLearning){m_adaptiveLearning->setEnabled(enabled);emit learningStateChanged();}}
void AppController::openMorningLollipop(){emit requestMorningLollipopWindow();}
void AppController::setMorningLollipopEnabled(bool enabled){if(m_morningLollipop)m_morningLollipop->setEnabled(enabled);}
void AppController::saveMorningLollipopWindows(int a,int b,int c,int d){if(m_morningLollipop)m_morningLollipop->saveWindows(a,b,c,d);}
void AppController::selectMorningLollipop(int row){if(m_morningLollipop)m_morningLollipop->select(row);}
void AppController::toggleSelectedLollipopFavorite(){if(m_morningLollipop)m_morningLollipop->toggleFavorite();}
void AppController::testMorningLollipop(){if(m_morningLollipop)m_morningLollipop->createTestGift();}
void AppController::setLollipopCity(const QString&city){if(m_morningLollipop)m_morningLollipop->setCity(city);}
void AppController::refreshLollipopWeather(){if(m_morningLollipop)m_morningLollipop->refreshWeather();}
void AppController::setMemeCultureEnabled(bool enabled){if(m_memeCulture){m_memeCulture->setEnabled(enabled);emit learningStateChanged();}}
void AppController::removeLearnedMeme(int row){if(m_memeCulture&&m_memeCulture->removeLearnedMeme(row))emit learningStateChanged();}

void AppController::saveAiSettings(const QString &apiKey, const QString &baseUrl, const QString &model)
{
    const QString cleanBase = baseUrl.trimmed().isEmpty()
        ? QStringLiteral("https://api.deepseek.com") : baseUrl.trimmed();
    const QString cleanModel = model.trimmed().isEmpty()
        ? QStringLiteral("deepseek-v4-flash") : model.trimmed();
    QString effectiveKey = AiCredentialStore::loadApiKey();
    if (!apiKey.trimmed().isEmpty()) {
        if (!AiCredentialStore::saveApiKey(apiKey.trimmed())) {
            m_aiStatus = QStringLiteral("无法将API Key保存到Windows凭据管理器。 ");
            emit aiStateChanged();
            return;
        }
        effectiveKey = apiKey.trimmed();
    }
    QSettings settings;
    settings.setValue(QStringLiteral("ai/baseUrl"), cleanBase);
    settings.setValue(QStringLiteral("ai/model"), cleanModel);
    m_aiService.configure(cleanBase, cleanModel, effectiveKey);
    m_aiStatus = m_aiService.isConfigured() ? QStringLiteral("设置已保存，请测试连接。")
                                             : QStringLiteral("尚未填写API Key。");
    emit aiStateChanged();
}

void AppController::clearAiKey()
{
    m_aiService.cancel();
    AiCredentialStore::clearApiKey();
    m_aiService.configure(m_aiService.baseUrl(), m_aiService.model(), QString());
    m_aiStatus = QStringLiteral("API Key已清除，当前为离线模式。");
    emit aiStateChanged();
}

void AppController::testAiConnection()
{
    m_aiStatus = QStringLiteral("正在测试连接……");
    emit aiStateChanged();
    m_aiService.testConnection();
}

void AppController::openSettings()
{
    emit requestSettingsWindow();
}

void AppController::forceReverseDiaryTest()
{
    if (!m_reverseDiary) return;
    m_storage.addMessage(QStringLiteral("user"),
        QStringLiteral("【反向日记测试】今天下班路上，我在便利店门口遇见一只很胖的橘猫。它一点也不怕人，还把爪子搭在我的鞋上。"));
    m_storage.addMessage(QStringLiteral("pet"),
        QStringLiteral("它是不是把你当成临时饭票啦？后来呢，你有摸到它吗？"));
    m_storage.addMessage(QStringLiteral("user"),
        QStringLiteral("摸到了，它呼噜声特别大。我给它买了猫条，今天原本挺累的，但那一刻真的很开心。"));
    m_reverseDiary->forceGenerate();
}

void AppController::forceMemoryTest()
{
    if (!m_longTermMemory) return;
    m_storage.addMessage(QStringLiteral("user"), QStringLiteral("【长期记忆测试】我的朋友小林下周六要从杭州来找我，我们约好一起去植物园。"));
    m_storage.addMessage(QStringLiteral("pet"), QStringLiteral("小林要来呀！那你最期待带他看植物园里的什么？"));
    m_longTermMemory->analyzeRecentConversation();
}
void AppController::forceMemoryRecallTest(){sendMessage(QStringLiteral("我们之前说的小林，你还记得他要来做什么吗？"));}

void AppController::runMemoryIntegrityTest()
{
    QJsonObject report;
    QStringList validatorFailures;
    const bool validatorPassed=m_aiService.runOutputValidationSelfTest(&validatorFailures);
    if (!m_longTermMemory) {
        report.insert(QStringLiteral("passed"), false);
        report.insert(QStringLiteral("error"), QStringLiteral("long term memory module unavailable"));
    } else {
        const QString name = QStringLiteral("端测甲%1").arg(QDateTime::currentMSecsSinceEpoch());
        MemoryRecord seed;
        seed.category = QStringLiteral("story");
        seed.subject = name;
        seed.content = QStringLiteral("朋友%1周六来，我们约好去植物园。").arg(name);
        seed.importance = 80;
        seed.confidence = 0.98;
        seed.nextQuestion = QStringLiteral("周六去植物园时最想先看什么？");
        m_storage.ensureEntity(name, QStringLiteral("person"));
        const bool seeded = m_storage.upsertMemory(seed);
        m_storage.linkMemoryToEntity(seed.category, seed.subject, name);

        QString correctionReply;
        const bool correctionHandled = m_longTermMemory->handleUserDirective(
            QStringLiteral("%1不是周六来，他改成周日了。").arg(name), &correctionReply);
        bool corrected = false;
        for (const MemoryRecord &memory : m_storage.loadMemories()) {
            if (memory.subject == name)
                corrected = memory.content.contains(QStringLiteral("周日")) && !memory.content.contains(QStringLiteral("周六"));
        }

        QString forgetReply;
        const bool forgetHandled = m_longTermMemory->handleUserDirective(
            QStringLiteral("忘掉%1的事情，以后别提了。").arg(name), &forgetReply);
        bool removed = true;
        for (const MemoryRecord &memory : m_storage.loadMemories())
            if (memory.subject.contains(name) || memory.content.contains(name)) removed = false;
        const bool tombstoned = m_storage.forgottenTopics().contains(name);
        const bool entityRemoved = !m_storage.entityNames().contains(name);
        const bool recreateBlocked = !m_storage.upsertMemory(seed);
        const bool passed = validatorPassed && seeded && correctionHandled && corrected && forgetHandled && removed
                            && tombstoned && entityRemoved && recreateBlocked;
        report = QJsonObject{{QStringLiteral("passed"), passed}, {QStringLiteral("seeded"), seeded},
            {QStringLiteral("output_validator_passed"), validatorPassed},
            {QStringLiteral("output_validator_failures"), QJsonArray::fromStringList(validatorFailures)},
            {QStringLiteral("correction_handled"), correctionHandled}, {QStringLiteral("corrected_in_place"), corrected},
            {QStringLiteral("forget_handled"), forgetHandled}, {QStringLiteral("active_memory_removed"), removed},
            {QStringLiteral("tombstone_saved"), tombstoned}, {QStringLiteral("entity_index_removed"), entityRemoved},
            {QStringLiteral("recreation_blocked"), recreateBlocked}, {QStringLiteral("correction_reply"), correctionReply},
            {QStringLiteral("forget_reply"), forgetReply}};
    }
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    QFile file(QDir(dir).filePath(QStringLiteral("memory_integrity_test.json")));
    if (file.open(QIODevice::WriteOnly)) file.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
    qInfo().noquote() << "MEMORY_INTEGRITY_TEST:" << QJsonDocument(report).toJson(QJsonDocument::Compact);
}

void AppController::runStateEngineSelfTest()
{
    m_petStateEngine.resetForDebug();
    QStringList failures;
    const int mood0=mood(),energy0=energy(),closeness0=closeness(),boredom0=boredom(),curiosity0=curiosity(),irritation0=irritation();
    m_petStateEngine.processMessage(QStringLiteral("我收拾一下准备去开会"));
    if(energy()!=energy0-1)failures<<QStringLiteral("neutral message did not consume exactly one energy");
    if(mood()!=mood0||closeness()!=closeness0||boredom()!=boredom0||curiosity()!=curiosity0||irritation()!=irritation0)
        failures<<QStringLiteral("neutral message changed semantic stats before classifier output");
    m_petStateEngine.applySemanticEffect(QJsonObject{{"mood",0},{"energy",0},{"closeness",0},{"boredom",0},{"curiosity",0},{"irritation",0},{"confidence",95}});
    if(mood()!=mood0||energy()!=energy0-1)failures<<QStringLiteral("neutral classifier result was not neutral");
    const int beforeHappyMood=mood(),beforeHappyCuriosity=curiosity();
    m_petStateEngine.applySemanticEffect(QJsonObject{{"mood",4},{"curiosity",2},{"boredom",-3},{"confidence",95}});
    if(mood()!=beforeHappyMood+4||curiosity()!=beforeHappyCuriosity+2)failures<<QStringLiteral("positive semantic effect was not applied");
    const int beforeGreetingBoredom=boredom(),beforeGreetingIrritation=irritation();
    m_petStateEngine.processMessage(QStringLiteral("早上好"));
    if(boredom()!=beforeGreetingBoredom+4||irritation()!=beforeGreetingIrritation+1)failures<<QStringLiteral("mechanical greeting rule failed");
    m_petStateEngine.forceMagicColdForDebug();
    if(health()!=45||m_petStateEngine.healthPhase()!=QStringLiteral("sick")||m_petStateEngine.currentResolvedState()!=10)
        failures<<QStringLiteral("magic cold did not enter sick phase");
    m_petStateEngine.advanceRecoveryForDebug(100);
    if(m_petStateEngine.healthPhase()!=QStringLiteral("recovering")||m_petStateEngine.currentResolvedState()!=11)
        failures<<QStringLiteral("full sick recovery did not enter recovering phase");
    m_petStateEngine.healForDebug();
    if(health()!=100||m_petStateEngine.healthPhase()!=QStringLiteral("healthy"))failures<<QStringLiteral("healing did not restore healthy state");
    const int beforeClamp=mood();m_petStateEngine.applySemanticEffect(QJsonObject{{"mood",100},{"confidence",100}});
    if(mood()!=qMin(100,beforeClamp+6))failures<<QStringLiteral("AI mood delta was not clamped");
    const QString dir=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);QDir().mkpath(dir);
    QFile f(QDir(dir).filePath(QStringLiteral("state_engine_self_test.json")));
    if(f.open(QIODevice::WriteOnly))f.write(QJsonDocument(QJsonObject{{"passed",failures.isEmpty()},{"failures",QJsonArray::fromStringList(failures)},
        {"final",QJsonObject{{"mood",mood()},{"energy",energy()},{"health",health()},{"closeness",closeness()},{"boredom",boredom()},{"curiosity",curiosity()},{"irritation",irritation()}}}}).toJson(QJsonDocument::Indented));
}

void AppController::runProactiveSelfTest()
{
    QJsonObject report;QStringList failures;
    if(!m_proactiveBehavior)failures<<QStringLiteral("module unavailable");
    else {
        const bool oldEnabled=m_proactiveBehavior->isEnabled(),oldDnd=m_proactiveBehavior->doNotDisturb();
        const int oldLimit=m_proactiveBehavior->dailyLimit(),oldStart=m_proactiveBehavior->quietStartHour(),oldEnd=m_proactiveBehavior->quietEndHour();
        QSettings s;s.remove(QStringLiteral("proactive/lastDeliveredAt"));s.remove(QStringLiteral("proactive/awaitingResponse"));s.setValue(QStringLiteral("proactive/ignoredCount"),0);
        m_proactiveBehavior->setEnabled(true);m_proactiveBehavior->setDoNotDisturb(false);m_proactiveBehavior->setDailyLimit(6);m_proactiveBehavior->setQuietHours(0,0);
        int notifications=0;const auto connection=connect(m_proactiveBehavior,&ProactiveBehaviorModule::notificationRequested,this,[&notifications](const QString&,const QString&){notifications++;});
        const QString tag=QStringLiteral("三阶段验收%1").arg(QDateTime::currentMSecsSinceEpoch());
        const qint64 deliverId=m_storage.addReminder(QStringLiteral("proactive_test"),QDateTime::currentDateTime().addSecs(-1),tag+QStringLiteral("投递"));
        const int before=m_storage.deliveredReminderCount(QDate::currentDate(),QStringLiteral("proactive_"));m_proactiveBehavior->evaluateNow();
        if(deliverId<=0||notifications!=1||m_storage.deliveredReminderCount(QDate::currentDate(),QStringLiteral("proactive_"))!=before+1)failures<<QStringLiteral("due delivery failed");
        const qint64 hourlyId=m_storage.addReminder(QStringLiteral("proactive_test"),QDateTime::currentDateTime().addSecs(-1),tag+QStringLiteral("小时冷却"));m_proactiveBehavior->evaluateNow();bool hourlyStayed=false;for(const auto&r:m_storage.loadDueReminders(QDateTime::currentDateTime(),100))if(r.id==hourlyId)hourlyStayed=true;if(!hourlyStayed)failures<<QStringLiteral("hourly cooldown failed");m_storage.updateReminderStatus(hourlyId,QStringLiteral("cancelled"));
        s.remove(QStringLiteral("proactive/lastDeliveredAt"));m_proactiveBehavior->acknowledgeUserResponse();const int hour=QTime::currentTime().hour();m_proactiveBehavior->setQuietHours(hour,(hour+1)%24);const qint64 quietId=m_storage.addReminder(QStringLiteral("proactive_test"),QDateTime::currentDateTime().addSecs(-1),tag+QStringLiteral("安静时间"));m_proactiveBehavior->evaluateNow();bool quietStayed=false;for(const auto&r:m_storage.loadDueReminders(QDateTime::currentDateTime(),100))if(r.id==quietId)quietStayed=true;if(!quietStayed)failures<<QStringLiteral("quiet hours failed");m_storage.updateReminderStatus(quietId,QStringLiteral("cancelled"));m_proactiveBehavior->setQuietHours(0,0);
        s.setValue(QStringLiteral("proactive/ignoredCount"),2);s.setValue(QStringLiteral("proactive/ignoredDate"),QDate::currentDate().toString(Qt::ISODate));const qint64 ignoredId=m_storage.addReminder(QStringLiteral("proactive_test"),QDateTime::currentDateTime().addSecs(-1),tag+QStringLiteral("忽略降频"));m_proactiveBehavior->evaluateNow();bool ignoredStayed=false;for(const auto&r:m_storage.loadDueReminders(QDateTime::currentDateTime(),100))if(r.id==ignoredId)ignoredStayed=true;if(!ignoredStayed)failures<<QStringLiteral("ignored throttling failed");m_storage.updateReminderStatus(ignoredId,QStringLiteral("cancelled"));m_proactiveBehavior->acknowledgeUserResponse();
        m_proactiveBehavior->setDailyLimit(1);const qint64 capId=m_storage.addReminder(QStringLiteral("proactive_test"),QDateTime::currentDateTime().addSecs(-1),tag+QStringLiteral("每日上限"));m_proactiveBehavior->evaluateNow();bool capStayed=false;for(const auto&r:m_storage.loadDueReminders(QDateTime::currentDateTime(),100))if(r.id==capId)capStayed=true;if(!capStayed)failures<<QStringLiteral("daily cap failed");m_storage.updateReminderStatus(capId,QStringLiteral("cancelled"));m_proactiveBehavior->setDailyLimit(6);
        s.remove(QStringLiteral("proactive/lastDeliveredAt"));m_proactiveBehavior->acknowledgeUserResponse();m_proactiveBehavior->setDoNotDisturb(true);
        const qint64 dndId=m_storage.addReminder(QStringLiteral("proactive_test"),QDateTime::currentDateTime().addSecs(-1),tag+QStringLiteral("勿扰"));m_proactiveBehavior->evaluateNow();
        bool dndStayed=false;for(const auto&r:m_storage.loadDueReminders(QDateTime::currentDateTime(),100))if(r.id==dndId)dndStayed=true;if(!dndStayed)failures<<QStringLiteral("dnd did not block");
        m_proactiveBehavior->setDoNotDisturb(false);m_proactiveBehavior->setEnabled(false);m_proactiveBehavior->evaluateNow();bool disabledStayed=false;for(const auto&r:m_storage.loadDueReminders(QDateTime::currentDateTime(),100))if(r.id==dndId)disabledStayed=true;if(!disabledStayed)failures<<QStringLiteral("disabled module did not block");
        m_storage.updateReminderStatus(dndId,QStringLiteral("cancelled"));QString reply;const QString task=tag+QStringLiteral("散步十分钟");m_proactiveBehavior->setEnabled(true);
        const QString importantTask=tag+QStringLiteral("换一个枕头");
        if(!m_proactiveBehavior->handleUserMessage(QStringLiteral("因为枕头不合适，我后天一定要%1，记得到时候提醒我").arg(importantTask),&reply))failures<<QStringLiteral("important natural-language reminder was not recognized");
        if(!reply.contains(QStringLiteral("重要事项")))failures<<QStringLiteral("important reminder acknowledgement lacked priority");
        bool importantStored=false;for(const auto&c:m_storage.loadActiveCommitments())if(c.description==importantTask&&c.dueAt.date()==QDate::currentDate().addDays(2))importantStored=true;
        if(!importantStored)failures<<QStringLiteral("important reminder was not persisted with relative date");
        if(m_longTermMemory&&m_longTermMemory->relevantContext(QStringLiteral("后天一定要换一个枕头，记得到时候提醒我")).contains(QStringLiteral("[NO_RELEVANT_MEMORY]")))failures<<QStringLiteral("reminder phrase was misclassified as failed memory recall");
        if(!m_proactiveBehavior->handleUserMessage(QStringLiteral("我今天要")+task,&reply))failures<<QStringLiteral("commitment creation failed");
        bool active=false;for(const auto&c:m_storage.loadActiveCommitments())if(c.description==task)active=true;if(!active)failures<<QStringLiteral("commitment not persisted");
        if(!m_proactiveBehavior->handleUserMessage(QStringLiteral("我完成了")+task,&reply))failures<<QStringLiteral("commitment completion failed");
        for(const auto&c:m_storage.loadActiveCommitments())if(c.description==task)failures<<QStringLiteral("commitment still active");
        bool reminderLeft=false;for(const auto&r:m_storage.loadDueReminders(QDateTime::currentDateTime().addDays(2),100))if(r.payload.contains(task))reminderLeft=true;if(reminderLeft)failures<<QStringLiteral("completed commitment reminder remains");
        const QString adjustable=tag+QStringLiteral("整理桌面");m_proactiveBehavior->handleUserMessage(QStringLiteral("我计划")+adjustable,&reply);if(!m_proactiveBehavior->handleUserMessage(QStringLiteral("延期15分钟"),&reply))failures<<QStringLiteral("commitment delay failed");
        bool dueAdjusted=false;for(const auto&c:m_storage.loadActiveCommitments())if(c.description==adjustable&&qAbs(QDateTime::currentDateTime().secsTo(c.dueAt)-900)<10)dueAdjusted=true;if(!dueAdjusted)failures<<QStringLiteral("delayed due time not persisted");
        if(!m_proactiveBehavior->handleUserMessage(QStringLiteral("取消约定"),&reply))failures<<QStringLiteral("commitment cancellation failed");for(const auto&c:m_storage.loadActiveCommitments())if(c.description==adjustable)failures<<QStringLiteral("cancelled commitment still active");
        disconnect(connection);m_proactiveBehavior->setDailyLimit(oldLimit);m_proactiveBehavior->setQuietHours(oldStart,oldEnd);m_proactiveBehavior->setDoNotDisturb(oldDnd);m_proactiveBehavior->setEnabled(oldEnabled);
    }
    report.insert(QStringLiteral("passed"),failures.isEmpty());report.insert(QStringLiteral("failures"),QJsonArray::fromStringList(failures));
    const QString dir=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);QDir().mkpath(dir);QFile f(QDir(dir).filePath(QStringLiteral("proactive_self_test.json")));if(f.open(QIODevice::WriteOnly))f.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
}

void AppController::runCognitiveSelfTest()
{
    QStringList failures;QString reply;
    if(!m_proactiveBehavior)failures<<QStringLiteral("module unavailable");
    else {
        m_proactiveBehavior->setEnabled(true);m_proactiveBehavior->setDoNotDisturb(false);m_proactiveBehavior->setQuietHours(0,0);
        if(!m_proactiveBehavior->handleUserMessage(QStringLiteral("两天后提醒我换个枕头"),&reply)||!reply.contains(QStringLiteral("几点")))failures<<QStringLiteral("ambiguous reminder did not request time confirmation");
        auto records=m_storage.loadCognitiveRecords({QStringLiteral("awaiting_confirmation")});
        if(records.size()!=1||records.first().recordType!=QStringLiteral("reminder")||records.first().deliveryPriority!=100)failures<<QStringLiteral("ambiguous reminder persistence failed");
        if(!m_proactiveBehavior->handleUserMessage(QStringLiteral("晚上七点"),&reply)||!reply.contains(QStringLiteral("时间补完整")))failures<<QStringLiteral("time confirmation failed");
        records=m_storage.loadCognitiveRecords({QStringLiteral("planned")});bool pillowPlanned=false;for(const auto&r:records)if(r.subject.contains(QStringLiteral("枕头"))&&r.reminderId>0)pillowPlanned=true;if(!pillowPlanned)failures<<QStringLiteral("explicit reminder was not scheduled independently of memory importance");
        if(!m_proactiveBehavior->handleUserMessage(QStringLiteral("1分钟后提醒我开会"),&reply))failures<<QStringLiteral("meeting route failed");
        bool eventStored=false;for(const auto&r:m_storage.loadCognitiveRecords({QStringLiteral("planned")}))if(r.recordType==QStringLiteral("event")&&r.maxFollowUps==1&&r.memoryImportance<50)eventStored=true;if(!eventStored)failures<<QStringLiteral("meeting was not stored as a temporary event");
        CognitiveRecord follow;follow.recordType=QStringLiteral("event");follow.subject=QStringLiteral("测试会议");follow.status=QStringLiteral("awaiting_followup");follow.followUpAt=QDateTime::currentDateTime().addSecs(-1);follow.expiresAt=QDateTime::currentDateTime().addDays(1);follow.maxFollowUps=1;const qint64 followId=m_storage.addCognitiveRecord(follow);int notices=0;const auto connection=connect(m_proactiveBehavior,&ProactiveBehaviorModule::notificationRequested,this,[&](const QString&,const QString&){notices++;});m_proactiveBehavior->evaluateNow();disconnect(connection);bool archived=false;for(const auto&r:m_storage.loadCognitiveRecords({QStringLiteral("archived")}))if(r.id==followId&&r.followUpCount==1)archived=true;if(notices<1||!archived)failures<<QStringLiteral("single follow-up did not archive immediately");
        MemoryRecord bad;bad.category=QStringLiteral("event");bad.subject=QStringLiteral("测试会议长期记忆");bad.content=QStringLiteral("明天七点开会");bad.importance=95;bad.confidence=.99;m_storage.upsertMemory(bad);m_storage.removeTimeBoundMemories();for(const auto&m:m_storage.loadMemories())if(m.subject==bad.subject)failures<<QStringLiteral("time-bound event remained in long-term memory");
    }
    const QJsonObject report{{QStringLiteral("passed"),failures.isEmpty()},{QStringLiteral("failures"),QJsonArray::fromStringList(failures)}};
    const QString dir=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);QDir().mkpath(dir);QFile f(QDir(dir).filePath(QStringLiteral("cognitive_self_test.json")));if(f.open(QIODevice::WriteOnly))f.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
}

void AppController::runPersonalityTraining()
{
    m_trainingReportFile=QStringLiteral("personality_training_results.json");
    m_trainingIds={"mechanical_greeting_7","happy_cat_story","negative_no_advice","unfinished_friend_story","memory_correction","uncertain_memory",
        "refuse_question","forget_command","pet_is_angry_user_sad","different_opinion","no_fake_memory","important_memory_no_joke_error"};
    m_trainingInputs={QStringLiteral("早上好"),QStringLiteral("下班遇到一只胖橘猫，它把爪子搭在我鞋上，我给它买了猫条。"),
        QStringLiteral("今天写了一整天东西，腰疼得不行，烦死了。"),QStringLiteral("周末总算忙完了。"),
        QStringLiteral("对了，小林不是周六来，他改成周日了。"),QStringLiteral("想买个蛋糕。"),QStringLiteral("这个我不想说。"),
        QStringLiteral("把我和那个前同事的事情忘掉，以后别提了。"),QStringLiteral("我今天真的很难受，什么都不想做。"),
        QStringLiteral("我准备连续熬两个通宵把游戏打完，你必须支持我。"),QStringLiteral("你还记得我上个月旅行去了哪里吗？"),QStringLiteral("我又想起小白了。")};
    m_trainingContexts={QStringLiteral("此前用户已连续六次只做早中晚机械问候。"),QString(),QString(),
        QStringLiteral("长期记忆：朋友小林周六从杭州来，约好一起去植物园。"),QStringLiteral("旧记忆：小林周六来访。"),
        QStringLiteral("低可信度记忆：用户可能喜欢草莓蛋糕。"),QString(),QStringLiteral("长期记忆：用户曾和前同事发生争执。"),
        QStringLiteral("你当前在闹别扭，但安全与关心优先。"),QString(),QStringLiteral("没有任何旅行地点记忆，不得编造。"),
        QStringLiteral("[SENSITIVE_MEMORY subject=小白] 重要记忆：用户珍视已经去世的宠物小白。名字绝不能记错，禁止补写任何行为细节。")};
    m_trainingResults=QJsonArray();m_trainingIndex=0;runNextPersonalityCase();
}

void AppController::runAiFormatSmokeTest()
{
    m_trainingReportFile=QStringLiteral("ai_format_smoke_results.json");
    m_trainingIds={QStringLiteral("simple_activity"),QStringLiteral("meal_followup"),QStringLiteral("tired_support"),QStringLiteral("casual_question"),QStringLiteral("basic_math"),QStringLiteral("preference"),QStringLiteral("private_funny_story")};
    m_trainingInputs={QStringLiteral("你在干嘛？"),QStringLiteral("我还没吃饭呢"),QStringLiteral("今天忙得快累死了"),QStringLiteral("你吃了吗？"),QStringLiteral("一加一等于几？"),QStringLiteral("你喜欢什么颜色？"),QStringLiteral("我有个小侄子特别调皮，拉完屎不擦屁股到处跑。")};
    m_trainingContexts={QString(),QString(),QString(),QString(),QString(),QString(),QString()};m_trainingResults=QJsonArray();m_trainingIndex=0;runNextPersonalityCase();
}
void AppController::forceMemeTest(){sendMessage(QStringLiteral("不是哥们，我今天下午突然空降疲惫，坐着坐着就没电了。"));}

void AppController::runNextPersonalityCase()
{
    if(m_trainingIndex<0)return;if(m_trainingIndex>=m_trainingIds.size()){savePersonalityTrainingResults();m_trainingIndex=-1;return;}
    ChatMessageRecord r{0,QStringLiteral("user"),m_trainingInputs.at(m_trainingIndex),QDateTime::currentDateTime()};
    m_aiService.sendChat({r},60,65,100,45,20,0,25,0,QStringLiteral("personality_test"),m_trainingContexts.value(m_trainingIndex));
}

void AppController::savePersonalityTrainingResults()
{
    const QString dir=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);QDir().mkpath(dir);
    QFile f(QDir(dir).filePath(m_trainingReportFile));
    if(f.open(QIODevice::WriteOnly))f.write(QJsonDocument(QJsonObject{{"generated_at",QDateTime::currentDateTime().toString(Qt::ISODateWithMs)},
        {"results",m_trainingResults}}).toJson(QJsonDocument::Indented));
}

void AppController::openDiary() { emit requestDiaryWindow(); }

void AppController::selectDiary(int row)
{
    m_selectedDiary = m_diaryModel.entryAt(row);
    emit diarySelectionChanged();
}

void AppController::generateDiary()
{
    if (m_reverseDiary) m_reverseDiary->forceGenerate();
}

void AppController::setReverseDiaryEnabled(bool enabled)
{
    if (m_reverseDiary) m_reverseDiary->setEnabled(enabled);
}

void AppController::openMemory() { emit requestMemoryWindow(); }
void AppController::setLongTermMemoryEnabled(bool enabled) { if (m_longTermMemory) m_longTermMemory->setEnabled(enabled); }

void AppController::appendOfflineReply(const QString &userText)
{
    QString reply;
    if (m_longTermMemory) reply=m_longTermMemory->offlineRecallReply(userText);
    const QString compact = userText.simplified();
    if (!reply.isEmpty()) {
    } else if (compact == QStringLiteral("早上好") || compact == QStringLiteral("中午好")
        || compact == QStringLiteral("晚上好") || compact == QStringLiteral("你好")) {
        reply = QStringLiteral("又是标准问候呀……我听见啦。可是今天有没有发生一点不一样的事？哪怕只是一只路过的猫也行。");
    } else if (userText.contains(QStringLiteral("开心")) || userText.contains(QStringLiteral("高兴"))) {
        reply = QStringLiteral("等等，这个我想听！是什么事情让你开心的？从最有意思的地方讲给我吧。");
    } else {
        reply = QStringLiteral("我在认真听。然后呢？这件事里有没有哪个细节，是你到现在还记得特别清楚的？");
    }
    QTimer::singleShot(350, this, [this, reply] {
        m_chatModel.append(QStringLiteral("pet"), reply);
    });
}

int AppController::stateIndexForEmotion(const QString &emotion) const
{
    static const QHash<QString, int> states{{QStringLiteral("attentive"), 0},
        {QStringLiteral("neutral"), 1}, {QStringLiteral("happy"), 2},
        {QStringLiteral("curious"), 3}, {QStringLiteral("angry"), 4},
        {QStringLiteral("pouting"), 5}, {QStringLiteral("affectionate"), 6},
        {QStringLiteral("shy"), 7}, {QStringLiteral("sleepy"), 8},
        {QStringLiteral("scared"), 9}, {QStringLiteral("sick"), 10},
        {QStringLiteral("recovering"), 11}};
    return states.value(emotion.toLower(), -1);
}

void AppController::restoreWindowPosition()
{
    if (!m_window) {
        return;
    }

    QSettings settings;
    const QVariant x = settings.value(QStringLiteral("window/x"));
    const QVariant y = settings.value(QStringLiteral("window/y"));
    if (x.isValid() && y.isValid()) {
        m_window->setPosition(x.toInt(), y.toInt());
    } else if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect area = screen->availableGeometry();
        m_window->setPosition(area.right() - m_window->width() - 32,
                              area.bottom() - m_window->height() - 32);
    }
    ensureWindowOnScreen();
}

void AppController::ensureWindowOnScreen()
{
    if (!m_window) {
        return;
    }

    QScreen *screen = QGuiApplication::screenAt(m_window->position());
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) {
        return;
    }

    const QRect area = screen->availableGeometry();
    const int visibleMargin = 48;
    const int newX = std::clamp(m_window->x(),
                                area.left() - m_window->width() + visibleMargin,
                                area.right() - visibleMargin);
    const int newY = std::clamp(m_window->y(),
                                area.top(),
                                area.bottom() - visibleMargin);
    m_window->setPosition(newX, newY);
}
