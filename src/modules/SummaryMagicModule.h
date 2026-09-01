#pragma once
#include "FeatureModule.h"
#include "../data/repositories/CollectionRepository.h"
#include <QUrl>
#include <QTimer>

class AiService;
class SummaryMagicModule final:public FeatureModule
{
    Q_OBJECT
public:
    SummaryMagicModule(CollectionRepository *storage,AiService *ai,QObject *parent=nullptr);
    QString id()const override{return QStringLiteral("summary_magic");}QString displayName()const override{return QStringLiteral("AI总结魔法");}
    bool isEnabled()const override{return m_enabled;}void setEnabled(bool enabled)override;
    bool busy()const{return m_busy;}QString sourceName()const{return m_sourceName;}QString sourceInfo()const{return m_sourceInfo;}QString inputText()const{return m_inputText;}QString resultText()const{return m_resultText;}QString status()const{return m_status;}
    QStringList historyItems()const;bool loadFile(const QUrl &url);void setInputText(const QString &text);void generate(const QString &mode,const QString &userInstruction=QString());void selectHistory(int row);void deleteHistory(int row);void clear();void copyResult();void setInteractionStatus(const QString &text);void recordFeedback(const QString &kind);
signals:void changed();void enabledChanged(bool enabled);void studyStarted(const QString &message);void studyFinished(const QString &message);
private:void refreshHistory();QString formatResult(const QJsonObject&o)const;
    CollectionRepository*m_storage=nullptr;AiService*m_ai=nullptr;bool m_enabled=true,m_busy=false;QString m_sourceName,m_sourceInfo,m_inputText,m_resultText,m_status,m_mode;QList<SummaryRecord>m_history;QTimer m_phraseTimer;int m_phraseIndex=0;
};
