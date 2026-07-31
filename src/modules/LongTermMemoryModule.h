#pragma once
#include "FeatureModule.h"
#include "../data/DataRepository.h"

class AiService;
class LongTermMemoryModule final : public FeatureModule {
    Q_OBJECT
public:
    LongTermMemoryModule(DataRepository*,AiService*,QObject* parent=nullptr);
    QString id() const override; QString displayName() const override;
    bool isEnabled() const override; void setEnabled(bool) override;
    void analyzeRecentConversation();
    QString relevantContext(const QString &message);
    QString personalityContext(int closeness,int boredom) const;
    QString offlineRecallReply(const QString &message) const;
    bool handleUserDirective(const QString &message, QString *reply);
signals: void memoriesChanged(); void enabledChanged(bool); void analysisStatus(const QString&);
private: DataRepository *m_repository; AiService *m_ai; bool m_enabled=true;
    bool applyFactCorrection(const QString &message, QString *reply);
};
