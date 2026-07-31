#pragma once

#include <QString>

class LogicRole
{
public:
    virtual ~LogicRole() = default;
    virtual QString id() const = 0;
    virtual QString instruction() const = 0;
};
