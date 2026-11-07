#pragma once

#include <optional>

#include "engine/Schedule.h"
#include "engine/Validator.h"

namespace chimedes::engine
{

struct ScheduleResult
{
    enum class Error
    {
        None,
        Invalid,            // validation failed: see `validation`
        EndBeforeStart,     // timeline mode with the end node left of the start node
        TreeUnavailable     // the tree method does not apply (Prim on a directed graph)
    };

    Error error = Error::None;
    ValidationResult validation;
    std::optional<Schedule> schedule;

    bool ok() const noexcept { return error == Error::None && schedule.has_value(); }
};

// Computes the schedule of one cycle of the track. It is a pure function: it does
// not touch audio or UI, and is called again every time the track changes.
ScheduleResult buildSchedule (const model::Track& track);

} // namespace chimedes::engine
