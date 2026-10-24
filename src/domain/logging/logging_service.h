#pragma once

#include "domain/logging/log_event.h"

namespace domain {

class LoggingService
{
public:
    virtual ~LoggingService() = default;

    virtual void log(const LogEvent& event) = 0;
};

} // namespace domain
