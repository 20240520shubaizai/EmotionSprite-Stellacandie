#pragma once

#include <QObject>
#include <QString>

class FeatureModule : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    virtual QString id() const = 0;
    virtual QString displayName() const = 0;
    virtual bool isEnabled() const = 0;
    virtual void setEnabled(bool enabled) = 0;
};

