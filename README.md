# Chimedes

Aplicación de escritorio para crear música generativa dibujando grafos. Los nodos son notas o acordes; las aristas, las conexiones entre ellos. La música surge al recorrer el grafo, con una línea de tiempo tradicional o con algoritmos de búsqueda.

Escrito en C++20 con [JUCE](https://juce.com) 9.

## Estado

Fase 1 (versión mínima) en curso:

- [x] Esqueleto del proyecto: CMake, JUCE y ventana con la regla de tiempo
- [x] Modelo y motor: validación de conexidad, árboles (BFS, DFS, Dijkstra, Prim), recorridos y horarios en ambos modos, con pruebas
- [ ] Audio: transporte, sintetizador y publicación de horarios entre hilos
- [ ] Lienzo: crear, mover y conectar nodos, y marcar inicio y fin

## Compilar

Clona con submódulos, porque JUCE vive en `JUCE/`:

```sh
git clone --recurse-submodules https://github.com/bfailach/Chimedes.git
cd Chimedes
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Si ya lo clonaste sin submódulos: `git submodule update --init`.

En Linux, JUCE necesita estas cabeceras (Ubuntu/Debian):

```sh
sudo apt install libasound2-dev libx11-dev libxrandr-dev libxinerama-dev \
  libxcursor-dev libxext-dev libxi-dev libxrender-dev libxcomposite-dev \
  libfreetype-dev libfontconfig1-dev libgl1-mesa-dev
```

En Windows basta con Visual Studio 2022 o posterior; en macOS, con Xcode.

## Pruebas

El modelo y el motor no dependen de JUCE, así que sus pruebas compilan en segundos:

```sh
cmake -S . -B build-tests -DCHIMEDES_BUILD_APP=OFF
cmake --build build-tests
./build-tests/chimedes_tests
```

## Estructura

```text
Source/
├── model/    Proyecto, pistas, nodos y aristas (C++ puro)
├── engine/   Validación, árboles, recorridos y horarios (C++ puro)
├── ui/       Interfaz con JUCE
└── Main.cpp  Punto de entrada de la aplicación
Tests/        Pruebas del motor
JUCE/         Submódulo de Git (JUCE 9.0.4)
```

## Cómo suena un grafo

El motor convierte cada pista en un **horario**: la lista de notas de un ciclo con su pulso de inicio. El audio solo reproduce ese horario.

- **Modo línea de tiempo:** cada nodo suena en el pulso `x / 100` (100 px = 1 pulso). El ciclo va del nodo de inicio hasta que acaba de sonar el de fin.
- **Modo algoritmo:** el grafo se convierte en un árbol con raíz en el nodo de inicio (BFS, DFS, Dijkstra o Prim) y se recorre con BFS o DFS. La espera entre nodos es la longitud de la arista que los une. Los hermanos de una ramificación pueden sonar en secuencia o a la vez.
