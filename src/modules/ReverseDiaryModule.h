#pragma once
#include "FeatureModule.h"
#include <QDate>
#include <QDateTime>
#include <QTimer>
class AiService; class ConversationRepository; class DiaryRepository; class DreamRepository; class PetStateRepository;
class ReverseDiaryModule final : public FeatureModule
{
    Q_OBJECT
public:
    ReverseDiaryModule(ConversationRepository*,DiaryRepository*,DreamRepository*,PetStateRepository*,AiService*,QObject *parent=nullptr);
    QString id()const override;QString displayName()const override;bool isEnabled()const override;void setEnabled(bool enabled)override;
    void forceGenerate();void evaluateAutoGeneration();bool isGenerating()const;
signals:
    void diaryGenerated(const QString&content);void diaryFailed(const QString&message);void enabledChanged(bool enabled);void generatingChanged(bool generating);
private:
    QString sanitizeCompanionLanguage(QString content)const;void ensureStickersForDate(const QDate&date);
    void generateForDate(const QDate&date,bool automatic);bool hasEntry(const QDate&date)const;
    ConversationRepository*m_conversations=nullptr;DiaryRepository*m_storage=nullptr;DreamRepository*m_dreams=nullptr;PetStateRepository*m_petState=nullptr;AiService*m_ai=nullptr;bool m_enabled=true,m_generating=false,m_automatic=false;
    QDate m_targetDate;QTimer m_autoTimer;QDateTime m_nextAutoRetry;
};
