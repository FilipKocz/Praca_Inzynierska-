# DiscretizationViewer – punkt 1: silnik renderowania na żywo

Qt 6 (>= 6.3), C++17, OpenGL 3.3 Core, CMake.

## Uruchomienie w Qt Creatorze
1. File → Open File or Project → `CMakeLists.txt`.
2. Wybierz kit Qt 6.x (MSVC 64-bit albo MinGW 64-bit) → Configure Project.
3. Ctrl+R.

Wymagane moduły Qt: Widgets, OpenGL, OpenGLWidgets (są w standardowej instalacji).

## Struktura
- `camera.*` – kamera orbitalna na kwaternionach: obrót (turntable / trackball),
  przesuwanie, zoom, presety widoków, rzut perspektywiczny i ortogonalny,
  automatyczne płaszczyzny odcięcia, dopasowanie do sceny.
- `discretizationview.*` – `QOpenGLWidget`: punkty jako sprite'y z cieniowaniem,
  kolorowanie wg typu węzła (konwencja Medusy: <0 brzeg, >0 wnętrze) / skalara (viridis) /
  jednolite, ramka otaczająca, gizmo osi. Dane można podmienić (`setDiscretization`)
  albo dopisywać porcjami (`beginStreaming` + `appendPoints`) – na GPU wysyłany jest
  tylko nowy fragment bufora.
- `discretization.*` – struktura danych, wczytywanie CSV, generatory demo.
- `mainwindow.*` – GUI: panel ustawień, symulacja generacji na żywo.

## Format CSV
Separator `,` `;` lub spacje, opcjonalny nagłówek, `#` = komentarz.
2 kolumny: `x,y` · 3: `x,y,z` · 4: `x,y,z,typ` · 5: `x,y,z,typ,skalar`.

## Sterowanie
LPM – obrót · PPM/ŚPM/Shift+LPM – przesuw · kółko – zoom · F – dopasuj ·
1/3/7 – przód/prawo/góra (Ctrl – przeciwny) · 0 – izometria · 5 – perspektywa/orto.
Dla danych 2D obrót jest wyłączony (LPM przesuwa).

## Podpięcie Medusy (później)
Silnik generuje w osobnym `QThread` i emituje porcje węzłów sygnałem połączonym
(Qt::QueuedConnection) ze slotem `appendPoints`. Wymaga to
`Q_DECLARE_METATYPE(Discretization)` + `qRegisterMetaType<Discretization>()`.
