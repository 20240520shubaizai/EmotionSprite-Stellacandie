#pragma once

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QJsonArray>
#include <QHash>
#include <QUrl>

#include "ChatMessageModel.h"
#include "StorageService.h"
#include "PetStateEngine.h"
#include "AiService.h"
#include "modules/ModuleManager.h"
#include "modules/DiaryEntryModel.h"
#include "modules/MemoryListModel.h"

class QQuickWindow;
class QSystemTrayIcon;
class QTimer;
class ReverseDiaryModule;
class LongTermMemoryModule;
class ProactiveBehaviorModule;
class MemeCultureModule;
class AdaptiveLearningModule;
class FileSnackModule;
class DataCleanupModule;
class SummaryMagicModule;
class DreamModule;
class VisionService;
class VisionRecognitionModule;
class MorningLollipopModule;
class AgentClient;
class ChatAgentAdapter;

class AppController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int currentStateIndex READ currentStateIndex NOTIFY currentStateChanged)
    Q_PROPERTY(QString currentStateName READ currentStateName NOTIFY currentStateChanged)
    Q_PROPERTY(QString currentImage READ currentImage NOTIFY currentStateChanged)
    Q_PROPERTY(bool alwaysOnTop READ alwaysOnTop WRITE setAlwaysOnTop NOTIFY alwaysOnTopChanged)
    Q_PROPERTY(QAbstractItemModel *chatModel READ chatModel CONSTANT)
    Q_PROPERTY(QString databasePath READ databasePath CONSTANT)
    Q_PROPERTY(int mood READ mood NOTIFY petStatsChanged)
    Q_PROPERTY(int energy READ energy NOTIFY petStatsChanged)
    Q_PROPERTY(int health READ health NOTIFY petStatsChanged)
    Q_PROPERTY(int closeness READ closeness NOTIFY petStatsChanged)
    Q_PROPERTY(int boredom READ boredom NOTIFY petStatsChanged)
    Q_PROPERTY(int neglect READ neglect NOTIFY petStatsChanged)
    Q_PROPERTY(int curiosity READ curiosity NOTIFY petStatsChanged)
    Q_PROPERTY(int irritation READ irritation NOTIFY petStatsChanged)
    Q_PROPERTY(int fullness READ fullness NOTIFY petStatsChanged)
    Q_PROPERTY(QString healthPhaseName READ healthPhaseName NOTIFY petStatsChanged)
    Q_PROPERTY(QString conditionName READ conditionName NOTIFY petStatsChanged)
    Q_PROPERTY(int recoveryProgress READ recoveryProgress NOTIFY petStatsChanged)
    Q_PROPERTY(bool aiConfigured READ aiConfigured NOTIFY aiStateChanged)
    Q_PROPERTY(bool aiBusy READ aiBusy NOTIFY aiStateChanged)
    Q_PROPERTY(QString aiStatus READ aiStatus NOTIFY aiStateChanged)
    Q_PROPERTY(QString aiBaseUrl READ aiBaseUrl NOTIFY aiStateChanged)
    Q_PROPERTY(QString aiModel READ aiModel NOTIFY aiStateChanged)
    Q_PROPERTY(QString chatRouteMode READ chatRouteMode NOTIFY aiStateChanged)
    Q_PROPERTY(QString chatRouteLabel READ chatRouteLabel NOTIFY aiStateChanged)
    Q_PROPERTY(QAbstractItemModel *diaryModel READ diaryModel CONSTANT)
    Q_PROPERTY(int diaryCount READ diaryCount NOTIFY diarySelectionChanged)
    Q_PROPERTY(QString selectedDiaryDate READ selectedDiaryDate NOTIFY diarySelectionChanged)
    Q_PROPERTY(QString selectedDiaryContent READ selectedDiaryContent NOTIFY diarySelectionChanged)
    Q_PROPERTY(QString selectedDiaryUpdatedAt READ selectedDiaryUpdatedAt NOTIFY diarySelectionChanged)
    Q_PROPERTY(QStringList selectedDiaryStickers READ selectedDiaryStickers NOTIFY diarySelectionChanged)
    Q_PROPERTY(bool reverseDiaryEnabled READ reverseDiaryEnabled NOTIFY reverseDiaryStateChanged)
    Q_PROPERTY(bool reverseDiaryGenerating READ reverseDiaryGenerating NOTIFY reverseDiaryStateChanged)
    Q_PROPERTY(QAbstractItemModel *memoryModel READ memoryModel CONSTANT)
    Q_PROPERTY(QStringList memoryItems READ memoryItems NOTIFY memoryStateChanged)
    Q_PROPERTY(bool longTermMemoryEnabled READ longTermMemoryEnabled NOTIFY memoryStateChanged)
    Q_PROPERTY(bool proactiveEnabled READ proactiveEnabled NOTIFY proactiveStateChanged)
    Q_PROPERTY(bool doNotDisturb READ doNotDisturb NOTIFY proactiveStateChanged)
    Q_PROPERTY(int proactiveDailyLimit READ proactiveDailyLimit NOTIFY proactiveStateChanged)
    Q_PROPERTY(int quietStartHour READ quietStartHour NOTIFY proactiveStateChanged)
    Q_PROPERTY(int quietEndHour READ quietEndHour NOTIFY proactiveStateChanged)
    Q_PROPERTY(QStringList activeCommitments READ activeCommitments NOTIFY proactiveStateChanged)
    Q_PROPERTY(bool adaptiveLearningEnabled READ adaptiveLearningEnabled NOTIFY learningStateChanged)
    Q_PROPERTY(bool memeCultureEnabled READ memeCultureEnabled NOTIFY learningStateChanged)
    Q_PROPERTY(QStringList learnedMemes READ learnedMemes NOTIFY learningStateChanged)
    Q_PROPERTY(bool desktopRoaming READ desktopRoaming NOTIFY desktopRoamingChanged)
    Q_PROPERTY(int desktopRoamDirection READ desktopRoamDirection NOTIFY desktopRoamingChanged)
    Q_PROPERTY(QString desktopAnimation READ desktopAnimation NOTIFY desktopRoamingChanged)
    Q_PROPERTY(bool fileSnackEnabled READ fileSnackEnabled NOTIFY fileSnackChanged)
    Q_PROPERTY(bool hasPendingSnack READ hasPendingSnack NOTIFY fileSnackChanged)
    Q_PROPERTY(QString snackFileName READ snackFileName NOTIFY fileSnackChanged)
    Q_PROPERTY(QString snackFileInfo READ snackFileInfo NOTIFY fileSnackChanged)
    Q_PROPERTY(QString snackName READ snackName NOTIFY fileSnackChanged)
    Q_PROPERTY(QString snackEmoji READ snackEmoji NOTIFY fileSnackChanged)
    Q_PROPERTY(QString snackWarning READ snackWarning NOTIFY fileSnackChanged)
    Q_PROPERTY(QString snackStatus READ snackStatus NOTIFY fileSnackChanged)
    Q_PROPERTY(QString snackSourcePath READ snackSourcePath NOTIFY fileSnackChanged)
    Q_PROPERTY(QString snackModifiedText READ snackModifiedText NOTIFY fileSnackChanged)
    Q_PROPERTY(QString snackSafetyLevel READ snackSafetyLevel NOTIFY fileSnackChanged)
    Q_PROPERTY(bool snackStrongConfirmationRequired READ snackStrongConfirmationRequired NOTIFY fileSnackChanged)
    Q_PROPERTY(QStringList snackBagItems READ snackBagItems NOTIFY fileSnackChanged)
    Q_PROPERTY(QStringList snackCatalogItems READ snackCatalogItems NOTIFY fileSnackChanged)
    Q_PROPERTY(QStringList snackHistoryItems READ snackHistoryItems NOTIFY fileSnackChanged)
    Q_PROPERTY(QStringList snackPendingFiles READ snackPendingFiles NOTIFY fileSnackChanged)
    Q_PROPERTY(bool dataCleanupEnabled READ dataCleanupEnabled NOTIFY dataCleanupChanged)
    Q_PROPERTY(QStringList managedMemoryItems READ managedMemoryItems NOTIFY dataCleanupChanged)
    Q_PROPERTY(QString memoryCleanupSummary READ memoryCleanupSummary NOTIFY dataCleanupChanged)
    Q_PROPERTY(QString memoryCleanupResult READ memoryCleanupResult NOTIFY dataCleanupChanged)
    Q_PROPERTY(bool summaryMagicEnabled READ summaryMagicEnabled NOTIFY summaryMagicChanged)
    Q_PROPERTY(bool summaryMagicBusy READ summaryMagicBusy NOTIFY summaryMagicChanged)
    Q_PROPERTY(QString summarySourceName READ summarySourceName NOTIFY summaryMagicChanged)
    Q_PROPERTY(QString summarySourceInfo READ summarySourceInfo NOTIFY summaryMagicChanged)
    Q_PROPERTY(QString summaryInputText READ summaryInputText NOTIFY summaryMagicChanged)
    Q_PROPERTY(QString summaryResult READ summaryResult NOTIFY summaryMagicChanged)
    Q_PROPERTY(QString summaryStatus READ summaryStatus NOTIFY summaryMagicChanged)
    Q_PROPERTY(QStringList summaryHistoryItems READ summaryHistoryItems NOTIFY summaryMagicChanged)
    Q_PROPERTY(bool summaryRewardReserved READ summaryRewardReserved NOTIFY summaryMagicChanged)
    Q_PROPERTY(bool summaryRewardCanClaim READ summaryRewardCanClaim NOTIFY petStatsChanged)
    Q_PROPERTY(bool dreamEnabled READ dreamEnabled NOTIFY dreamChanged)
    Q_PROPERTY(bool dreamBusy READ dreamBusy NOTIFY dreamChanged)
    Q_PROPERTY(QString dreamStatus READ dreamStatus NOTIFY dreamChanged)
    Q_PROPERTY(QStringList dreamItems READ dreamItems NOTIFY dreamChanged)
    Q_PROPERTY(int unopenedDreamCount READ unopenedDreamCount NOTIFY dreamChanged)
    Q_PROPERTY(QString selectedDreamTitle READ selectedDreamTitle NOTIFY dreamChanged)
    Q_PROPERTY(QString selectedDreamDate READ selectedDreamDate NOTIFY dreamChanged)
    Q_PROPERTY(QString selectedDreamContent READ selectedDreamContent NOTIFY dreamChanged)
    Q_PROPERTY(QString selectedDreamSymbols READ selectedDreamSymbols NOTIFY dreamChanged)
    Q_PROPERTY(QString selectedDreamHint READ selectedDreamHint NOTIFY dreamChanged)
    Q_PROPERTY(QString selectedDreamColor READ selectedDreamColor NOTIFY dreamChanged)
    Q_PROPERTY(QString selectedDreamEcho READ selectedDreamEcho NOTIFY dreamChanged)
    Q_PROPERTY(bool selectedDreamFavorite READ selectedDreamFavorite NOTIFY dreamChanged)
    Q_PROPERTY(bool visionConfigured READ visionConfigured NOTIFY visionChanged)
    Q_PROPERTY(bool visionBusy READ visionBusy NOTIFY visionChanged)
    Q_PROPERTY(QString visionStatus READ visionStatus NOTIFY visionChanged)
    Q_PROPERTY(QString visionBaseUrl READ visionBaseUrl NOTIFY visionChanged)
    Q_PROPERTY(QString visionModel READ visionModel NOTIFY visionChanged)
    Q_PROPERTY(bool visionRecognitionEnabled READ visionRecognitionEnabled NOTIFY visionChanged)
    Q_PROPERTY(bool hasPendingVisionPhoto READ hasPendingVisionPhoto NOTIFY visionChanged)
    Q_PROPERTY(QString visionPhotoUrl READ visionPhotoUrl NOTIFY visionChanged)
    Q_PROPERTY(QString visionPhotoName READ visionPhotoName NOTIFY visionChanged)
    Q_PROPERTY(QString visionPhotoStatus READ visionPhotoStatus NOTIFY visionChanged)
    Q_PROPERTY(QString visionResultSummary READ visionResultSummary NOTIFY visionChanged)
    Q_PROPERTY(bool morningLollipopEnabled READ morningLollipopEnabled NOTIFY morningLollipopChanged)
    Q_PROPERTY(QString morningLollipopStatus READ morningLollipopStatus NOTIFY morningLollipopChanged)
    Q_PROPERTY(QStringList morningLollipopItems READ morningLollipopItems NOTIFY morningLollipopChanged)
    Q_PROPERTY(int lollipopWorkdayStart READ lollipopWorkdayStart NOTIFY morningLollipopChanged)
    Q_PROPERTY(int lollipopWorkdayEnd READ lollipopWorkdayEnd NOTIFY morningLollipopChanged)
    Q_PROPERTY(int lollipopWeekendStart READ lollipopWeekendStart NOTIFY morningLollipopChanged)
    Q_PROPERTY(int lollipopWeekendEnd READ lollipopWeekendEnd NOTIFY morningLollipopChanged)
    Q_PROPERTY(QString selectedLollipopDate READ selectedLollipopDate NOTIFY morningLollipopChanged)
    Q_PROPERTY(QString selectedLollipopFlavor READ selectedLollipopFlavor NOTIFY morningLollipopChanged)
    Q_PROPERTY(QString selectedLollipopEmoji READ selectedLollipopEmoji NOTIFY morningLollipopChanged)
    Q_PROPERTY(QString selectedLollipopGreeting READ selectedLollipopGreeting NOTIFY morningLollipopChanged)
    Q_PROPERTY(QString selectedLollipopRarity READ selectedLollipopRarity NOTIFY morningLollipopChanged)
    Q_PROPERTY(bool selectedLollipopFavorite READ selectedLollipopFavorite NOTIFY morningLollipopChanged)
    Q_PROPERTY(QString lollipopCity READ lollipopCity NOTIFY morningLollipopChanged)
    Q_PROPERTY(QString lollipopWeather READ lollipopWeather NOTIFY morningLollipopChanged)
    Q_PROPERTY(QString selectedLollipopType READ selectedLollipopType NOTIFY morningLollipopChanged)
    Q_PROPERTY(QString selectedLollipopStory READ selectedLollipopStory NOTIFY morningLollipopChanged)
    Q_PROPERTY(QString selectedLollipopWeather READ selectedLollipopWeather NOTIFY morningLollipopChanged)
    Q_PROPERTY(QString selectedLollipopColor READ selectedLollipopColor NOTIFY morningLollipopChanged)
    Q_PROPERTY(QString selectedLollipopShape READ selectedLollipopShape NOTIFY morningLollipopChanged)
    Q_PROPERTY(QString selectedLollipopPattern READ selectedLollipopPattern NOTIFY morningLollipopChanged)

