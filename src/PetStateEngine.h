#pragma once

#include "data/repositories/PetStateRepository.h"
#include <QObject>
#include <QJsonObject>

class PetStateEngine final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int mood READ mood NOTIFY statsChanged)
    Q_PROPERTY(int energy READ energy NOTIFY statsChanged)
    Q_PROPERTY(int health READ health NOTIFY statsChanged)
    Q_PROPERTY(int closeness READ closeness NOTIFY statsChanged)
    Q_PROPERTY(int boredom READ boredom NOTIFY statsChanged)
    Q_PROPERTY(int neglect READ neglect NOTIFY statsChanged)
    Q_PROPERTY(int curiosity READ curiosity NOTIFY statsChanged)
    Q_PROPERTY(int irritation READ irritation NOTIFY statsChanged)
    Q_PROPERTY(int fullness READ fullness NOTIFY statsChanged)
    Q_PROPERTY(QString healthPhase READ healthPhase NOTIFY statsChanged)
    Q_PROPERTY(QString healthPhaseName READ healthPhaseName NOTIFY statsChanged)
    Q_PROPERTY(QString conditionName READ conditionName NOTIFY statsChanged)
    Q_PROPERTY(int recoveryProgress READ recoveryProgress NOTIFY statsChanged)
public:
    explicit PetStateEngine(PetStateRepository *storage,QObject *parent=nullptr);
    int mood()const; int energy()const; int health()const; int closeness()const;
    int boredom()const; int neglect()const; int curiosity()const; int irritation()const;
    int fullness()const;
    QString healthPhase()const; QString healthPhaseName()const; QString conditionName()const;
    int recoveryProgress()const; QString healthContext()const;
    void load(); int processMessage(const QString &message); int applySemanticEffect(const QJsonObject &effect);
    int refreshForElapsedTime(); int currentResolvedState()const{return m_resolvedState;}
    Q_INVOKABLE void adjustForDebug(const QString &stat,int delta);
    Q_INVOKABLE void resetForDebug();
    Q_INVOKABLE void forceMagicColdForDebug();
    Q_INVOKABLE void advanceRecoveryForDebug(int amount=25);
    Q_INVOKABLE void healForDebug();
    Q_INVOKABLE void rest();
    bool canEat(int nutrition, QString *reason=nullptr);
    void feedSnack(int nutrition);
signals:
    void statsChanged(); void suggestedStateChanged(int stateIndex);
private:
    static int bounded(int value); void saveAndNotify(); int resolveState()const; int publishResolvedState();
    void updateHealthForElapsedTime(const QDateTime &now); void evaluateDailyIllness(const QDateTime &now);
    void setHealthPhase(const QString &phase,const QDateTime &now); void addRecovery(int amount);
    void updateHealthValue();
    PetStateRepository *m_storage=nullptr; PetStateRecord m_state;
    int m_resolvedState=1,m_candidateState=1,m_candidateCount=0;
};
