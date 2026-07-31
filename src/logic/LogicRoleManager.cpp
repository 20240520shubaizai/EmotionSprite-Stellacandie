#include "LogicRoleManager.h"
#include <QStringList>

void LogicRoleManager::registerRole(std::unique_ptr<LogicRole> role)
{
    if (role) m_roles.push_back(std::move(role));
}

QString LogicRoleManager::combinedInstructions() const
{
    QStringList instructions;
    for (const auto &role : m_roles)
        instructions.append(QStringLiteral("[LOGIC_ROLE:%1]\n%2").arg(role->id(), role->instruction()));
    return instructions.join(QStringLiteral("\n\n"));
}
