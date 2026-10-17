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
        Invalid,            // la validación falló: ver `validation`
        EndBeforeStart,     // modo línea de tiempo con el fin a la izquierda del inicio
        TreeUnavailable     // el método de árbol no aplica (Prim en un grafo dirigido)
    };

    Error error = Error::None;
    ValidationResult validation;
    std::optional<Schedule> schedule;

    bool ok() const noexcept { return error == Error::None && schedule.has_value(); }
};

// Calcula el horario de un ciclo de la pista. Es una función pura: no toca
// audio ni interfaz, y se vuelve a llamar cada vez que la pista cambia.
ScheduleResult buildSchedule (const model::Track& track);

} // namespace chimedes::engine
