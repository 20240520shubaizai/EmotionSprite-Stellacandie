#pragma once

#include "FeatureModule.h"
#include "../data/repositories/ConversationRepository.h"
#include "../data/repositories/DreamRepository.h"
#include "../data/repositories/MemoryRepository.h"
#include "../data/repositories/PetStateRepository.h"
#include <QTimer>

class AiService;
class DreamModule final:public FeatureModule
{
    Q_OBJECT
public:
    DreamModule(ConversationRepository*,MemoryRepository*,PetStateRepository*,DreamRepository*,AiService*,QObject *parent=nullptr);
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
    ConversationRepository*m_conversations=nullptr;MemoryRepository*m_memories=nullptr;PetStateRepository*m_petState=nullptr;DreamRepository*m_storage=nullptr;AiService*m_ai=nullptr;bool m_enabled=true,m_busy=false;QString m_status;QList<DreamRecord>m_dreams;int m_selected=-1;QTimer m_timer;
};
