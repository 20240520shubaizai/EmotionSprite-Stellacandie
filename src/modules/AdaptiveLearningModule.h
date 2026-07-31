#pragma once
#include "FeatureModule.h"
class AdaptiveLearningModule final:public FeatureModule
{
    Q_OBJECT
public:
    explicit AdaptiveLearningModule(QObject *parent=nullptr);
    QString id()const override;QString displayName()const override;bool isEnabled()const override;void setEnabled(bool)override;
    void observeUserResponse(const QString &message,const QString &lastAssistantReply);
    QString context()const;
private:
    void adjust(const QString &key,double delta,const QString &reason);
    double score(const QString &key)const;
    bool m_enabled=true;
};
