#pragma once

#include "FeatureModule.h"
#include "../data/DataRepository.h"
#include <QTimer>

class AiService;
class DreamModule final:public FeatureModule
{
    Q_OBJECT
public:
    DreamModule(DataRepository *storage,AiService *ai,QObject *parent=nullptr);
    QString id()const override{return QStringLiteral("dream_system");}
    QString displayName()const override{return QStringLiteral("梦境星星瓶");}
    bool isEnabled()const override{return m_enabled;}
    void setEnabled(bool enabled)override;
    bool busy()const{return m_busy;}
    QString status()const{return m_status;}
    QStringList items()const;
    int unopenedCount()const;
    DreamRecord selectedDream()const;
    void evaluateDailyDream();
    void requestTodayDream();
    void select(int row);
    void toggleFavorite();
    void submitRealityEcho(const QString &text);
    void submitVisualRealityEcho(const QJsonObject &vision,const QString &userNote);
    QString conversationContext(const QString &message,int closeness);
    QString visionContext()const;
signals:
    void changed();
    void enabledChanged(bool enabled);
    void echoResponseReady(const QString &reply);
private:
    void refresh();
    void startGeneration();
    QJsonObject buildContext()const;
    DataRepository*m_storage=nullptr;AiService*m_ai=nullptr;bool m_enabled=true,m_busy=false;QString m_status;QList<DreamRecord>m_dreams;int m_selected=-1;QTimer m_timer;
};
