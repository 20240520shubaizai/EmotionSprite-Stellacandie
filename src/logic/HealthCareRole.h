#pragma once
#include "LogicRole.h"
class HealthCareRole final : public LogicRole { public: QString id()const override; QString instruction()const override; };
