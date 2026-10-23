# Contribuir a BT4 Tag Team

Se aceptan reportes en español o inglés. Antes de proponer un cambio, revisa los
[issues](https://github.com/NauTaro/BT4-Tag-Team-Recomp/issues) y la
[revisión R16](docs/review/R16-rc1.md) para conocer el estado de la vista previa.

## Orientarse en el código

| Ruta | Uso |
| --- | --- |
| `src/`, `include/` | Juego y cabeceras de la decompilación, con modificaciones BT4 |
| `port/src/`, `port/include/` | Runtime del port, red, voz e interfaz |
| `port/setup/` | Lanzador de instalación |
| `port/release/` | Identidad de versión, política portable y scripts de distribución |
| `tools/review/` | Herramientas del flujo local de revisión |
| `server/cloudflare/`, `port/matchmaking/` | Servicios de red; consulta sus README antes de modificarlos |
| `docs/` | Documentación heredada, guías y evidencias de revisión BT4 |

Las notas de decompilación describen el BT3 original; no prueban por sí solas el
comportamiento del mod. Para trabajar en funciones decompiladas, consulta
[decomp_guide.md](docs/decomp_guide.md) y las reglas específicas de ese flujo en
[agent_rules.md](docs/agent_rules.md). Una prueba de herramientas no demuestra una
coincidencia binaria de una función del juego.

## Preparación y comprobaciones ligeras

Para documentación y pruebas de la herramienta portable basta con Python 3.12.
Desde la raíz del repositorio:

```sh
python -m unittest discover -s tools/review/tests -v
```

No hacen falta ISO, compilador de Windows ni dependencias de Python adicionales.
El mismo comando se ejecuta en GitHub Actions para Linux y Windows.

Para compilar el juego, consulta [tools/review/README.md](tools/review/README.md)
y la [documentación heredada del port](docs/port/README.md). Los helpers de
revisión reflejan un entorno local de Windows y necesitan herramientas y datos
externos. No constituyen todavía una receta de compilación limpia y portable.

## Reportar un error o sugerir una mejora

Usa las plantillas de issues. Para un error, incluye el contenido de
`BT4_VERSION.txt`, sistema operativo, GPU y controlador, modo local u online,
pasos reproducibles, resultado esperado y resultado observado. Para errores de
combate, indica personajes, trajes, escenario, técnica y número de jugadores.
Para errores de voz, añade los dispositivos de entrada y salida.

Adjunta solo los fragmentos relevantes de `bt3_log.txt` o `bt3_crash.txt`, quitando
datos personales. Las capturas pueden ayudar a describir errores visuales.
No adjuntes ISO, archivos extraídos del juego, partidas guardadas ni credenciales.

## Preparar una pull request

Mantén cada cambio centrado en un problema. Explica qué comportamiento cambia,
cómo lo comprobaste y qué queda sin verificar. Conserva los créditos y avisos de
licencia del código y las dependencias afectadas.

Los cambios de simulación o red deben explicar su efecto sobre el determinismo
y la compatibilidad de protocolo. Las pruebas locales o de herramientas no
sustituyen una partida real entre clientes para validar cambios online.
No afirmes compatibilidad con BT3 original u otros mods sin evidencia específica.

Mantén fuera del repositorio las ISO, datos del juego, partidas, herramientas
locales, compilaciones, registros y credenciales. Los archivos de los jugadores
se conservan: el verificador portable solo informa y no elimina entradas.
