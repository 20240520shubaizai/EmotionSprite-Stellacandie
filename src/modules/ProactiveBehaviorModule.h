#pragma once

#include "FeatureModule.h"
#include "../data/DataRepository.h"
#include <QTimer>

class ProactiveBehaviorModule final : public FeatureModule
{
    Q_OBJECT
public:
    ProactiveBehaviorModule(DataRepository *repository,QObject *parent=nullptr);
    QString id()const override;QString displayName()const override;
    bool isEnabled()const override;void setEnabled(bool enabled)override;
    bool doNotDisturb()const;void setDoNotDisturb(bool enabled);
    int dailyLimit()const;void setDailyLimit(int limit);
    int quietStartHour()const;int quietEndHour()const;void setQuietHours(int startHour,int endHour);
    bool schedule(const QString &type,const QDateTime &when,const QString &message);
    void acknowledgeUserResponse();bool handleUserMessage(const QString &message,QString *reply);void evaluateNow();
signals:
    void enabledChanged(bool enabled);void settingsChanged();void notificationRequested(const QString &title,const QString &message);
private:
    bool isQuietTime(const QDateTime &now)const;
    qint64 scheduleCognitive(CognitiveRecord record,const QString &payload);
    CognitiveRecord bestMatchingActive(const QString &subject)const;
    void seedLifestyleNudge();
    DataRepository *m_repository=nullptr;QTimer m_timer;bool m_enabled=true;bool m_dnd=false;
    int m_dailyLimit=3;int m_quietStart=23;int m_quietEnd=8;
};
