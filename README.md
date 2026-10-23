**Español** · [English](README.en.md)

<div align="center">
  <h1>DBZ BT4 Tag Team Recomp</h1>
  <p><strong>El universo Tenkaichi. Tu equipo. Una sola arena.</strong></p>
  <img src="docs/presentation/bt4-intro.gif" width="800" alt="Presentación animada de BT4 Tag Team con Goku, Vegeta, Jiren, Hit, Broly y Freezer">
  <p>Combates simultáneos por equipos · Salas online · Voz por proximidad</p>
  <p><strong>Windows · Perfil BT4 B14 Rev2 · Vista previa R16-rc1</strong></p>
  <p><a href="#qué-cambia-frente-a-bt3-original">Descubre el mod</a> · <a href="#instalación-y-actualización">Instalación</a> · <a href="docs/review/R16-rc1.md">Estado y pruebas</a></p>
</div>

## La arena se llena. La batalla cambia.

**BT4 Tag Team Recomp** lleva la intensidad de Budokai Tenkaichi a una batalla
compartida: varios luchadores, dos equipos y toda la acción ocurriendo a la vez.
El proyecto une el contenido del perfil **BT4 B14 Rev2** con una adaptación
nativa para Windows basada en **Tenkaichi3Decomp**, y añade su propio modo Tag Team.

Mientras buscas una apertura, tus aliados también están peleando. Cambiar de
objetivo, seguir el estado del equipo y coordinarse pasa a formar parte del combate.
La propuesta conserva el espectáculo de Tenkaichi y lo abre a enfrentamientos
simultáneos de **hasta 5 contra 5**, con plazas para jugadores y CPU.

| Forma tu equipo | Comparte la batalla | Encuentra tu estilo |
| --- | --- | --- |
| Hasta diez luchadores en la arena, repartidos en dos equipos. | Salas con código y voz por proximidad para coordinarte durante la partida. | HUD Tag Team ajustable y opciones de imagen, audio y controles del port de PC. |

<p align="center">
  <img src="docs/presentation/tag-team-battle.jpg" width="960" alt="Combate Tag Team 5 contra 5 con panel de aliados, objetivos enemigos y radar">
  <br><em>Un campo de batalla compartido: aliados a la izquierda, rivales a la derecha y radar para seguir la acción.</em>
</p>

## Qué cambia frente a BT3 original

La referencia de esta comparación es **Budokai Tenkaichi 3 de PlayStation 2**.
El contenido BT4, las funciones del port y las incorporaciones Tag Team se
combinan aquí en una experiencia propia.

| Aspecto | BT3 original (PS2) | BT4 Tag Team Recomp |
| --- | --- | --- |
| Combates por equipos | Un luchador activo por bando, con relevos entre miembros. | Varios luchadores activos al mismo tiempo, hasta 5 por equipo. |
| Personajes y contenido | Plantel y recursos del juego original. | Contenido del perfil BT4 B14 Rev2, con adaptaciones de carga de recursos para el port. |
| Juego en red | Sin el sistema de salas online de este proyecto. | Salas con código y selección de equipos para organizar partidas Tag Team. |
| Comunicación | Sin voz por proximidad integrada. | Voz integrada cuya atenuación depende de la distancia entre luchadores. |
| Lectura del combate | Interfaz centrada en los dos luchadores activos. | HUD ajustable con información de aliados, objetivos y radar para varios combatientes. |
| Experiencia en PC | Versión original diseñada para consola. | Ejecución nativa en Windows y ajustes de resolución, pantalla, audio y controles. |

La base de ejecución en PC y sus opciones proceden del trabajo de
**Tenkaichi3Decomp**. La adaptación al perfil BT4 y el modo Tag Team amplían esa
base; los créditos del port y del contenido se conservan más abajo.

<p align="center">
  <img src="docs/presentation/pc-settings.jpg" width="960" alt="Pantalla de inicio BT4 en Windows con el panel de ajustes de vídeo del port abierto">
  <br><em>El espectáculo de BT4 con un panel de ajustes de PC para adaptar la presentación a tu equipo.</em>
</p>

Las capturas muestran una configuración concreta, no una garantía de rendimiento
para todos los equipos. La capacidad de hasta diez luchadores tampoco implica
que todas las combinaciones de jugadores, personajes y conexiones estén verificadas;
consulta las pruebas y límites de la vista previa.

