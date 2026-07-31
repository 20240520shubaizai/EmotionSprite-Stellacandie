#pragma once
#include "FeatureModule.h"
#include <QDate>
#include <QDateTime>
#include <QTimer>
class AiService; class DataRepository;
class ReverseDiaryModule final : public FeatureModule
{
    Q_OBJECT
public:
    ReverseDiaryModule(DataRepository *storage,AiService *ai,QObject *parent=nullptr);
    QString id()const override;QString displayName()const override;bool isEnabled()const override;void setEnabled(bool enabled)override;
    void forceGenerate();void evaluateAutoGeneration();bool isGenerating()const;
signals:
    void diaryGenerated(const QString&content);void diaryFailed(const QString&message);void enabledChanged(bool enabled);void generatingChanged(bool generating);
private:
    QString sanitizeCompanionLanguage(QString content)const;void ensureStickersForDate(const QDate&date);
    void generateForDate(const QDate&date,bool automatic);bool hasEntry(const QDate&date)const;
    DataRepository*m_storage=nullptr;AiService*m_ai=nullptr;bool m_enabled=true,m_generating=false,m_automatic=false;
    QDate m_targetDate;QTimer m_autoTimer;QDateTime m_nextAutoRetry;
};
