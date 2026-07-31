#pragma once
#include "FeatureModule.h"
#include "../data/DataRepository.h"
#include <QTimer>

class DataCleanupModule final : public FeatureModule
{
    Q_OBJECT
public:
    explicit DataCleanupModule(DataRepository *repository,QObject *parent=nullptr);
    QString id()const override{return QStringLiteral("data_cleanup");}
    QString displayName()const override{return QStringLiteral("记忆治理与数据清理");}
    bool isEnabled()const override{return m_enabled;}
    void setEnabled(bool enabled)override;
    QStringList memoryItems()const;
    QString summary()const;
    QString lastResult()const{return m_lastResult;}
    bool runMaintenance();
    bool toggleLock(int row);
    bool setState(int row,const QString &state);
    bool restore(int row);
    bool softDelete(int row);
signals:
    void changed();void enabledChanged(bool enabled);
private:
    void refresh();
    DataRepository *m_repository=nullptr;bool m_enabled=true;QTimer m_timer;QList<MemoryRecord>m_items;QString m_lastResult;
};