## Estado del proyecto

La versión registrada es `BT4-v0.1.38.1-r16-rc1`, para el perfil **BT4 B14 Rev2**.
Consulta la [revisión R16](docs/review/R16-rc1.md) para conocer las pruebas
realizadas y los problemas pendientes. Entre los límites documentados están el
fallo visual de la primera técnica rush de Hit, aún sin resolver, y la cobertura
limitada de equipos, dispositivos y escenarios.

Las descargas disponibles aparecen en [Releases](https://github.com/NauTaro/BT4-Tag-Team-Recomp/releases).
Una release en borrador no es una descarga publicada. Descargar el código fuente
no proporciona una instalación lista para jugar.

## Instalación y actualización

El ZIP completo del lanzador para Windows está preparado para una primera
instalación desde tu propia ISO de **BT4 B14 Rev2** o para una instalación con los
datos ya extraídos. El juego y sus datos no se distribuyen aquí.

1. Obtén un paquete compatible desde Releases cuando esté publicado.
2. Para una primera instalación, utiliza el lanzador con tu propia ISO compatible.
3. Para actualizar, cierra el juego y copia los archivos del programa sobre tu instalación.
   El paquete no incluye `gamedata`, `saves` ni `bt3_settings.txt`, por lo que
   esos datos no se reemplazan.

No necesitas extraer la ISO en cada actualización del código. La extracción es
necesaria para una instalación nueva o un perfil de datos distinto o incompatible.
El [perfil BT4](BT4_PROFILE.md) documenta los datos esperados; su referencia a
0.1.37 describe la base histórica del perfil. La base upstream actual está fijada
en [UPSTREAM_LOCK.json](UPSTREAM_LOCK.json).

La [política de instalación portable](port/release/PORTABLE_LAYOUT.json) define
la estructura de la carpeta del juego. Los informes de pruebas, el código fuente
y los ZIP de distribución se guardan fuera de ella. El verificador permite
revisar esa estructura sin borrar archivos:

```sh
python tools/review/check_portable_layout.py "ruta/a/tu/instalacion"
```

Este comando revisa los nombres y tipos de las entradas de la carpeta principal;
no verifica la integridad de los datos ni garantiza que la instalación esté completa.

## Documentación y desarrollo

| Recurso | Contenido |
| --- | --- |
| [Cómo contribuir](CONTRIBUTING.md) | Preparación, comprobaciones y reportes de errores |
| [Herramientas de revisión](tools/review/README.md) | Requisitos y límites del flujo de compilación local |
| [Documentación del port](docs/port/README.md) | Guías del port heredadas de upstream |
| [Notas de decompilación](docs/README.md) | Sistemas internos del BT3 original y evidencia técnica |
| [Revisión R16](docs/review/R16-rc1.md) | Pruebas registradas y límites de la vista previa |
| [README original](README_UPSTREAM.md) | Documentación y créditos de Tenkaichi3Decomp |

Las comprobaciones ligeras necesitan Python 3.12 y se ejecutan desde la raíz:

```sh
python -m unittest discover -s tools/review/tests -v
```

GitHub Actions ejecuta estas pruebas en Linux y Windows. Comprueban la herramienta
de estructura portable; no compilan el juego ni validan jugabilidad, voz o red.
La compilación completa sigue dependiendo del entorno descrito en las herramientas
de revisión y no se ha verificado desde cero en otros equipos.

## Base, créditos y licencias

Base: [z3xox/Tenkaichi3Decomp](https://github.com/z3xox/Tenkaichi3Decomp),
tag `v0.1.38.1`, commit `11059a6d3e814cf36e6ca1f6307839f61c56f1cc`.
Se conserva el historial de upstream. Este mod es independiente y no es una
publicación oficial de ese proyecto.

Decompilación y port original: colaboradores de Tenkaichi3Decomp. El contenido
BT4, las ilustraciones y el icono conservan los derechos de sus respectivos autores.
Se mantienen las licencias [AGPL](LICENSE), [MIT](LICENSE-MIT) y los avisos de
terceros incluidos con las bibliotecas del runtime y de voz.

Capturas y animación: [origen y créditos de los medios](docs/presentation/CREDITS.md).
