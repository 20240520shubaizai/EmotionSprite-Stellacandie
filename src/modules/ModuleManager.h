#pragma once

#include <QHash>
#include <QObject>

class FeatureModule;

class ModuleManager final : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    bool registerModule(FeatureModule *module);
    FeatureModule *module(const QString &id) const;

private:
    QHash<QString, FeatureModule *> m_modules;
};

