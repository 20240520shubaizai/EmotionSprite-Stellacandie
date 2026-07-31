#include "ModuleManager.h"
#include "FeatureModule.h"

bool ModuleManager::registerModule(FeatureModule *module)
{
    if (!module || module->id().isEmpty() || m_modules.contains(module->id())) {
        return false;
    }
    module->setParent(this);
    m_modules.insert(module->id(), module);
    return true;
}

FeatureModule *ModuleManager::module(const QString &id) const
{
    return m_modules.value(id, nullptr);
}

