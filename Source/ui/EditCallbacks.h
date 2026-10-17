#pragma once

#include <functional>

#include "model/Track.h"

namespace chimedes::ui
{

// Cómo los componentes de la interfaz piden cambios a la pista. Quien posee la pista
// (MainComponent) los aplica, los registra para deshacer y recalcula el horario.
//
//   - edit: un cambio puntual, una entrada de deshacer.
//   - beginGesture / updateGesture / endGesture: un cambio continuo (arrastrar un
//     nodo, mover un deslizador) que se aplica en vivo y deja una sola entrada.
struct EditCallbacks
{
    using Change = std::function<void (model::Track&)>;

    std::function<void (const Change&)> edit;
    std::function<void()> beginGesture;
    std::function<void (const Change&)> updateGesture;
    std::function<void()> endGesture;
};

} // namespace chimedes::ui
