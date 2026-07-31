#pragma once

#include "LogicRole.h"

#include <memory>
#include <vector>

class LogicRoleManager
{
public:
    void registerRole(std::unique_ptr<LogicRole> role);
    QString combinedInstructions() const;

private:
    std::vector<std::unique_ptr<LogicRole>> m_roles;
};