public:
    SyncRepository *syncRepository(){return &m_storage;}
    StorageService *storageService(){return &m_storage;}
    explicit AppController(QObject *parent = nullptr);

    int currentStateIndex() const;
    QString currentStateName() const;
    QString currentImage() const;
    bool alwaysOnTop() const;
    QAbstractItemModel *chatModel();
    QString databasePath() const;
    int mood() const;
    int energy() const;
    int health() const;
    int closeness() const;
    int boredom() const;
    int neglect() const;
    int curiosity() const;
    int irritation() const;
    int fullness() const;
    QString healthPhaseName()const; QString conditionName()const; int recoveryProgress()const;
    bool aiConfigured() const;
    bool aiBusy() const;
    QString aiStatus() const;
    QString aiBaseUrl() const;
    QString aiModel() const;
    QString chatRouteMode() const;
    QString chatRouteLabel() const;
    QAbstractItemModel *diaryModel();
    int diaryCount() const;
    QString selectedDiaryDate() const;
    QString selectedDiaryContent() const;
    QString selectedDiaryUpdatedAt() const;
    QStringList selectedDiaryStickers() const;
    bool reverseDiaryEnabled() const;
    bool reverseDiaryGenerating() const;
    QAbstractItemModel *memoryModel();
    QStringList memoryItems() const;
    bool longTermMemoryEnabled() const;
    bool proactiveEnabled() const;
    bool doNotDisturb() const;
    int proactiveDailyLimit() const;
    int quietStartHour() const;
    int quietEndHour() const;
    QStringList activeCommitments() const;
    bool adaptiveLearningEnabled() const;
    bool memeCultureEnabled() const;
    QStringList learnedMemes() const;
    bool desktopRoaming() const { return m_desktopRoaming; }
    int desktopRoamDirection() const { return m_desktopRoamDirection; }
    QString desktopAnimation() const { return m_desktopAnimation; }
    bool fileSnackEnabled()const; bool hasPendingSnack()const; QString snackFileName()const; QString snackFileInfo()const;
    QString snackName()const; QString snackEmoji()const; QString snackWarning()const; QString snackStatus()const{return m_snackStatus;}
    QString snackSourcePath()const; QString snackModifiedText()const; QString snackSafetyLevel()const;
    bool snackStrongConfirmationRequired()const; QStringList snackBagItems()const; QStringList snackCatalogItems()const; QStringList snackHistoryItems()const;
    QStringList snackPendingFiles()const;
    bool dataCleanupEnabled()const;QStringList managedMemoryItems()const;QString memoryCleanupSummary()const;QString memoryCleanupResult()const;
    bool summaryMagicEnabled()const;bool summaryMagicBusy()const;QString summarySourceName()const;QString summarySourceInfo()const;QString summaryInputText()const;QString summaryResult()const;QString summaryStatus()const;QStringList summaryHistoryItems()const;
    bool summaryRewardReserved()const;bool summaryRewardCanClaim()const;
    bool dreamEnabled()const;bool dreamBusy()const;QString dreamStatus()const;QStringList dreamItems()const;int unopenedDreamCount()const;
    QString selectedDreamTitle()const;QString selectedDreamDate()const;QString selectedDreamContent()const;QString selectedDreamSymbols()const;QString selectedDreamHint()const;QString selectedDreamColor()const;QString selectedDreamEcho()const;bool selectedDreamFavorite()const;
    bool visionConfigured()const;bool visionBusy()const;QString visionStatus()const;QString visionBaseUrl()const;QString visionModel()const;bool visionRecognitionEnabled()const;bool hasPendingVisionPhoto()const;QString visionPhotoUrl()const;QString visionPhotoName()const;QString visionPhotoStatus()const;QString visionResultSummary()const;
    bool morningLollipopEnabled()const;QString morningLollipopStatus()const;QStringList morningLollipopItems()const;
    int lollipopWorkdayStart()const;int lollipopWorkdayEnd()const;int lollipopWeekendStart()const;int lollipopWeekendEnd()const;
    QString selectedLollipopDate()const;QString selectedLollipopFlavor()const;QString selectedLollipopEmoji()const;QString selectedLollipopGreeting()const;QString selectedLollipopRarity()const;bool selectedLollipopFavorite()const;
    QString lollipopCity()const;QString lollipopWeather()const;QString selectedLollipopType()const;QString selectedLollipopStory()const;QString selectedLollipopWeather()const;QString selectedLollipopColor()const;QString selectedLollipopShape()const;QString selectedLollipopPattern()const;

    void attachWindow(QQuickWindow *window);
    void createTrayIcon();
    void setAgentClient(AgentClient *client);

