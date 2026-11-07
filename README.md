# Chimedes

Aplicación de escritorio para crear música generativa dibujando grafos. Los nodos son notas o acordes; las aristas, las conexiones entre ellos. La música surge al recorrer el grafo, con una línea de tiempo tradicional o con algoritmos de búsqueda.

Escrito en C++20 con [JUCE](https://juce.com) 9.

## Estado

Fase 1 (versión mínima) completa:

- [x] Esqueleto del proyecto: CMake, JUCE y ventana con la regla de tiempo
- [x] Modelo y motor: validación de conexidad, árboles (BFS, DFS, Dijkstra, Prim), recorridos y horarios en ambos modos, con pruebas
- [x] Audio: transporte con BPM, sintetizador básico y publicación de horarios entre hilos
- [x] Lienzo editable, panel de propiedades, deshacer y rehacer
- [x] Ajuste opcional al pulso en el modo algoritmo
- [x] Interfaz oscura con paneles flotantes, selector de notas con teclado, seis instrumentos integrados y botón Aleatorio

Fase 2 completa:

- [x] Modo algoritmo: árboles, recorridos, hermanos en secuencia o a la vez y ajuste al pulso
- [x] Guardar y abrir proyectos (`.chimedes`)

Fase 3 completa:

- [x] Varias pistas que suenan a la vez, cada una con su ciclo
- [x] Edición en vivo mientras suena
- [x] Muestras de sonido propias como instrumentos

Fase 4 completa:

- [x] Aleatorizar el grafo, las notas, el ritmo, los instrumentos o el algoritmo, por separado o todo junto
- [x] Exportar a WAV
- [x] Exportar a MIDI

## Uso

La aplicación abre con un grafo de demostración; el botón **Nuevo** (icono de hoja) empieza uno vacío. El lienzo ocupa toda la ventana, con el panel de propiedades flotando a la derecha y el transporte en la barra inferior.

| Acción | Cómo |
| --- | --- |
| Crear nodo | Doble clic en un espacio vacío (hereda las notas del último nodo seleccionado) |
| Mover nodo | Arrastrar desde su centro |
| Conectar | Arrastrar desde el borde de un nodo hasta otro |
| Seleccionar | Clic en un nodo o una arista |
| Borrar | Supr o Retroceso, con algo seleccionado |
| Inicio y fin | Clic derecho en un nodo, o los botones del panel derecho |
| Notas del nodo | Teclado del panel derecho: clic propone una nota y la hace sonar, Mayús+clic forma acordes; `<` `>` cambian de octava. La propuesta se marca con borde blanco y se guarda con Enter |
| Instrumento | Panel derecho: uno para la pista y, si quieres, otro para cada nodo |
| Probar notas | Clic en el teclado del panel o letras **A W S E D F T G Y H U J K O L P** (de do a re♯ de la octava siguiente, desde el primer do visible). Suenan al momento. Varias letras a la vez o Mayús forman un acorde; **Z / X** cambian de octava |
| Guardar la nota o el acorde | **Enter** (Esc descarta la propuesta) |
| Nuevo, abrir, guardar | Iconos de hoja, carpeta y disquete de la barra superior (junto al nombre del proyecto), o **Ctrl+N**, **Ctrl+O**, **Ctrl+S** (**Ctrl+Mayús+S**: guardar como). Antes de cerrar, abrir otro o empezar uno nuevo, si hay cambios sin guardar, la app pregunta |
| Pistas | Panel izquierdo: clic activa una pista (la que se edita; las demás se ven atenuadas y siguen sonando), doble clic la renombra, el altavoz la silencia, **+** agrega una (hasta 16) y el clic derecho duplica o elimina. Un clic en una estrella atenuada también activa su pista. Todo se puede deshacer |
| Al azar | Botón **Aleatorio** (icono de dado): un menú para sortear en la pista activa el grafo, las notas, el ritmo, los instrumentos o el algoritmo, todo junto, o todo en todas las pistas. Se puede deshacer |
| Exportar | Icono de bandeja de la barra superior: **WAV** (audio, 48 kHz y 24 bits) o **MIDI**, eligiendo la duración |
| Zoom | Rueda del ratón |
| Desplazarse | Arrastrar en un espacio vacío, o con el botón central |
| Reproducir / pausa | Barra espaciadora |
| Deshacer / rehacer | Iconos de flecha de la barra inferior, Ctrl+Z / Ctrl+Y o Ctrl+Mayús+Z |

Los dos primeros nodos de una pista vacía se marcan solos como inicio y fin. Anillos de los nodos: verde = inicio, violeta = fin, naranja = inalcanzable. Los nombres de las notas aparecen tenues junto a cada nodo y se aclaran al pasar el ratón o al seleccionarlo; la regla de tiempo solo se muestra en el modo línea de tiempo. Los nodos inalcanzables desde el inicio se marcan en naranja y la pista no suena hasta corregirlo.

### Aspecto: atlas estelar

El lienzo se dibuja como un mapa de estrellas sobre negro suave (tema *Starlight*). Cada nodo es una estrella cuyo tamaño sigue a la intensidad. Al reproducir:

- una chispa recorre la arista hacia la nota siguiente;
- la estrella destella en el color de su nota y lanza ondas de colores vecinos (en un acorde, la última onda toma el color de su otra nota);
- lo recorrido queda como un rastro pastel que se apaga al empezar el ciclo siguiente.

Los colores siguen la rueda cromática por clase de nota: do rojo, re naranja, mi verde, sol azul, la violeta, si magenta. Un do es del mismo color en cualquier octava, y las teclas seleccionadas en el teclado del panel usan el mismo color. El resto de la interfaz es casi monocroma, con blanco estelar como único acento.

Tipografías: [Space Grotesk](https://github.com/floriankarsten/space-grotesk) para la interfaz y [JetBrains Mono](https://github.com/JetBrains/JetBrainsMono) para las cifras, ambas con licencia SIL Open Font License (ver `Resources/Fonts`). Van incluidas en el ejecutable.

## Varias pistas

Cada pista es un grafo independiente con sus propias opciones (modo, árbol, recorrido, instrumento) y su propio ciclo: una pista de 3 pulsos contra otra de 4 da una polirritmia que se repite cada 12. Todas comparten el BPM y el lienzo; la activa se dibuja entera y las demás atenuadas, con sus destellos al sonar. Las pistas nuevas toman otro instrumento para distinguirlas al oído.

## Proyectos

Un proyecto se guarda como `.chimedes`: un archivo JSON legible con el BPM, las opciones de la pista y cada nodo y arista en su propia línea. El nombre del archivo aparece en la barra superior y en el título de la ventana, con un punto (•) si hay cambios sin guardar. En Windows se puede abrir un proyecto arrastrándolo sobre `Chimedes.exe` o pasándolo como argumento.

Al abrir, los campos que falten toman su valor por defecto, los valores fuera de rango se ajustan y se descartan las aristas hacia nodos que no existen, así que un archivo editado a mano no deja la app en un estado imposible. Si el archivo está dañado, el mensaje dice en qué línea. Al guardar se escribe primero en un archivo temporal: si algo falla, el anterior queda intacto.

## Instrumentos

Seis instrumentos sintetizados, sin archivos de audio: órgano, piano eléctrico (FM), campana (FM inarmónica), cuerda pulsada (Karplus-Strong), pad (dos sierras desafinadas y filtradas) y bajo. Cada instrumento suena por su propio canal MIDI, así que en una misma pista pueden convivir varios.

### Muestras propias

En el desplegable de instrumento (de la pista o de un nodo), **Cargar muestra…** abre un archivo WAV, AIFF, FLAC u OGG (hasta 30 segundos) y lo usa como instrumento: la muestra se toma como grabada en do central (C4) y se transpone para cada nota. Un proyecto admite hasta diez muestras; si no hay sitio, se quitan las que nadie usa. Volver a cargar el mismo archivo reutiliza la muestra.

El proyecto guarda la ruta del archivo, no el audio. Si la muestra está dentro de la carpeta del proyecto, la ruta se guarda relativa, así que se puede mover o compartir la carpeta entera. Si al abrir falta un archivo, aparece un aviso y esa muestra queda en silencio; el resto del proyecto suena normal.

## Aleatorio

El dado abre un menú. Todo afecta a la pista activa, salvo la última opción, y se puede deshacer:

- **Grafo nuevo**: ver abajo.
- **Notas**: misma forma, escala y tonalidad nuevas; la melodía sigue las aristas desde el inicio y el fin vuelve a la tónica.
- **Ritmo**: duraciones e intensidades nuevas, con un carácter para toda la pista (más cortas o largas, más parejas o acentuadas). El nodo de fin conserva su duración, así el ciclo dura lo mismo.
- **Instrumentos**: el de la pista y, a veces, algunos nodos con otro, entre los integrados y las muestras cargadas.
- **Algoritmo** (en modo algoritmo): árbol (Prim solo en grafos no dirigidos), recorrido, hermanos y ajuste al pulso.
- **Todo**, y **Todo, en todas las pistas** (cada una con su propio sorteo, en un solo paso de deshacer).

El grafo nuevo es al azar: de 5 a 14 nodos; forma de cadena, ramas, abanico o red; nodos repartidos o apiñados; dispersión vertical variable; y una duración total de 4 a 16 pulsos. Las notas caen sobre una rejilla de pulsos o medios pulsos y la última termina justo al acabar el ciclo, así que el ciclo dura un número entero de pulsos y la música sigue al BPM. Las notas salen de una escala y tonalidad al azar, con algún acorde, y la última nota vuelve a la tónica. El grafo siempre se puede reproducir y la vista se ajusta para mostrarlo entero.

## Exportar

Desde el icono de bandeja de la barra superior. Antes de elegir el archivo se elige la duración, con lo que dura a ese BPM:

- **Vuelta completa**: hasta que todas las pistas vuelven a coincidir (el mínimo común múltiplo de sus ciclos; una pista de 3 pulsos y otra de 4 dan 12). Si eso pasa de 4096 pulsos, tantas vueltas de la pista más larga como quepan.
- **1, 2, 4 u 8 vueltas** de la pista más larga.

Solo suenan las pistas no silenciadas.

- **WAV**: el audio tal como suena, con el mismo motor, instrumentos y muestras, renderizado más rápido que en tiempo real, con barra de progreso y botón de cancelar. Estéreo, 48 kHz, 24 bits, con 2 segundos de cola al final para que las notas se apaguen. Si la mezcla pasa de 0 dB, se baja el volumen entero para que no recorte.
- **MIDI**: archivo estándar (formato 1) con una pista de tempo y una pista MIDI por cada pista, con su nombre. Cada instrumento va en su canal (el 10, percusión en General MIDI, nunca se usa) y los integrados llevan un programa General MIDI parecido: órgano, piano eléctrico, campanas tubulares, guitarra de nailon, pad cálido y bajo. Las muestras no llevan programa: en el programa de destino se les asigna el sonido que se quiera.

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

## Renderizar sin tarjeta de sonido

`ChimedesRender` se compila junto con la aplicación y renderiza a WAV con el mismo motor de audio: el grafo de demostración, o un proyecto con la misma exportación que la app (`ChimedesRender salida.wav proyecto.chimedes [vueltas, 0 = completa]`):

```sh
./build/ChimedesRender_artefacts/Debug/ChimedesRender demo.wav timeline 2 3
```

Modos: `timeline`, `seq-bfs`, `seq-dfs`, `sim-bfs`. Después van el número de ciclos y, opcionalmente, el instrumento (1 órgano, 2 piano eléctrico, 3 campana, 4 cuerda pulsada, 5 pad, 6 bajo). La herramienta muestra los tiempos esperados de cada nota junto a los ataques que detecta en el audio.

## Estructura

```text
Source/
├── model/    Proyecto, pistas, nodos y aristas (C++ puro)
├── engine/   Validación, árboles, recorridos, horarios y secuenciador (C++ puro)
├── audio/    Motor de audio y sintetizador (JUCE)
├── ui/       Interfaz con JUCE
└── Main.cpp  Punto de entrada de la aplicación
Tools/        Herramientas de desarrollo (ChimedesRender)
Tests/        Pruebas del motor
JUCE/         Submódulo de Git (JUCE 9.0.4)
```

## Cómo suena un grafo

El motor convierte cada pista en un **horario**: la lista de notas de un ciclo con su pulso de inicio. El audio solo reproduce ese horario.

- **Modo línea de tiempo:** cada nodo suena en el pulso `x / 100` (100 px = 1 pulso). El ciclo va del nodo de inicio hasta que acaba de sonar el de fin.
- **Modo algoritmo:** el grafo se convierte en un árbol con raíz en el nodo de inicio (BFS, DFS, Dijkstra o Prim) y se recorre con BFS o DFS. La espera entre nodos es la longitud de la arista que los une. Los hermanos de una ramificación pueden sonar en secuencia o a la vez.
