# Roadmap y checklist de LinuxGitShell

> Hoja de ruta ejecutable para convertir LinuxGitShell en una integración gráfica de Git, nativa para Linux, mantenible y preparada para una comunidad open source.

Este roadmap deriva de la [especificación maestra](LinuxGitShell-Codex.md). La especificación sigue siendo la fuente de verdad para requisitos funcionales y decisiones de arquitectura; este archivo organiza el trabajo, sus dependencias y los criterios para considerar cada hito terminado.

## Estado inicial

- [x] Visión, alcance funcional y arquitectura objetivo documentados.
- [x] Plataforma inicial definida: Manjaro/Arch Linux, KDE Plasma 6 y Dolphin.
- [x] Stack inicial definido: C++20, Qt 6, KDE Frameworks 6, CMake, Git CLI y D-Bus.
- [x] Bootstrap compilable del proyecto.
- [x] Comunidad, licencia y procesos de contribución formalizados con canales públicos y privados.
- [x] Primera versión publicada: [`v0.1.0`](https://github.com/luks95/linuxgitshell/releases/tag/v0.1.0).

**Fase actual:** Fase 2 — Menú contextual de Dolphin (`v0.2.0`, parte 1).

**Último incremento:** plugin mínimo fusionado en
[#9](https://github.com/luks95/linuxgitshell/pull/9); diseño del contexto no bloqueante en
[`docs/repository-context-cache.md`](docs/repository-context-cache.md), seguido por
[#10](https://github.com/luks95/linuxgitshell/issues/10); modelo de snapshots y cache acotado sin Git
en `libs/repositorycontext/`, servicio de contexto experimental en `daemon/`, cliente no bloqueante
y menú según el repositorio (`Show Status`), seguidos por
[#12](https://github.com/luks95/linuxgitshell/issues/12).

## Cómo utilizar este documento

- Marcar una tarea solo cuando exista evidencia verificable en el repositorio, CI o artefactos publicados.
- Mantener una sola fase principal activa y abrir trabajo paralelo únicamente si no compromete la estabilidad.
- Actualizar `STATUS.md` al finalizar cada sesión importante de desarrollo.
- Convertir las tareas de la fase activa en issues pequeños, con alcance y criterios de aceptación claros.
- No cerrar una fase si falla alguno de sus criterios de salida.
- Añadir enlaces a issues, pull requests o decisiones de arquitectura junto a la tarea correspondiente cuando existan.
- Revisar este roadmap al cerrar cada milestone; no usarlo como sustituto del seguimiento diario en issues.

## Principios no negociables

- [ ] Mantener los plugins de Dolphin pequeños; la lógica Git, el cache y las ventanas viven fuera del proceso de Dolphin.
- [ ] Centralizar toda ejecución de Git; ningún widget debe invocar Git directamente.
- [ ] Usar argumentos separados con `QProcess`; evitar `shell -c` y comandos construidos por concatenación.
- [ ] Preferir salidas `--porcelain`, `-z` y otros formatos estables para automatización.
- [ ] No bloquear el hilo principal de Dolphin ni de la aplicación con operaciones Git.
- [ ] Mantener separados modelo, servicios, IPC e interfaz gráfica.
- [ ] Respetar XDG y utilizar KConfig donde corresponda.
- [ ] No almacenar ni registrar contraseñas, tokens, claves privadas o credenciales embebidas en URLs.
- [ ] Confirmar claramente las operaciones destructivas y ofrecer primero la variante más segura.
- [ ] No copiar código ni recursos de TortoiseGit sin una revisión explícita de compatibilidad de licencia.
- [ ] Mantener el proyecto compilable, ejecutable y probado al terminar cada fase.
- [ ] No prometer en la documentación funciones que todavía no estén disponibles.

## Estrategia de versiones e hitos

| Hito | Alcance | Fases |
| --- | --- | --- |
| `v0.1.0` Foundation | Core Git probado y aplicación mínima | 0–1 |
| `v0.2.0` Dolphin MVP | Menú contextual y overlays con cache local | 2–3 |
| `v0.3.0` Background services | Daemon, D-Bus, watchers y cache estable | 4 |
| `v0.4.0` Daily workflow | Working Tree, commit y diff | 5–6 |
| `v0.5.0` History | Log, historial de archivo y acciones sobre commits | 7 |
| `v0.6.0` Conflict ready | Resolución de conflictos de tres vías | 8 |
| `v0.7.0` References | Branches, merge y tags | 9 |
| `v0.8.0` Remotes | Fetch, pull, push y sincronización | 10 |
| `v0.9.0` Advanced workflows | Rebase, cherry-pick, stash y herramientas de historial | 11–12 |
| `v1.0.0` Dolphin stable | Producto estable, configurable, traducido y empaquetado | 13–15 y gate 1.0 |
| `v1.x` Expansion | Funciones avanzadas y otros gestores de archivos | Post-1.0 |

Las versiones son objetivos de planificación, no fechas prometidas. Un hito puede dividirse sin degradar sus criterios de calidad.

---

# Fase 0 — Fundación open source y bootstrap

## 0.1 Decisiones y documentación del proyecto

- [x] Crear `README.md` con propósito, estado real, plataforma soportada y limitaciones.
- [x] Elegir y añadir `LICENSE`: MIT.
- [x] Revisar compatibilidad de licencia de Qt, KDE Frameworks y cada dependencia incorporada.
- [x] Definir política de encabezados SPDX para código y recursos.
- [x] Crear `CONTRIBUTING.md` con build, tests, estilo, traducciones, issues y pull requests.
- [x] Crear `CODE_OF_CONDUCT.md` y un procedimiento de aplicación con contacto responsable.
- [x] Crear `SECURITY.md` con versiones soportadas y canal privado para reportes sensibles.
- [x] Crear `GOVERNANCE.md` con roles, toma de decisiones y proceso para nuevos maintainers.
- [x] Crear `CHANGELOG.md` basado en cambios orientados al usuario.
- [x] Crear `STATUS.md` usando el formato definido en la especificación.
- [x] Crear `SUPPORT.md` para separar soporte, preguntas, bugs y vulnerabilidades.
- [x] Definir DCO (`Signed-off-by`) como mecanismo de contribución, sin CLA.
- [x] Documentar la política de compatibilidad y deprecación de la API D-Bus.
- [ ] Registrar decisiones arquitectónicas relevantes en `docs/adr/` a medida que se tomen, sin crear estructura vacía.

## 0.2 Infraestructura de colaboración

- [x] Publicar el repositorio en una forja abierta y configurar descripción, tópicos y enlace de documentación.
- [x] Proteger la rama principal: pull request, CI obligatorio y prohibición de force-push.
- [x] Crear plantillas para bug, solicitud de función y propuesta de diseño.
- [x] Crear plantilla de pull request con checklist de tests, documentación, traducciones y seguridad.
- [x] Definir etiquetas mínimas: `bug`, `feature`, `docs`, `good first issue`, `help wanted`, `security`, `performance`, `accessibility`, `packaging` y `blocked`.
- [x] Definir milestones alineados con las versiones de este roadmap.
- [x] Activar discusiones o un canal comunitario y documentar qué tipo de conversación pertenece allí.
- [x] Configurar revisión de dependencias y alertas de seguridad disponibles en la forja.
- [x] Definir política de triage, tiempos orientativos y cierre de issues inactivos sin automatismos agresivos.
- [ ] Identificar y documentar owners de core, Dolphin, UI, packaging, traducciones y releases cuando exista comunidad suficiente.

## 0.3 Inspección del entorno

- [x] Registrar distribución y versión.
- [x] Detectar KDE Plasma, Dolphin, Qt 6 y KDE Frameworks 6.
- [x] Detectar Git, CMake, Extra CMake Modules y compilador C++.
- [x] Consultar con `pacman` los nombres y versiones reales de paquetes; no asumirlos.
- [x] Documentar dependencias obligatorias, opcionales y exclusivas de desarrollo.
- [x] No instalar dependencias hasta informar claramente qué falta y por qué.
- [x] Definir versiones mínimas razonables sin fijarlas más de lo necesario.

## 0.4 Bootstrap técnico

- [x] Crear `CMakeLists.txt` raíz con C++20 y warnings apropiados.
- [x] Crear una aplicación Qt/KF6 mínima llamada `linuxgitshell`.
- [x] Crear la librería inicial `gitcore` sin dependencia de widgets.
- [x] Integrar un framework de pruebas compatible con el stack Qt/KDE.
- [x] Añadir una prueba mínima ejecutable mediante CTest.
- [x] Configurar logging por categorías desde el inicio.
- [x] Preparar internacionalización para inglés y español sin hardcodear la UI.
- [x] Crear reglas de formato para C++ y documentar cómo aplicarlas.
- [x] Configurar un build fuera del árbol fuente.
- [x] Permitir instalación en prefijo de usuario para el ciclo de desarrollo cuando sea viable.
- [x] Documentar build, ejecución, tests, instalación y desinstalación.
- [x] Verificar que no se generan archivos fuera de rutas XDG previstas.

## 0.5 CI inicial

- [x] Configurar configure, build y unit tests en cada pull request.
- [x] Usar un contenedor Arch Linux con Qt 6/KF6.
- [x] Añadir comprobación de formato sin modificar automáticamente contribuciones.
- [x] Añadir análisis estático inicial y comprobación de patrones comunes de secretos.
- [x] Guardar logs útiles cuando falle el build o un test.
- [x] Cancelar jobs obsoletos de la misma rama para ahorrar recursos.
- [x] Documentar diferencias conocidas entre CI y Manjaro/Arch real.

## Criterio de salida de la Fase 0

- [x] `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug` finaliza correctamente.
- [x] `cmake --build build -j` finaliza correctamente.
- [x] `ctest --test-dir build --output-on-failure` ejecuta al menos un test y pasa.
- [x] La aplicación mínima inicia en KDE Plasma 6.
- [x] CI reproduce configure, build y tests.
- [x] Existen licencia, README, guía de contribución, seguridad y estado del proyecto.

---

# Fase 1 — Git Core (`v0.1.0`)

## 1.1 Runner de procesos

- [x] Implementar `GitProcessRunner` sobre `QProcess`.
- [x] Modelar programa, argumentos, directorio de trabajo, entorno, stdout, stderr y exit code.
- [x] Añadir ejecución asíncrona y cancelación segura.
- [x] Añadir timeout solo donde sea apropiado y distinguirlo de cancelación.
- [x] Sanitizar logs para ocultar credenciales y URLs sensibles.
- [x] Conservar la salida Git original para diagnóstico.
- [x] Crear errores tipados y traducción de errores frecuentes a mensajes útiles.
- [x] Probar paths con espacios, Unicode y nombres que comienzan con guion.

## 1.2 Descubrimiento y modelo de repositorio

- [x] Detectar la raíz con `git rev-parse` desde cualquier subdirectorio.
- [x] Soportar repositorio normal, bare, submódulo y linked worktree.
- [x] Soportar `.git` como archivo y como directorio.
- [x] Detectar worktree, git dir y superproyecto sin asumir rutas.
- [x] Modelar branch actual, detached HEAD, upstream, ahead/behind y remotos.
- [x] Modelar operaciones en progreso: merge, rebase, cherry-pick, revert y bisect.
- [x] Manejar symlinks, mount points, paths largos y case sensitivity.

## 1.3 Status y configuración

- [x] Ejecutar `git status --porcelain=v2 -z`.
- [x] Implementar parser independiente de la UI.
- [x] Conservar por separado el estado del index y del working tree.
- [x] Soportar clean, modified, added, deleted, renamed, copied, untracked, ignored y conflict.
- [x] Conservar rutas original/destino en renames.
- [x] Implementar lectura básica de Git config mostrando valor y origen.
- [x] Evitar sobrescribir configuraciones no modificadas por el usuario.

## 1.4 Tests del core

- [x] Crear repositorios temporales aislados por test.
- [x] Cubrir repositorio clean.
- [x] Cubrir cambios modified, staged, unstaged y combinados.
- [x] Cubrir untracked e ignored.
- [x] Cubrir added, deleted, renamed y paths Unicode.
- [x] Cubrir conflictos reales y stages de index.
- [x] Cubrir detached HEAD, bare, submodule y worktree cuando corresponda.
- [x] Comprobar que los tests no leen ni alteran repositorios personales.

## 1.5 Aplicación mínima funcional

- [x] Aceptar una ruta de repositorio por línea de comandos.
- [x] Mostrar raíz, branch y resumen de status.
- [x] Mostrar fallos de Git de forma gráfica y permitir inspeccionar la salida original.
- [x] Ejecutar status sin bloquear la interfaz.
- [x] Añadir smoke test manual documentado.

## Criterio de salida de la Fase 1

- [x] La aplicación muestra correctamente clean, modified, staged y untracked.
- [x] El parser está cubierto por unit e integration tests.
- [x] Todos los jobs obligatorios de CI pasan.
- [x] `STATUS.md` y `CHANGELOG.md` reflejan el estado real.
- [x] Se etiqueta y publica [`v0.1.0`](https://github.com/luks95/linuxgitshell/releases/tag/v0.1.0)
  con notas y checksums verificados.

---

# Fase 2 — Menú contextual de Dolphin (`v0.2.0`, parte 1)

## Implementación

- [x] Investigar y documentar la API KF6 vigente para plugins de Dolphin.
- [x] Crear un plugin pequeño que delegue acciones al core o a procesos externos de LinuxGitShell.
- [x] Detectar selección de archivo/carpeta, raíz y pertenencia a repositorio.
- [x] Limitar acciones cuando la selección atraviesa repositorios diferentes.
- [ ] Fuera de un repositorio: mostrar `Git Clone...` y `Git Create repository here...`.
  Pendiente de que la aplicación implemente clonado y creación; hoy no se muestra ninguna entrada.
- [ ] Dentro de un repositorio: mostrar `Show Status`, `Commit`, `Pull`, `Push`, `Show Log` y `Settings`.
  `Show Status` está disponible; las demás acciones se añadirán cuando la aplicación las implemente.
- [x] Adaptar acciones para repositorios bare y operaciones Git en progreso.
- [ ] Abrir Settings y demás ventanas como aplicaciones externas con contexto de repositorio.
- [x] Evitar cualquier status síncrono pesado en el proceso de Dolphin.
- [x] Añadir logging diagnóstico para carga del plugin sin registrar rutas sensibles por defecto.
- [x] Diseñar un cache de contexto asíncrono que soporte `.git` como archivo/directorio y repos bare,
  con fallback frío, límites, invalidación y presupuesto de latencia ([#10](https://github.com/luks95/linuxgitshell/issues/10)).
- [x] Implementar el modelo de snapshots y el cache LRU acotado, sin Git, filesystem ni IPC.
- [x] Implementar el servicio de contexto fuera de Dolphin sobre `RepositoryDiscovery`
  (`linuxgitshell-daemon`, interfaz experimental `org.linuxgitshell.Experimental.Context1`).
- [x] Implementar el cliente asíncrono de snapshots en el plugin, sin Git ni IPC síncrona en
  `actions()`.

## Verificación

- [x] Documentar instalación de desarrollo y layout de sistema mediante `DESTDIR`; el paquete del
  sistema sigue diferido a la fase de packaging.
- [x] Documentar desinstalación limpia y recarga/reinicio de Dolphin.
- [ ] Verificar menú sobre raíz, subcarpeta, archivo, multiselección y fuera de repo.
- [ ] Verificar que un error o cierre de LinuxGitShell no derriba Dolphin.
- [x] Crear checklist de prueba manual para versiones soportadas de Dolphin.

## Criterio de salida de la Fase 2

- [ ] El plugin carga de forma estable en Dolphin.
- [ ] Las acciones solo aparecen cuando son válidas.
- [ ] Las acciones abren flujos conectados al Git Core.
- [ ] Dolphin permanece responsivo en repositorios grandes o lentos.

---

# Fase 3 — Overlays MVP (`v0.2.0`, parte 2)

## Implementación

- [ ] Crear iconos propios y libres para clean, modified, added, conflict y untracked.
- [ ] Conservar fuente, licencia y atribución de cada recurso visual.
- [ ] Diseñar iconos legibles en tamaños pequeños, HiDPI, tema claro y oscuro.
- [ ] Implementar proveedor de overlays con cache local acotado.
- [ ] Evitar ejecutar `git status` por cada solicitud de icono.
- [ ] Invalidar el cache después de cambios Git conocidos.
- [ ] Añadir deleted e ignored cuando el modelo base sea estable.
- [ ] Diseñar propagación recursiva de estado de carpetas como opción desactivable/configurable.
- [ ] Definir precedencia visual para estados combinados sin perder el detalle en el modelo.

## Pruebas y rendimiento

- [ ] Verificar actualización tras editar, crear, borrar, stagear y resolver un archivo.
- [ ] Medir latencia de paint/request sin cache y con cache caliente.
- [ ] Probar árbol profundo y repositorio con gran cantidad de archivos.
- [ ] Probar montaje externo y documentar limitaciones de montaje de red.
- [ ] Confirmar que desactivar overlays elimina su coste adicional.

## Criterio de salida de la Fase 3

- [ ] Los cinco estados MVP se muestran correctamente.
- [ ] Los overlays se actualizan sin reiniciar Dolphin.
- [ ] No existe una ejecución Git descontrolada por paint/request.
- [ ] Se publica `v0.2.0` con procedimiento de instalación y desinstalación probado.

---

# Fase 4 — Daemon, D-Bus y cache (`v0.3.0`)

## Daemon

- [ ] Crear `linuxgitshell-daemon` como servicio de sesión del usuario.
- [ ] Implementar registro y descubrimiento eficiente de múltiples repositorios.
- [ ] Cachear la relación `path -> repository root` con invalidación correcta.
- [ ] Implementar límites de memoria, expiración y diagnóstico del cache.
- [ ] Excluir `/proc`, `/sys`, `/dev` y `/run` del descubrimiento general.
- [ ] Mantener separadas exclusiones de performance y reglas `.gitignore`.

## Watchers

- [ ] Definir una abstracción para watchers.
- [ ] Evaluar `QFileSystemWatcher` e inotify con métricas reales.
- [ ] Vigilar metadata relevante: index, HEAD, refs y estados de operaciones.
- [ ] Evitar vigilancia recursiva ingenua de millones de archivos.
- [ ] Implementar debounce de ráfagas y refresh en background.
- [ ] Manejar límites de inotify y degradar de manera explícita.
- [ ] Detectar repositorios movidos, desmontados o eliminados.

## API D-Bus

- [ ] Adoptar un nombre versionado, por ejemplo `org.linuxgitshell.Daemon1`.
- [ ] Definir introspección en XML y tipos estables.
- [ ] Implementar `GetRepository`, `GetStatus`, `GetRepositoryStatus`, `RefreshRepository`, `RegisterRepository` y `UnregisterRepository`.
- [ ] Implementar señales `StatusChanged`, `RepositoryChanged` y `DaemonStateChanged`.
- [ ] Definir límites de confianza, validación de paths y comportamiento ante clientes maliciosos.
- [ ] Añadir timeouts y fallback para daemon no disponible.
- [ ] Documentar compatibilidad y evolución de la API.

## Integración y resiliencia

- [ ] Migrar contexto y overlays a consultas rápidas por IPC.
- [ ] Reiniciar el daemon sin cerrar Dolphin.
- [ ] Recuperar el registro/cache después de un crash sin datos corruptos.
- [ ] Añadir métricas diagnósticas: repos observados, entradas, memoria estimada y último refresh.
- [ ] Añadir integration tests de D-Bus y debounce.
- [ ] Someter el daemon a pruebas de carga con varios repositorios.

## Criterio de salida de la Fase 4

- [ ] Una edición actualiza el overlay mediante watcher, cache y señal D-Bus.
- [ ] Dolphin nunca espera una operación Git pesada.
- [ ] La caída o ausencia del daemon se maneja sin derribar clientes.
- [ ] La API D-Bus está versionada, probada y documentada.
- [ ] Se publica `v0.3.0`.

---

# Fase 5 — Working Tree y Commit (`v0.4.0`, parte 1)

## Working Tree

- [ ] Crear tabla de archivos con path, estado, staged/unstaged y estadísticas.
- [ ] Añadir filtros para modified, added, deleted, conflict, untracked, ignored, staged y unstaged.
- [ ] Permitir selección múltiple y operaciones por lote.
- [ ] Implementar stage y unstage con actualización asíncrona del modelo.
- [ ] Implementar restore/revert de archivos con explicación y confirmación apropiadas.
- [ ] Diferenciar claramente restore de archivo, revert de commit y reset de branch.
- [ ] Abrir el diff del elemento seleccionado.

## Commit

- [ ] Crear selección de archivos y panel de diff.
- [ ] Añadir mensaje e historial local de mensajes recientes.
- [ ] Validar mensaje, selección y commit vacío antes de ejecutar.
- [ ] Implementar amend con advertencia cuando corresponda.
- [ ] Integrar firma GPG/SSH gradualmente sin administrar secretos.
- [ ] Permitir autor alternativo únicamente de forma explícita.
- [ ] Mostrar progreso, resultado y salida Git completa.
- [ ] Respetar hooks normales del repositorio y comunicar sus errores.
- [ ] Diseñar stage parcial por hunk como extensión posterior sin bloquear el MVP.

## Criterio de salida de la Fase 5

- [ ] Stage, unstage y commit funcionan sobre selecciones reales.
- [ ] Amend y errores de hooks se presentan de forma comprensible.
- [ ] Ninguna operación bloquea la UI.
- [ ] Los integration tests cubren el flujo diario completo.

---

# Fase 6 — Diff Viewer (`v0.4.0`, parte 2)

## Modelo y parser

- [ ] Modelar `DiffFile`, `DiffHunk` y `DiffLine` fuera de la UI.
- [ ] Parsear added, deleted, rename, binary, Unicode y `no newline at EOF`.
- [ ] Añadir límites y advertencias para diffs enormes.
- [ ] No implementar inicialmente un algoritmo de diff propio; utilizar la salida estructurada de Git.

## Interfaz

- [ ] Implementar vista unified.
- [ ] Implementar vista side-by-side con alineación y scroll sincronizado.
- [ ] Añadir números de línea y navegación entre cambios.
- [ ] Añadir búsqueda, copia y regiones sin cambios colapsables.
- [ ] Añadir whitespace visible e ignore-whitespace configurables.
- [ ] Integrar syntax highlighting con KTextEditor/KSyntaxHighlighting cuando sea adecuado.
- [ ] Soportar tema de sistema, claro, oscuro, HiDPI y teclado.
- [ ] Detectar binarios y ofrecer una representación segura.

## Comparaciones

- [ ] Working tree vs index.
- [ ] Working tree vs HEAD.
- [ ] Index vs HEAD.
- [ ] Commit vs parent.
- [ ] Commit vs commit.
- [ ] Branch vs branch y versión de archivo vs versión de archivo.

## Criterio de salida de la Fase 6

- [ ] Las comparaciones prioritarias muestran datos correctos en ambos modos.
- [ ] Diffs grandes no congelan ni agotan memoria sin advertencia.
- [ ] Hay tests del parser y pruebas de UI documentadas.
- [ ] Se publica `v0.4.0`.

---

# Fase 7 — Log e historial de archivo (`v0.5.0`)

## Datos

- [ ] Implementar parser/modelo de commits, padres, refs, firmas y estadísticas.
- [ ] Implementar paginación o lazy loading.
- [ ] Construir lanes a partir de padres; no basar la UI final en `git log --graph` ASCII.
- [ ] Soportar repositorios con merges, octopus merges y detached HEAD.

## Interfaz y acciones

- [ ] Mostrar graph, hash, message, author, date y badges de refs.
- [ ] Mostrar detalles completos, padres, firma, archivos y estadísticas.
- [ ] Filtrar por texto, autor, email, path, fecha, branch, tag y hash.
- [ ] Comparar con parent, working tree y commit seleccionado.
- [ ] Crear branch/tag, cherry-pick, revert, reset, browse y blame desde un commit.
- [ ] Exigir confirmación fuerte en reset y explicar soft/mixed/hard.
- [ ] Implementar historial por archivo con seguimiento opcional de renames.
- [ ] Permitir guardar una revisión antigua sin modificar el working tree.

## Criterio de salida de la Fase 7

- [ ] Un historial grande carga incrementalmente y permanece responsivo.
- [ ] Los grafos con merges se representan correctamente.
- [ ] Filtros y acciones operan sobre la revisión esperada.
- [ ] Se publica `v0.5.0`.

---

# Fase 8 — Resolución de conflictos (`v0.6.0`)

## Core de conflictos

- [ ] Detectar archivos conflictuados y tipo de conflicto.
- [ ] Leer `git ls-files -u` y blobs de stages 1/2/3.
- [ ] Modelar BASE, OURS, THEIRS y RESULT sin depender solo de marcadores textuales.
- [ ] Probar modify/modify, add/add, delete/modify, rename/rename, rename/delete y file/directory gradualmente.
- [ ] Preservar encoding, fin de línea y permisos cuando sea posible.

## Merge de tres vías

- [ ] Mostrar BASE, LOCAL, REMOTE y RESULT.
- [ ] Navegar conflictos con contador de progreso.
- [ ] Resolver por bloque: local, remote, ambos órdenes o edición manual.
- [ ] Validar que no queden conflictos sin decidir antes de marcar resuelto.
- [ ] Permitir guardar trabajo y cancelar sin pérdida de datos.
- [ ] Integrar con merge, rebase y cherry-pick en progreso.
- [ ] Diferenciar cerrar la UI de abortar la operación Git.
- [ ] Añadir comparación de imágenes como tarea post-1.0.

## Criterio de salida de la Fase 8

- [ ] Los conflictos de texto prioritarios se resuelven por bloque.
- [ ] El resultado se stagea solo con acción explícita del usuario.
- [ ] Cancelar o cerrar no destruye cambios.
- [ ] Existen repositorios fixture/integration tests para conflictos reales.
- [ ] Se publica `v0.6.0`.

---

# Fase 9 — Branches, merge y tags (`v0.7.0`)

## Branches

- [ ] Crear árbol de branches locales y remotos.
- [ ] Implementar create, switch/checkout, rename y delete.
- [ ] Implementar set/unset upstream y push/delete de branch remoto.
- [ ] Añadir compare y show log por branch.
- [ ] Verificar cambios locales antes de switch/checkout.
- [ ] Confirmar delete forzado y explicar commits que quedarían sin referencia.

## Merge

- [ ] Mostrar branch origen, destino y estrategia aplicable.
- [ ] Ejecutar merge de forma asíncrona y mostrar salida completa.
- [ ] Detectar fast-forward, merge commit, conflictos y estado en progreso.
- [ ] Integrar Continue/Abort/Resolve con el resolver común.

## Tags

- [ ] Crear tags lightweight y annotated.
- [ ] Añadir firma de tag cuando Git y la identidad estén configurados.
- [ ] Implementar delete local/remoto y push uno/todos.
- [ ] Mostrar mensaje, target y estado de firma.

## Criterio de salida de la Fase 9

- [ ] Los flujos principales de branches, merge y tags son gráficos y probados.
- [ ] Las operaciones destructivas muestran impacto y requieren confirmación.
- [ ] Se publica `v0.7.0`.

---

# Fase 10 — Red y sincronización (`v0.8.0`)

## Infraestructura común

- [ ] Crear `GitOperation` con started, progress, stdout/stderr, finished, failed y cancelled.
- [ ] Crear diálogo de progreso reutilizable.
- [ ] Mostrar progreso indeterminado cuando Git no ofrezca porcentaje real.
- [ ] Definir cancelación segura por etapa de operación.
- [ ] Integrar OpenSSH, ssh-agent, KWallet/credential helpers existentes sin almacenar secretos propios.
- [ ] Detectar y redactar credenciales en argumentos, salida y logs.
- [ ] Funcionar con cualquier remoto Git compatible, no solo GitHub/GitLab.

## Fetch, pull y push

- [ ] Fetch por remoto/todos, prune y tags.
- [ ] Pull con merge, rebase y ff-only.
- [ ] Push con branch local/remota, tags y set upstream.
- [ ] Ofrecer `force-with-lease` antes que force y explicar el riesgo.
- [ ] Traducir auth failure, host key, non-fast-forward y remote unavailable a mensajes accionables.
- [ ] Mantener siempre accesible la salida Git completa.

## Sync

- [ ] Mostrar branch local/remota y ahead/behind.
- [ ] Mostrar commits incoming y outgoing con acceso al Log.
- [ ] Integrar Fetch, Pull y Push sin duplicar lógica.

## Criterio de salida de la Fase 10

- [ ] Fetch, pull y push funcionan por SSH, HTTPS y remoto local en escenarios probados.
- [ ] Ningún secreto aparece en logs o diagnósticos.
- [ ] Los errores frecuentes ofrecen recuperación segura.
- [ ] Se publica `v0.8.0`.

---

# Fase 11 — Rebase, cherry-pick y stash (`v0.9.0`, parte 1)

## Rebase

- [ ] Implementar rebase normal con preflight de working tree.
- [ ] Mostrar estado, commit actual y total cuando Git lo permita.
- [ ] Integrar Resolve, Continue, Skip y Abort.
- [ ] Implementar rebase interactivo: pick, reword, edit, squash, fixup y drop.
- [ ] Permitir reordenamiento accesible por teclado además de drag & drop.
- [ ] Mostrar exactamente el plan antes de iniciar.

## Cherry-pick

- [ ] Seleccionar uno o varios commits y confirmar el orden.
- [ ] Soportar commit automático, `--no-commit` y mainline parent cuando aplique.
- [ ] Integrar conflictos y Continue/Abort.

## Stash

- [ ] Listar nombre, mensaje, fecha y branch de origen.
- [ ] Crear stash con include-untracked y keep-index.
- [ ] Ver cambios, apply, pop y create branch.
- [ ] Confirmar drop y clear; mostrar el alcance exacto.

## Criterio de salida de la Fase 11

- [ ] Los tres flujos sobreviven a conflictos y reinicio de la UI.
- [ ] El estado Git en progreso nunca se oculta.
- [ ] Integration tests cubren éxito, conflicto y abort.

---

# Fase 12 — Historial avanzado (`v0.9.0`, parte 2)

## Revision Graph

- [ ] Crear grafo interactivo con zoom, pan, fit, minimap y búsqueda.
- [ ] Mostrar labels locales, remotos y tags con context menu.
- [ ] Validar rendimiento con historias extensas y muchas ramas.

## Reflog

- [ ] Mostrar selector, hash, acción, mensaje y fecha.
- [ ] Añadir show log, create branch, compare y copy hash.
- [ ] Marcar reset/recovery como peligrosos y explicar su efecto.

## Blame

- [ ] Mostrar commit, autor, fecha, línea y código.
- [ ] Abrir commit en Log y navegar a la revisión previa.
- [ ] Añadir compare, copy hash y copy line.
- [ ] Cargar incrementalmente archivos grandes.

## Repository Browser y referencias

- [ ] Navegar el árbol de cualquier revisión sin tocar el working tree.
- [ ] Ver, abrir, guardar, comparar y consultar historial/blame de un archivo.
- [ ] Mostrar árbol `refs/heads`, `refs/remotes` y `refs/tags`.
- [ ] Añadir acciones contextuales seguras por tipo de referencia.

## Criterio de salida de la Fase 12

- [ ] Las cuatro herramientas comparten modelos y acciones del core.
- [ ] Historias y archivos grandes permanecen utilizables.
- [ ] Se publica `v0.9.0`.

---

# Fase 13 — Flujos avanzados candidatos a 1.0

Estas funciones entrarán en 1.0 solo si alcanzan el mismo nivel de estabilidad. De lo contrario, se moverán explícitamente a 1.x sin bloquear la calidad del núcleo.

## Worktrees

- [ ] Listar path, branch, HEAD, locked y prunable mediante formato porcelain.
- [ ] Crear, abrir, lock/unlock, prune y remove.
- [ ] Detectar cambios antes de remove y solicitar confirmación fuerte.

## Submodules

- [ ] Listar path, URL, commit actual/esperado, branch y status.
- [ ] Add, initialize, update, sync y abrir repositorio.
- [ ] Mostrar diff del gitlink y estado anidado.
- [ ] Tratar deinitialize/remove como funciones avanzadas y destructivas.

## Bisect

- [ ] Crear wizard para good/bad revision.
- [ ] Mostrar commit actual y acciones Good/Bad/Skip.
- [ ] Mostrar el primer commit malo y abrirlo en Log.
- [ ] Permitir abort seguro.

## Patches, export y clean

- [ ] Crear/previsualizar/aplicar patch y mailbox/series.
- [ ] Integrar errores y conflictos con componentes comunes.
- [ ] Exportar snapshot de commit/branch/tag sin `.git` por defecto.
- [ ] Implementar Clean con dry-run obligatorio antes de borrar.
- [ ] Mostrar la lista exacta y volver a validar antes de ejecutar clean real.

## Compatibilidad avanzada

- [ ] Detectar Git LFS y explicar si falta el ejecutable.
- [ ] Detectar sparse checkout y evitar operaciones incompatibles.
- [ ] Adaptar todas las acciones a bare repos.

## Criterio de salida de la Fase 13

- [ ] Cada función incluida tiene tests, documentación y manejo de operaciones parciales.
- [ ] Toda eliminación tiene preview o confirmación proporcional al riesgo.
- [ ] Las funciones que no alcancen calidad se documentan como roadmap 1.x, no como soporte actual.

---

# Fase 14 — Settings, accesibilidad e internacionalización

## Settings

- [ ] Crear `linuxgitshell-settings` externo al plugin de Dolphin.
- [ ] Implementar General, Git, Context Menu, Icon Overlays y Status Cache.
- [ ] Implementar Network, SSH, Diff, Merge, Log y Commit.
- [ ] Implementar Hooks, Repository, Appearance, Integrations, Diagnostics y Advanced.
- [ ] Editar Git config por scope system/global/repository mostrando origen y valor efectivo.
- [ ] No sobrescribir valores que el usuario no cambió.
- [ ] Configurar acciones visibles, ubicación, orden y perfiles Simple/Developer/Advanced/Custom.
- [ ] Configurar overlays y propagación recursiva.
- [ ] Mostrar estado y controles seguros del daemon/cache.
- [ ] Permitir diff/merge tool built-in, KDiff3, Meld, VS Code y comando custom validado.
- [ ] Detectar SSH, agent, known_hosts y claves públicas sin exponer contenido privado.
- [ ] No modificar `~/.ssh/config` sin consentimiento explícito.

## Temas y accesibilidad

- [ ] Integrar tema System/Light/Dark sin colores hardcodeados.
- [ ] Validar HiDPI, high contrast y tamaños de fuente del sistema.
- [ ] Garantizar navegación completa por teclado y orden de foco.
- [ ] Añadir labels para screen readers.
- [ ] No comunicar estados únicamente mediante color.
- [ ] Revisar shortcuts y evitar conflictos con KDE/Dolphin.
- [ ] Ejecutar una auditoría manual de accesibilidad de los flujos 1.0.

## Internacionalización

- [ ] Extraer todas las cadenas traducibles.
- [ ] Mantener inglés como idioma fuente y traducción completa al español.
- [ ] Añadir contexto para traductores y evitar concatenación de frases.
- [ ] Documentar flujo para añadir y probar traducciones.
- [ ] Validar layouts con expansión de texto y pluralización.

## Diagnósticos

- [ ] Mostrar versiones de LinuxGitShell, OS, kernel, Qt, KF6, Plasma, Dolphin y Git.
- [ ] Mostrar estado de daemon y plugins, y tipo de repositorio.
- [ ] Añadir copy diagnostics y open logs.
- [ ] Redactar usuario, credenciales, paths sensibles y contenido del repositorio.
- [ ] Añadir debug logging opt-in con instrucciones para desactivarlo.

## Criterio de salida de la Fase 14

- [ ] Todas las opciones persistentes respetan XDG/KConfig.
- [ ] Los flujos principales son utilizables por teclado y sin depender solo del color.
- [ ] Inglés y español están completos para la funcionalidad 1.0.
- [ ] Un reporte diagnóstico es útil y no filtra secretos.

---

# Fase 15 — Preparación del release comunitario 1.0

## Estabilización

- [ ] Congelar alcance y publicar una lista explícita de funciones diferidas.
- [ ] Resolver bugs críticos y altos; documentar limitaciones aceptadas.
- [ ] Ejecutar pruebas unitarias, de integración, manuales y de regresión completas.
- [ ] Probar repositorios pequeños, monorepos, HDD, disco externo y montaje de red.
- [ ] Medir tiempo de status, consumo de cache, latencia de overlays y carga de log/diff.
- [ ] Ejecutar sanitizers en builds de prueba cuando la toolchain lo permita.
- [ ] Ejecutar análisis estático y corregir findings relevantes.
- [ ] Hacer revisión de seguridad de ejecución de procesos, paths, D-Bus, logs y comandos custom.
- [ ] Hacer revisión de licencias de código, dependencias, traducciones, iconos y screenshots.

## Packaging Arch/Manjaro

- [ ] Crear `PKGBUILD` siguiendo las guías vigentes de Arch.
- [ ] Separar dependencias de build, runtime y opcionales.
- [ ] Instalar binarios, plugin, D-Bus, desktop files, metainfo, iconos y traducciones en rutas correctas.
- [ ] Validar paquete con herramientas de lint de Arch.
- [ ] Probar instalación, upgrade y desinstalación limpia en un entorno nuevo.
- [ ] Documentar archivos instalados y pasos de troubleshooting.
- [ ] Evaluar envío a AUR después de contar con tarballs firmados y proceso de release estable.

## Distribución y metadatos

- [ ] Añadir AppStream/metainfo con licencia, descripción, URLs y screenshots reales.
- [ ] Añadir desktop entries y MIME/URL handling solo donde corresponda.
- [ ] Crear iconos de aplicación propios en tamaños/formato requeridos.
- [ ] Preparar screenshots sin datos personales ni repositorios privados.
- [ ] Crear tarball de fuentes desde un tag inmutable.
- [ ] Publicar checksums y firma del release.
- [ ] Generar SBOM o inventario de dependencias del artefacto cuando sea viable.
- [ ] Verificar que el tarball permite build offline con dependencias del sistema instaladas.

## Documentación de usuario y comunidad

- [ ] Completar instalación, desinstalación, quick start y guía de troubleshooting.
- [ ] Documentar todos los flujos 1.0 y sus riesgos.
- [ ] Publicar guía para reportar bugs con diagnósticos sanitizados.
- [ ] Publicar guía para probar plugins y overlays en Dolphin.
- [ ] Revisar CONTRIBUTING con una contribución simulada desde cero.
- [ ] Preparar issues `good first issue` realmente acotados y mentoreables.
- [ ] Reconocer contribuidores y dependencias en release notes.

## Release candidates

- [ ] Publicar `v1.0.0-beta.1` y abrir periodo de pruebas comunitarias.
- [ ] Recopilar feedback con versión exacta, entorno y pasos reproducibles.
- [ ] Publicar al menos un release candidate después de corregir bloqueantes.
- [ ] Ejecutar el checklist completo sobre el commit exacto del tag final.
- [ ] Publicar `v1.0.0` con changelog, artefactos, checksums, firma y known issues.
- [ ] Definir calendario y responsables de parches `1.0.x`.

---

# Gate obligatorio para V1.0

## Funcionalidad mínima

- [ ] Menú contextual de Dolphin.
- [ ] Overlays de status mediante daemon/cache.
- [ ] Working Tree, stage/unstage y Commit GUI.
- [ ] Diff unified y side-by-side.
- [ ] Log, file history y revision graph.
- [ ] Create/switch/delete branches y tags básicos.
- [ ] Fetch, pull, push y Sync.
- [ ] Merge y resolver de conflictos de tres vías.
- [ ] Stash básico, rebase básico y cherry-pick básico.
- [ ] Reflog, blame y repository browser.
- [ ] Settings y diagnósticos.
- [ ] Paquete Arch/Manjaro.
- [ ] Inglés y español.

## Calidad y seguridad

- [ ] Configure, build y todos los tests pasan desde un checkout limpio.
- [ ] No hay crashes conocidos reproducibles en flujos principales.
- [ ] No hay bugs abiertos de pérdida/corrupción de datos.
- [ ] Ninguna operación Git pesada bloquea Dolphin.
- [ ] Las operaciones destructivas tienen confirmaciones y defaults seguros.
- [ ] No se filtran secretos en logs, diagnósticos o mensajes de error.
- [ ] Los plugins toleran la ausencia o reinicio del daemon y de la GUI.
- [ ] Instalación, actualización y desinstalación están probadas.
- [ ] Accesibilidad por teclado y temas claro/oscuro están verificadas.
- [ ] Licencias y atribuciones están completas.

## Proyecto abierto y sostenible

- [ ] Issues y pull requests tienen plantillas y proceso documentado.
- [ ] CI obligatorio protege la rama principal.
- [ ] SECURITY, GOVERNANCE, CONTRIBUTING y Code of Conduct están vigentes.
- [ ] Existe un canal mantenido para preguntas y decisiones de diseño.
- [ ] El roadmap, STATUS y CHANGELOG coinciden con el producto publicado.
- [ ] El procedimiento de release puede ser ejecutado por más de un maintainer.

---

# Post-1.0 — Expansión

El orden de estas tareas se decidirá con uso real, métricas e interés comunitario.

- [ ] Rebase interactivo avanzado.
- [ ] Stage parcial por hunk y por líneas.
- [ ] Comparación visual de imágenes.
- [ ] Patch series avanzado.
- [ ] Managers completos de submodules y worktrees.
- [ ] Bisect GUI completo.
- [ ] Administración de Git LFS y sparse checkout.
- [ ] Dashboard de uno y múltiples repositorios.
- [ ] Command palette, favoritos y repositorios recientes.
- [ ] Integraciones opcionales con GitHub, GitLab, Gitea y Forgejo sobre una capa separada.
- [ ] Investigar integración con Nautilus.
- [ ] Investigar integración con Nemo.
- [ ] Investigar integración con Thunar.
- [ ] Preparar paquetes Debian/Ubuntu, Fedora y openSUSE.
- [ ] Reevaluar Flatpak únicamente después de validar acceso al host e integración binaria con file managers.

## Requisitos para una nueva integración de file manager

- [ ] Reutilizar Git Core, modelos, daemon y API D-Bus sin forks específicos.
- [ ] Aislar el adaptador y respetar el modelo de extensiones del file manager.
- [ ] No degradar seguridad, rendimiento ni compatibilidad de la integración Dolphin.
- [ ] Añadir instalación, desinstalación, diagnósticos y tests propios.
- [ ] Documentar claramente nivel de soporte y maintainers responsables.

---

# Checklist transversal por pull request

- [ ] El cambio tiene issue o explicación clara de problema y alcance.
- [ ] La implementación mantiene los límites core/daemon/IPC/UI/plugin.
- [ ] No introduce una invocación Git directa desde widgets o plugins.
- [ ] No bloquea el hilo de UI.
- [ ] Los argumentos de proceso se pasan de forma segura y sin shell.
- [ ] Errores y cancelaciones quedan manejados.
- [ ] Se añadieron o actualizaron tests proporcionales al riesgo.
- [ ] Se actualizaron documentación y traducciones si cambió comportamiento visible.
- [ ] Se revisaron accesibilidad y navegación por teclado si cambió UI.
- [ ] Se revisaron logs y diagnósticos para evitar secretos.
- [ ] Se revisaron licencias/atribuciones si se añadió código o un asset externo.
- [ ] Configure, build, tests y checks de formato pasan.
- [ ] `STATUS.md` se actualizó si cambió fase, capacidad o limitación importante.
- [ ] `CHANGELOG.md` se actualizó si el cambio afecta al usuario.

# Definition of Done para cualquier fase

- [ ] Compila desde un checkout limpio con dependencias documentadas.
- [ ] Todos los tests anteriores y nuevos pasan.
- [ ] No rompe flujos entregados en fases previas.
- [ ] La lógica crítica tiene tests automatizados.
- [ ] La prueba manual específica está documentada y ejecutada.
- [ ] Los errores comunes son accionables y conservan la salida Git original.
- [ ] No existen rutas del desarrollador, credenciales ni datos privados hardcodeados.
- [ ] Rendimiento medido cuando el cambio toca status, watchers, overlays, log o diff.
- [ ] Documentación, traducciones, STATUS y CHANGELOG están sincronizados.
- [ ] Instalación y desinstalación siguen siendo limpias.
- [ ] Los criterios de salida de la fase están cumplidos.

# Riesgos a vigilar

| Riesgo | Mitigación y señal de control |
| --- | --- |
| Dolphin se congela | IPC rápido, cache, operaciones asíncronas y mediciones de latencia. |
| Watchers no escalan | Abstracción, debounce, límites, expiración y pruebas con monorepos. |
| Parsing frágil | Formatos porcelain/NUL, fixtures y tests contra versiones soportadas de Git. |
| Pérdida de datos | Preflight, previews, confirmaciones, variantes seguras y tests de abort/cancel. |
| Fuga de secretos | Redacción centralizada, revisión de logs y pruebas de URLs con credenciales ficticias. |
| Acoplamiento a KDE/Dolphin | Core y daemon sin UI; adaptadores delgados sobre una API versionada. |
| Scope excesivo | Gate por fases, versiones pequeñas y funciones avanzadas movibles a 1.x. |
| Dependencia de una persona | Gobernanza, documentación de release, code review y transferencia de ownership. |
| Licencias incompatibles | Inventario SPDX y revisión antes de incorporar código o assets. |
| Expectativas incorrectas | README, STATUS, changelog y matriz de soporte siempre actualizados. |

# Métricas de salud sugeridas

- [ ] Tiempo de status inicial y de refresh incremental en repositorio pequeño/grande.
- [ ] Latencia p50/p95 de consultas de overlays con cache caliente.
- [ ] Memoria del daemon por repositorio y por cantidad de entradas.
- [ ] Tiempo de carga inicial y de siguiente página del Log.
- [ ] Tiempo/memoria de diff pequeño, grande y truncado.
- [ ] Tasa de éxito de CI y duración de jobs.
- [ ] Bugs de pérdida de datos o crash por release.
- [ ] Tiempo de primera respuesta y resolución de issues comunitarios.
- [ ] Cobertura de traducción por idioma.
- [ ] Cantidad de contribuidores y maintainers activos, sin convertirla en objetivo de vanidad.

---

## Próxima tarea recomendada

Completar el checklist interactivo de `docs/dolphin-context-menu.md` en Dolphin/Wayland con el menú
según el repositorio (`Show Status`, avisos de operaciones y multiselección), medir la latencia
nativa de `actions()` (p50/p95/máximo) y adjuntar capturas. Con eso se pueden evaluar los criterios de
salida de la Fase 2; las acciones `Commit`, `Pull`, `Push`, `Show Log`, `Settings`, `Git Clone` y
`Create repository here` se añadirán al menú cuando la aplicación las implemente.