public slots:
    void nextState();
    void previousState();
    void setState(int index);
    void saveWindowPosition();
    void setAlwaysOnTop(bool enabled);
    void toggleWindowVisibility();
    void quitApplication();
    void sendMessage(const QString &text);
    void setChatRouteMode(const QString &mode);
    void adjustPetStat(const QString &stat, int delta);
    void resetPetStats();
    void saveAiSettings(const QString &apiKey, const QString &baseUrl, const QString &model);
    void clearAiKey();
    void testAiConnection();
    void openSettings();
    void forceReverseDiaryTest();
    void forceMemoryTest();
    void forceMemoryRecallTest();
    void runPersonalityTraining();
    void runAiFormatSmokeTest();
    void forceMemeTest();
    void runMemoryIntegrityTest();
    void runProactiveSelfTest();
    void runCognitiveSelfTest();
    void runStateEngineSelfTest();
    void openDiary();
    void selectDiary(int row);
    void generateDiary();
    void setReverseDiaryEnabled(bool enabled);
    void openMemory();
    void setLongTermMemoryEnabled(bool enabled);
    void openProactiveSettings();
    void setProactiveEnabled(bool enabled);
    void setDoNotDisturb(bool enabled);
    void saveProactiveSettings(int dailyLimit,int quietStart,int quietEnd);
    void addLifestyleReminder(const QString &message,int minutesFromNow);
    void completeCommitment(int row);
    void cancelCommitment(int row);
    void openLearningSettings();
    void setAdaptiveLearningEnabled(bool enabled);
    void setMemeCultureEnabled(bool enabled);
    void removeLearnedMeme(int row);
    void stopDesktopRoaming();
    void triggerDesktopRunAway(int direction);
    void forceMagicCold();
    void advanceHealthRecovery();
    void healPet();
    void letPetRest();
    void openFileSnack();
    bool prepareFileSnack(const QString &url);
    bool prepareFileSnacks(const QList<QUrl> &urls);
    void consumeFileSnack();
    void storeFileSnack();
    void eatBagSnack(int row);
    void protectPendingSnackDirectory();
    void clearFileSnack();
    void setFileSnackEnabled(bool enabled);
    void openDataCleanup();void setDataCleanupEnabled(bool enabled);void runDataCleanup();void toggleMemoryLock(int row);void setManagedMemoryState(int row,const QString &state);void restoreManagedMemory(int row);void deleteManagedMemory(int row);
    void openSummaryMagic();bool loadSummaryFile(const QString &url);void generateSummary(const QString &text,const QString &mode,const QString &userInstruction);void selectSummaryHistory(int row);void deleteSummaryHistory(int row);void clearSummaryMagic();void copySummaryResult();void setSummaryMagicEnabled(bool enabled);
    void praiseSummaryMagic();void rateSummaryMagic(const QString &kind);void rewardSummarySnack(int row);void claimReservedSummarySnack();
    void openDreamBottle();void setDreamEnabled(bool enabled);void collectTodayDream();void selectDream(int row);void toggleSelectedDreamFavorite();void submitDreamRealityEcho(const QString &text);
    void saveVisionSettings(const QString&apiKey,const QString&baseUrl,const QString&model);void clearVisionKey();void testVisionConnection();void setVisionRecognitionEnabled(bool enabled);bool prepareDreamPhoto(const QString&url);bool prepareChatPhoto(const QString&url);void clearDreamPhoto();void analyzeDreamPhoto(const QString&note);void sendChatPhoto(const QString&note);
    void openMorningLollipop();void setMorningLollipopEnabled(bool enabled);void saveMorningLollipopWindows(int workStart,int workEnd,int weekendStart,int weekendEnd);void selectMorningLollipop(int row);void toggleSelectedLollipopFavorite();void testMorningLollipop();
    void setLollipopCity(const QString&city);void refreshLollipopWeather();

