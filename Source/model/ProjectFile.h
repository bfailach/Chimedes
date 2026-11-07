#pragma once

#include <string>

#include "model/Track.h"

namespace chimedes::model
{

// Formato de archivo de Chimedes (.chimedes): JSON legible, con las opciones como
// palabras ("timeline", "bfs"...) en lugar de números.
//
// {
//   "format": "chimedes", "version": 1, "bpm": 120,
//   "tracks": [ { "name": "", "directed": true, "mode": "timeline", ...,
//                 "start": 1, "end": 5,
//                 "nodes": [ { "id": 1, "x": 0, "y": 0, "pitches": [60], ... } ],
//                 "edges": [ { "id": 1, "from": 1, "to": 2 } ] } ]
// }
//
// Al leer, los campos que falten toman su valor por defecto, los valores fuera de
// rango se ajustan y las aristas hacia nodos que no existen se descartan: un archivo
// editado a mano no debe dejar la app en un estado imposible.

inline constexpr const char* projectFileExtension = ".chimedes";
inline constexpr int currentProjectFormatVersion = 1;

std::string projectToJson (const Project& project);

struct ProjectLoadResult
{
    bool ok = false;
    std::string error;   // en español, para mostrarlo tal cual
    Project project;
};

ProjectLoadResult projectFromJson (const std::string& json);

} // namespace chimedes::model
