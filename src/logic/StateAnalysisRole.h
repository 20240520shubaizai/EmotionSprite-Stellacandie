#pragma once

#include "LogicRole.h"

class StateAnalysisRole final : public LogicRole
{
public:
    QString id() const override;
    QString instruction() const override;
};