signals:
    void currentStateChanged();
    void alwaysOnTopChanged();
    void petStatsChanged();
    void aiStateChanged();
    void agentConfigurationChanged();
    void requestSettingsWindow();
    void requestDiaryWindow();
    void diarySelectionChanged();
    void reverseDiaryStateChanged();
    void requestMemoryWindow();
    void memoryStateChanged();
    void requestProactiveWindow();
    void requestChatWindow();
    void proactiveStateChanged();
    void requestLearningWindow();
    void learningStateChanged();
    void desktopRoamingChanged();
    void requestFileSnackWindow();
    void fileSnackChanged();
    void requestDataCleanupWindow();void dataCleanupChanged();
    void requestSummaryMagicWindow();void summaryMagicChanged();
    void requestDreamWindow();void dreamChanged();
    void visionChanged();
    void requestMorningLollipopWindow();void morningLollipopChanged();
    void snackProcessingRequested(const QString &snackEmoji);
    void snackEatingRequested(const QString &snackEmoji);

private:
    struct State {
        QString name;
        QString image;
    };

    void restoreWindowPosition();
    void ensureWindowOnScreen();
    void appendOfflineReply(const QString &userText);
    void sendViaSelectedChatRoute(const QString &content, const QString &context,
                                  const QString &attachmentName = QString());
    void sendViaLegacyChat(const QString &content, const QString &context,
                           const QString &fallbackReason = QString());
    int stateIndexForEmotion(const QString &emotion) const;
    void runNextPersonalityCase();
    void savePersonalityTrainingResults();

    QList<State> m_states;
    int m_currentStateIndex = 1;
    bool m_alwaysOnTop = true;
    QPointer<QQuickWindow> m_window;
    QSystemTrayIcon *m_trayIcon = nullptr;
    StorageService m_storage;
    ChatMessageModel m_chatModel;
    PetStateEngine m_petStateEngine;
    AiService m_aiService;
    AgentClient *m_agentClient = nullptr;
    ChatAgentAdapter *m_chatAgent = nullptr;
    QString m_chatRouteMode = QStringLiteral("agent_main");
    QHash<QString, QString> m_agentTexts;
    QHash<QString, QString> m_agentContexts;
    QString m_legacyFallbackReason;
    ModuleManager m_moduleManager;
    ReverseDiaryModule *m_reverseDiary = nullptr;
    DiaryEntryModel m_diaryModel;
    LongTermMemoryModule *m_longTermMemory = nullptr;
    ProactiveBehaviorModule *m_proactiveBehavior = nullptr;
    MemeCultureModule *m_memeCulture = nullptr;
    AdaptiveLearningModule *m_adaptiveLearning = nullptr;
    FileSnackModule *m_fileSnack = nullptr;
    DataCleanupModule *m_dataCleanup = nullptr;
    SummaryMagicModule *m_summaryMagic = nullptr;
    DreamModule *m_dreamModule = nullptr;
    VisionService *m_visionService = nullptr;
    VisionRecognitionModule *m_visionRecognition = nullptr;
    MorningLollipopModule *m_morningLollipop = nullptr;
    QString m_visionStatus=QStringLiteral("视觉服务未配置");
    QString m_snackStatus;
    MemoryListModel m_memoryModel;
    DiaryEntryRecord m_selectedDiary;
    QString m_aiStatus = QStringLiteral("离线模式");
    QString m_pendingUserText;
    QString m_lastAssistantReply;
    QTimer *m_desktopRoamSchedule = nullptr;
    QTimer *m_desktopRoamMove = nullptr;
    bool m_desktopRoaming = false;
    int m_desktopRoamDirection = 1;
    int m_desktopRoamTargetX = 0;
    QString m_desktopAnimation = QStringLiteral("walk");
    QStringList m_trainingIds;
    QStringList m_trainingInputs;
    QStringList m_trainingContexts;
    QJsonArray m_trainingResults;
    int m_trainingIndex = -1;
    QString m_trainingReportFile = QStringLiteral("personality_training_results.json");
};
