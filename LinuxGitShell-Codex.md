# LinuxGitShell — Especificación maestra para Codex

> **Objetivo:** desarrollar para la comunidad Linux una integración gráfica de Git inspirada en la experiencia de TortoiseGit, comenzando por **KDE Plasma 6 + Dolphin** en **Manjaro/Arch Linux**, con la mayor cantidad posible de operaciones Git accesibles mediante interfaz gráfica, menú contextual, iconos de estado, historial visual, diff, resolución de conflictos, ramas, rebase, stash, reflog, submódulos, worktrees y configuración completa.

> **Estado al 2026-09-13:** Fases 0 y 1 completadas; `v0.1.0` publicada. La Fase 2 está en curso y
> ya incluye el primer plugin KF6 de Dolphin para abrir una selección local en la aplicación externa.
> El contexto de repositorio no bloqueante está diseñado en
> [`docs/repository-context-cache.md`](docs/repository-context-cache.md) y su implementación se sigue
> en [#12](https://github.com/luks95/linuxgitshell/issues/12). `STATUS.md` y
> `roadmap-checklist.md` son las fuentes de verdad para el progreso, mientras este documento conserva
> el alcance completo del producto.

---

## 1. Instrucción principal para Codex

Quiero que trabajes como arquitecto y desarrollador principal de este proyecto.

No quiero un simple conjunto de scripts ni solo un Service Menu de Dolphin. Quiero construir una aplicación de escritorio seria, modular, mantenible y preparada para ser publicada como software libre.

El producto debe sentirse como un **“TortoiseGit para Linux”**, pero implementado de forma nativa para Linux/KDE y sin depender de Windows.

### Prioridades

1. Integración profunda con Dolphin.
2. Experiencia completamente gráfica siempre que sea razonable.
3. Iconos/overlays de estado Git sobre archivos y carpetas.
4. Menú contextual dinámico según el estado del archivo/repositorio.
5. Commit gráfico.
6. Historial/log gráfico.
7. Comparación visual de código.
8. Resolución gráfica de conflictos con merge de tres vías.
9. Branches, tags, stash, rebase, cherry-pick, reflog, submodules y worktrees.
10. Configuración gráfica equivalente o superior a TortoiseGit.
11. Buen rendimiento incluso con repositorios grandes.
12. Arquitectura reutilizable en el futuro para Nautilus, Nemo y Thunar.

### Regla de trabajo

No intentes implementar todo en una sola pasada.

Trabaja por fases, manteniendo el proyecto compilable y ejecutable al finalizar cada fase.

Antes de modificar código:

1. inspecciona el repositorio actual;
2. detecta qué ya existe;
3. revisa el entorno instalado;
4. crea un plan corto de la fase;
5. implementa;
6. compila;
7. ejecuta tests;
8. informa qué quedó funcionando y qué sigue pendiente.

No destruyas código funcional para rehacerlo sin necesidad.

---

# 2. Nombre provisional

Usar inicialmente:

```text
LinuxGitShell
```

Nombres de binarios sugeridos:

```text
linuxgitshell
linuxgitshell-daemon
linuxgitshell-settings
```

Nombre D-Bus versionado sugerido:

```text
org.linuxgitshell.Daemon1
```

Mantener el nombre desacoplado internamente para poder cambiar el branding más adelante.

---

# 3. Plataforma inicial

Desarrollar primero para:

```text
Linux
└── Manjaro / Arch Linux
    └── KDE Plasma 6
        └── Dolphin
```

Entorno tecnológico principal:

```text
C++20 o superior
Qt 6
KDE Frameworks 6
CMake
Extra CMake Modules
Git CLI
D-Bus
KConfig
KIO
KTextEditor / KSyntaxHighlighting cuando corresponda
```

No asumir nombres exactos de paquetes de Arch/Manjaro sin verificarlos primero con `pacman`.

Antes de instalar dependencias, detectar si ya están instaladas.

No ejecutar instalaciones destructivas ni reemplazar componentes del sistema sin necesidad.

---

# 4. Visión de arquitectura

La arquitectura debe separar claramente:

```text
                         Dolphin
                            │
            ┌───────────────┴────────────────┐
            │                                │
    Context Menu Plugin               Overlay Plugin
            │                                │
            └───────────────┬────────────────┘
                            │
                           D-Bus
                            │
                 ┌──────────▼──────────┐
                 │ LinuxGitShellDaemon │
                 │                     │
                 │ repo discovery      │
                 │ status cache        │
                 │ filesystem watcher  │
                 │ Git operations      │
                 └──────────┬──────────┘
                            │
              ┌─────────────┼─────────────┐
              │             │             │
           Git CLI      Application GUI  KConfig
                            │
        ┌───────────────────┼─────────────────────────┐
        │                   │                         │
     Commit               Log/Diff                 Settings
        │                   │                         │
     Branches             Merge                   Network
     Rebase               Blame                   Overlays
     Stash                Graph                   Context menu
```

## Principio obligatorio

Los plugins de Dolphin deben ser pequeños.

No meter toda la lógica Git, ventanas, cache, watchers y operaciones dentro del proceso de Dolphin.

---

# 5. Estructura del repositorio

La implementación actual ya contiene `libs/gitcore/`, `gui/app/`,
`integrations/dolphin/contextmenu/`, `tests/`, `cmake/` y `po/`. Evolucionar hacia una estructura
similar a la siguiente solo al añadir implementaciones reales; no crear directorios vacíos:

```text
linux-git-shell/
│
├── CMakeLists.txt
├── README.md
├── LICENSE
├── CONTRIBUTING.md
├── CHANGELOG.md
├── STATUS.md
│
├── cmake/
│
├── libs/
│   ├── gitcore/
│   │   ├── GitRepository.*
│   │   ├── GitCommand.*
│   │   ├── GitStatus.*
│   │   ├── GitConfig.*
│   │   ├── GitLog.*
│   │   └── GitDiff.*
│   │
│   ├── gitmodel/
│   ├── diff/
│   └── ipc/
│
├── daemon/
│   ├── main.cpp
│   ├── GitDaemon.*
│   ├── RepositoryCache.*
│   ├── RepositoryRegistry.*
│   ├── FileWatcher.*
│   └── DBusService.*
│
├── integrations/
│   └── dolphin/
│       ├── contextmenu/
│       │   └── GitActionPlugin.*
│       └── overlays/
│           └── GitOverlayPlugin.*
│
├── gui/
│   ├── common/
│   ├── dashboard/
│   ├── workingtree/
│   ├── commit/
│   ├── log/
│   ├── diff/
│   ├── merge/
│   ├── graph/
│   ├── blame/
│   ├── branches/
│   ├── references/
│   ├── repobrowser/
│   ├── rebase/
│   ├── stash/
│   ├── bisect/
│   ├── reflog/
│   ├── submodules/
│   ├── worktrees/
│   ├── patch/
│   ├── sync/
│   └── settings/
│
├── dbus/
│   └── org.linuxgitshell.Daemon.xml
│
├── icons/
│   ├── git-clean.svg
│   ├── git-modified.svg
│   ├── git-added.svg
│   ├── git-deleted.svg
│   ├── git-conflict.svg
│   ├── git-untracked.svg
│   └── git-ignored.svg
│
├── packaging/
│   ├── arch/
│   │   └── PKGBUILD
│   ├── debian/
│   └── fedora/
│
└── tests/
```

No crear directorios vacíos innecesariamente. Agregarlos cuando aparezca la funcionalidad correspondiente.

---

# 6. Git backend

## Primera etapa

Usar el Git instalado en el sistema mediante `QProcess`.

No reimplementar Git.

Crear una abstracción central, por ejemplo:

```cpp
class GitService
{
public:
    RepositoryStatus status(const QString &repo);
    LogResult log(const QString &repo, const LogOptions &options);
    DiffResult diff(const DiffRequest &request);

    OperationResult add(...);
    OperationResult commit(...);
    OperationResult fetch(...);
    OperationResult pull(...);
    OperationResult push(...);
    OperationResult checkout(...);
    OperationResult switchBranch(...);
    OperationResult merge(...);
    OperationResult rebase(...);
    OperationResult cherryPick(...);
    OperationResult stash(...);
};
```

Las ventanas no deben ejecutar `git` directamente.

Todo debe pasar por una capa centralizada.

## Comandos útiles

Preferir formatos parseables, especialmente:

```bash
git status --porcelain=v2 -z
git diff --no-ext-diff
git diff --cached --no-ext-diff
git log --format=...
git show
git branch --format=...
git for-each-ref
git reflog
git worktree list --porcelain
git submodule status
git config --list --show-origin
```

Cuando exista formato estable `--porcelain`, preferirlo.

No parsear salida humana de Git si existe una salida diseñada para scripts.

---

# 7. Modelo de estados Git

Definir un modelo independiente de la interfaz gráfica.

Por ejemplo:

```cpp
enum class GitStatus {
    None,
    Clean,
    Modified,
    Added,
    Deleted,
    Renamed,
    Copied,
    Untracked,
    Ignored,
    Conflict
};
```

Considerar también estados combinados del index y working tree.

Ejemplo:

```text
Index: Modified
WorkTree: Modified
```

No perder información simplemente reduciéndolo todo a un solo color.

---

# 8. Integración con Dolphin

## 8.1 Menú contextual

Usar el mecanismo KDE/KF6 apropiado para acciones dinámicas de Dolphin.

El menú debe cambiar según:

- si la ruta pertenece o no a un repositorio;
- si se seleccionó archivo o carpeta;
- cantidad de elementos seleccionados;
- estado Git;
- si hay merge/rebase/cherry-pick en progreso;
- si hay conflictos;
- si el repositorio posee remoto;
- si existe upstream;
- si se está sobre la raíz del repositorio.

### Fuera de un repositorio

Mostrar como mínimo:

```text
Git Clone...
Git Create repository here...
```

### Dentro de un repositorio

El menú objetivo debe poder aproximarse a:

```text
Git Commit "main"...
Git Sync...
────────────────────────
Git >
    Pull...
    Fetch...
    Push...
    Diff
    Diff with previous version
    Show log
    Show Reflog
    Browse References
    Daemon
    Revision graph
    Repo-browser
    Check for modifications
    Rebase...
    Stash changes
    Bisect start
    Resolve...
    Revert...
    Clean up...
    Switch/Checkout...
    Merge...
    Create Branch...
    Create Tag...
    Export...
    Add...
    Worktrees
    Submodule Add...
    Create Patch Serial...
    Apply Patch Serial...
    Settings
```

No todas las acciones deben aparecer siempre.

Ejemplo: `Resolve...` solo tiene sentido si existen conflictos.

## 8.2 Menú configurable

Crear una configuración que permita elegir:

```text
Acción                  Visible       Ubicación
────────────────────────────────────────────────
Commit                   Sí           Principal
Pull                     Sí           Git >
Push                     Sí           Git >
Fetch                    Sí           Git >
Diff                     Sí           Git >
Show Log                 Sí           Git >
Rebase                   Sí           Git >
Stash                    Sí           Git >
Bisect                   No           Git >
Worktrees                Sí           Git >
```

Permitir:

- activar/desactivar acciones;
- poner acciones en menú principal o submenú Git;
- ordenar acciones;
- restaurar valores predeterminados;
- perfiles Simple / Developer / Advanced / Custom.

---

# 9. Overlays de estado

Implementar overlays sobre archivos y carpetas.

Estados visuales mínimos:

```text
Clean
Modified
Added
Deleted
Conflict
Untracked
Ignored
```

El plugin de overlay no debe ejecutar `git status` de forma síncrona cada vez que Dolphin pida un icono.

Debe consultar un cache rápido.

## Estado recursivo de carpetas

Si existe:

```text
repo/
└── src/
    └── main/
        └── App.java    MODIFIED
```

permitir mostrar:

```text
MODIFIED repo/
MODIFIED src/
MODIFIED main/
MODIFIED App.java
```

Hacerlo configurable.

---

# 10. Daemon y cache

Crear un daemon de sesión del usuario.

Responsabilidades:

```text
Repository discovery
Repository registry
Status cache
Filesystem watching
Git metadata watching
D-Bus API
Background refresh
Debounce
Invalidation
```

## Watchers

En Linux considerar:

```text
inotify
QFileSystemWatcher
```

Diseñar una abstracción para no acoplar todo a una sola implementación.

Observar especialmente cambios en:

```text
.git/index
.git/HEAD
.git/refs/
working tree
```

No vigilar recursivamente millones de archivos de manera ingenua.

## Debounce

Agrupar ráfagas de eventos.

Ejemplo conceptual:

```text
file event
file event
file event
file event
      ↓
200 ms debounce
      ↓
one status refresh
```

El tiempo debe ser configurable internamente y luego desde Settings si es útil.

## Repositorios grandes

Evitar degradación grave con:

- monorepos;
- cientos de miles de archivos;
- repositorios sobre HDD;
- repositorios en montajes externos;
- carpetas de red.

Nunca bloquear Dolphin esperando una operación Git pesada.

---

# 11. D-Bus

El daemon debe exponer una API bien definida.

Nombre sugerido:

```text
org.linuxgitshell.Daemon
```

Métodos iniciales conceptuales:

```text
GetRepository(path)
GetStatus(path)
GetRepositoryStatus(repo)
RefreshRepository(repo)
RegisterRepository(repo)
UnregisterRepository(repo)
```

Señales:

```text
StatusChanged(path, status)
RepositoryChanged(repo)
DaemonStateChanged(state)
```

Más adelante agregar operaciones si tiene sentido.

No exponer una API arbitraria sin versionado.

Pensar desde el inicio en una futura API:

```text
org.linuxgitshell.Daemon1
```

---

# 12. Working Tree / Check for modifications

Crear una ventana gráfica para ver el estado del repositorio.

Objetivo visual aproximado:

```text
┌──────────────────────────────────────────────────────────────┐
│ Working Tree - project                      main  ↑2 ↓1       │
├──────────────────────────────────────────────────────────────┤
│ Path                     Status       Added   Removed         │
├──────────────────────────────────────────────────────────────┤
│ ☑ src/User.java          Modified       12       4           │
│ ☑ src/Login.java         Modified        3       1           │
│ ☐ debug.txt              Untracked      20       0           │
│ ⚠ UserController.java    Conflict       14      11           │
├──────────────────────────────────────────────────────────────┤
│ [Diff] [Stage] [Unstage] [Revert]             [Commit...]    │
└──────────────────────────────────────────────────────────────┘
```

Agregar filtros:

```text
Modified
Added
Deleted
Conflicted
Untracked
Ignored
Staged
Unstaged
```

Permitir selección múltiple y operaciones por lote.

---

# 13. Commit gráfico

Crear una ventana completa de commit.

Debe permitir:

- seleccionar archivos;
- stage/unstage;
- ver diff del archivo seleccionado;
- mensaje de commit;
- historial de mensajes recientes;
- amend;
- sign commit;
- autor cuando corresponda;
- detectar commit vacío;
- validar errores antes de ejecutar;
- mostrar resultado completo.

Objetivo visual aproximado:

```text
┌──────────────────────────────────────────────────────┐
│ Commit                                               │
├──────────────────────────────────────────────────────┤
│ Changes                                              │
│ ☑ User.java                  Modified                │
│ ☑ UserService.java           Modified                │
│ ☐ Debug.java                 Untracked               │
├──────────────────────────────────────────────────────┤
│ Diff                                                 │
│ - old line                                           │
│ + new line                                           │
├──────────────────────────────────────────────────────┤
│ Message                                              │
│ ┌──────────────────────────────────────────────────┐ │
│ │ Corrige autenticación                           │ │
│ └──────────────────────────────────────────────────┘ │
│                                                      │
│ ☐ Amend      ☐ Sign commit                           │
│                                      [Commit]        │
└──────────────────────────────────────────────────────┘
```

## Funcionalidad avanzada futura

Agregar stage parcial por hunk y eventualmente por líneas.

---

# 14. Diff Viewer

Implementar un visor de diferencias nativo.

Debe soportar como mínimo:

```text
Working tree vs index
Working tree vs HEAD
Index vs HEAD
Commit vs commit
Commit vs parent
Branch vs branch
File version vs file version
```

## Modos

### Side-by-side

```text
OLD                               NEW
──────────────────                ──────────────────
1 public void save() {           1 public void save() {
2     validate();                2     validate();
3     dao.save();                3     repository.save();
4 }                              4 }
```

### Unified

```diff
@@ -1,4 +1,4 @@
 public void save() {
     validate();
-    dao.save();
+    repository.save();
 }
```

## Requisitos visuales

- números de línea;
- sincronización vertical entre paneles;
- navegación entre cambios;
- búsqueda;
- copiar selección;
- colapsar regiones sin cambios;
- whitespace visible opcional;
- ignorar whitespace configurable;
- syntax highlighting;
- tema claro/oscuro;
- archivos binarios detectados correctamente.

Investigar `KTextEditor` y `KSyntaxHighlighting` antes de reinventar un editor completo.

---

# 15. Resolución gráfica de conflictos

Esta es una funcionalidad prioritaria y debe ser excelente.

## Merge de tres vías

Mostrar:

```text
BASE
LOCAL / OURS
REMOTE / THEIRS
RESULT
```

Objetivo:

```text
┌──────────────────────┬──────────────────────┬──────────────────────┐
│ BASE                 │ LOCAL                │ REMOTE               │
├──────────────────────┼──────────────────────┼──────────────────────┤
│ save(user);          │ saveUser(user);      │ repository.save();   │
└──────────────────────┴──────────────────────┴──────────────────────┘

                           ↓ merge

┌─────────────────────────────────────────────────────────────────────┐
│ RESULT                                                              │
│ repository.save(user);                                              │
└─────────────────────────────────────────────────────────────────────┘

[Take Local] [Take Remote] [Take Both] [Previous] [Next] [Resolved]
```

## Resolver por bloque

No limitarse a aceptar el archivo completo.

Cada bloque conflictivo debe permitir:

```text
Take Local
Take Remote
Take Both: Local then Remote
Take Both: Remote then Local
Edit Result
Previous Conflict
Next Conflict
```

Mostrar progreso:

```text
Conflict 3 / 8
```

## Tipos de conflictos

Soportar gradualmente:

```text
modify/modify
add/add
delete/modify
rename/rename
rename/delete
file/directory
```

No asumir que todos los conflictos son texto simple.

## Imágenes

Fase avanzada:

- comparación side-by-side;
- slider;
- overlay;
- selección de versión;
- diferencias básicas cuando sea posible.

---

# 16. Log gráfico

Crear un historial de commits muy completo.

Debe incluir:

```text
Graph
Hash
Message
Author
Date
References
```

Ejemplo:

```text
Graph       Hash      Message                 Author      Date
────────────────────────────────────────────────────────────
●           a82d92a   Fix login               Lucas       Today
│\
│ ●         b01a331   Feature invoice         Pedro       Yesterday
│ ●         772acc2   Add report              Pedro       Yesterday
● │         c2b22e3   Update database         Lucas       2 days ago
│/
●           281abc3   Initial version         Lucas       Sep 1
```

Mostrar badges para:

```text
HEAD
main
origin/main
tags
```

Al seleccionar un commit mostrar:

- hash completo;
- autor;
- email;
- fecha;
- committer;
- mensaje;
- padres;
- firmas;
- lista de archivos;
- estadísticas added/deleted.

Acciones sobre commit:

```text
Compare with parent
Compare with working tree
Compare with selected commit
Create branch here
Create tag here
Cherry-pick
Revert commit
Reset branch here
Browse repository
Blame file
Copy hash
Copy message
```

Agregar búsqueda y filtros por:

```text
text
author
email
path
date
branch
tag
hash
```

---

# 17. Revision Graph

Crear una vista gráfica independiente del log.

Características:

```text
zoom
pan
fit to screen
minimap
search commit
branch labels
tag labels
remote labels
merge edges
context menu
```

Investigar `QGraphicsScene` / `QGraphicsView` o una solución Qt equivalente.

No depender de imágenes prerenderizadas del comando Git si eso impide interacción.

---

# 18. Branch Manager

Vista de árbol:

```text
Local
├── main
├── develop
└── feature/login

Remote
└── origin
    ├── main
    └── develop
```

Acciones:

```text
Switch/Checkout
Create
Rename
Delete
Merge into current
Rebase current onto this
Compare
Show Log
Set upstream
Unset upstream
Push branch
Delete remote branch
```

Confirmar operaciones destructivas.

---

# 19. Tags

Interfaz gráfica para:

```text
Create lightweight tag
Create annotated tag
Sign tag
Delete local tag
Delete remote tag
Push tag
Push all tags
Show tag message
Browse commit
```

---

# 20. Repository Browser

Permitir navegar el árbol de archivos de cualquier commit sin modificar el working tree.

Funciones:

```text
Select revision
Browse directories
Open file
View file
Save file as
Compare file
Show history
Blame
Copy path
```

---

# 21. Browse References

Vista tipo árbol:

```text
refs
├── heads
├── remotes
└── tags
```

Mostrar:

```text
ref name
hash
subject
author
date
```

Acciones contextuales apropiadas.

---

# 22. Blame

Crear blame visual con columnas:

```text
Commit
Author
Date
Line
Code
```

Al seleccionar una línea mostrar información del commit.

Doble clic debe poder abrir el commit en Log.

Permitir:

```text
Show previous revision
Show commit
Copy hash
Copy line
Compare revision
```

---

# 23. Rebase gráfico

Implementar rebase normal y, posteriormente, rebase interactivo.

## Interactive rebase

Tabla:

```text
Action     Commit      Message
──────────────────────────────────────
pick       a31bc2      Add login
pick       12ffab      Fix typo
pick       b381fa      Fix login
```

Acciones:

```text
pick
reword
edit
squash
fixup
drop
```

Permitir reordenar con drag & drop.

Durante rebase:

```text
Rebase in progress
Commit 3 of 7

Conflict detected

[Resolve] [Continue] [Skip] [Abort]
```

No esconder el estado interno de Git.

---

# 24. Cherry-pick

Desde Log permitir seleccionar uno o varios commits.

Mostrar ventana previa con:

```text
commit order
hash
message
author
```

Opciones:

```text
commit automatically
no commit
mainline parent when required
```

Manejar conflictos mediante el Conflict Resolver común.

---

# 25. Stash Manager

Lista gráfica:

```text
stash@{0}
message
date
branch
```

Acciones:

```text
Create stash
Include untracked
Keep index
View changes
Apply
Pop
Create branch
Drop
Clear
```

Confirmar `drop` y `clear`.

---

# 26. Bisect

Wizard gráfico:

```text
Select GOOD revision
Select BAD revision
Start
```

Durante el proceso:

```text
Current commit: abc123

[Good] [Bad] [Skip]
```

Al finalizar:

```text
First bad commit
hash
message
author
date
```

Permitir abrirlo en Log.

---

# 27. Reflog

Vista:

```text
HEAD@{0}
a82bc1
checkout: develop -> main

HEAD@{1}
f91ac2
commit: Fix login
```

Acciones:

```text
Show Log
Create branch here
Reset here
Copy hash
Compare
```

Marcar claramente operaciones peligrosas.

---

# 28. Submodules

Vista gráfica de submódulos.

Mostrar:

```text
path
URL
current commit
expected commit
branch
status
```

Acciones:

```text
Add
Initialize
Update
Sync
Open in Dolphin
Open repository
View Diff
Deinitialize
Remove (advanced)
```

---

# 29. Worktrees

Manager gráfico:

```text
Path
Branch
HEAD
Locked
Prunable
```

Acciones:

```text
Create worktree
Open in Dolphin
Open GitShell
Lock
Unlock
Prune
Remove
```

---

# 30. Patches

Fase avanzada.

Soportar gráficamente:

```text
Create patch
Create patch series
Apply patch
Apply mailbox/series
Preview patch
```

Mostrar conflictos y errores de forma entendible.

---

# 31. Clone y Create Repository

## Clone

Wizard gráfico:

```text
Repository URL
Destination directory
Branch
Depth
Recursive submodules
Bare
Mirror (advanced)
```

Mostrar progreso real.

## Create repository

Permitir:

```text
Initialize repository
Initial branch name
Bare repository
Add .gitignore template later
```

---

# 32. Pull / Push / Fetch

Todas deben tener diálogo gráfico.

## Push

Mostrar:

```text
Local branch
Remote
Remote branch
Commits ahead
Tags
Force
Force-with-lease
Set upstream
```

Preferir `force-with-lease` sobre `force` y explicar visualmente el riesgo.

## Pull

Opciones:

```text
merge
rebase
ff-only
remote
branch
```

## Fetch

Opciones:

```text
remote
all remotes
prune
fetch tags
```

---

# 33. Sync Window

Crear una ventana de sincronización cómoda.

Ejemplo:

```text
LOCAL main                    REMOTE origin/main

Ahead: 3
Behind: 2

Incoming commits
────────────────────────────
● Fix customer calculation
● Update README

Outgoing commits
────────────────────────────
● New report
● Fix invoice
● Add validation

[Fetch] [Pull] [Push]
```

Permitir abrir cada commit en Log.

---

# 34. Progress Dialog común

Crear un componente reutilizable para operaciones Git largas.

Ejemplo:

```text
Fetching origin

██████████████████░░░░ 78%

Objects: 3812 / 4920
Transferred: 18.3 MB

[Cancel]

▸ Show Git output
```

No inventar porcentajes cuando Git no provea progreso real.

Cuando no sea posible calcular porcentaje, usar progreso indeterminado.

---

# 35. Errores amigables

No mostrar solamente:

```text
Process exited with code 1
```

Traducir errores frecuentes a mensajes útiles sin ocultar la salida original.

Ejemplo:

```text
Push rejected

origin/main contiene commits que no están en tu rama local.

Opciones recomendadas:
[Fetch]
[Pull + Merge]
[Pull + Rebase]

[Show Git output]
```

Siempre permitir ver la salida Git completa.

---

# 36. Settings

Crear un centro de configuración completo.

Estructura objetivo:

```text
Settings
│
├── General
├── Git
├── Context Menu
├── Icon Overlays
├── Status Cache
├── Network
├── SSH
├── Diff
├── Merge
├── Log
├── Commit
├── Hooks
├── Repository
├── Appearance
├── Integrations
├── Diagnostics
└── Advanced
```

## General

```text
Language
Theme: System / Light / Dark
Start daemon with session
Check for updates
Open links behavior
```

## Git

Editar gráficamente:

```text
user.name
user.email
core.editor
init.defaultBranch
commit.gpgsign
user.signingkey
pull.rebase
```

Permitir elegir scope:

```text
System
Global
Repository
```

Mostrar de dónde proviene el valor efectivo.

No sobrescribir configuraciones que el usuario no modificó.

## Diff tool

Opciones:

```text
Built-in
KDiff3
Meld
VS Code
Custom command
```

## Merge tool

Opciones equivalentes.

## Context Menu

Permitir:

```text
Enable/disable action
Main menu / Git submenu
Order
Profiles
Reset defaults
```

## Icon Overlays

Configurar:

```text
Show clean
Show modified
Show added
Show deleted
Show conflict
Show ignored
Show untracked
Propagate folder status
```

## Status Cache

Mostrar:

```text
Daemon: Running
PID
Repositories monitored
Cached entries
Memory estimate
Last refresh
```

Acciones:

```text
Restart daemon
Refresh all
Clear cache
```

## Excluded paths

Permitir excluir rutas como:

```text
/proc
/sys
/dev
/run
/tmp
```

No confundir exclusiones del daemon con `.gitignore`.

## Diagnostics

Mostrar:

```text
LinuxGitShell version
Qt version
KDE Frameworks version
Plasma version
Dolphin version
Git version
Kernel version
Daemon state
Plugins loaded
```

Botones:

```text
Copy diagnostic information
Open logs
Enable debug logging
```

---

# 37. Configuración Linux / XDG

Respetar XDG.

Preferir mecanismos KDE apropiados, especialmente KConfig.

Rutas esperadas conceptualmente:

```text
~/.config/
~/.cache/
~/.local/share/
~/.local/state/
```

No escribir archivos arbitrarios directamente en `$HOME`.

No guardar secretos en archivos de configuración propios.

---

# 38. Credenciales y seguridad

No implementar un almacén propio de contraseñas o tokens.

Integrarse con mecanismos existentes:

```text
OpenSSH
ssh-agent
KWallet cuando corresponda
Git credential helpers
Git Credential Manager si está disponible
GPG agent
SSH signing
```

Nunca registrar en logs:

```text
passwords
tokens
private keys
credential helper secrets
```

Sanitizar URLs que incluyan credenciales.

---

# 39. SSH

Settings debe permitir detectar:

```text
/usr/bin/ssh
ssh-agent
known_hosts
public keys
```

Ofrecer prueba de conexión cuando sea técnicamente apropiado.

No modificar `~/.ssh/config` automáticamente sin consentimiento explícito.

---

# 40. Commit signing

Soportar gradualmente:

```text
GPG signing
SSH signing
```

Mostrar:

```text
signing enabled
selected key
verification status
```

En Log, mostrar commits firmados y estado de verificación si Git provee la información.

---

# 41. Hooks

No ejecutar hooks propios ocultos.

Git debe continuar respetando hooks normales del repositorio.

En Settings se puede mostrar información de hooks existentes.

Fase avanzada: facilitar edición/visualización, pero no crear un ecosistema inseguro de scripts automáticos.

---

# 42. Temas e interfaz

Debe integrarse visualmente con KDE.

Requisitos:

```text
System theme
Light theme
Dark theme
HiDPI
Keyboard navigation
Accessible labels
Native dialogs where appropriate
```

Evitar colores hardcodeados si el tema del sistema puede proveerlos.

Los estados Git deben seguir siendo distinguibles con temas claros y oscuros.

No depender únicamente del color: usar iconos/texto cuando sea importante.

---

# 43. Internacionalización

Preparar desde temprano para traducciones con `KI18n` o el mecanismo KDE correspondiente.

Idioma inicial:

```text
English
Spanish
```

No hardcodear todo el texto de interfaz de forma que luego sea difícil traducirlo.

---

# 44. Accesibilidad

Considerar:

```text
keyboard-only navigation
screen reader labels
focus order
high contrast
non-color-only status
configurable fonts through KDE
```

---

# 45. Rendimiento

Reglas obligatorias:

1. nunca bloquear el hilo principal de Dolphin con `git status`;
2. no hacer escaneos recursivos ingenuos en cada actualización;
3. cachear estado;
4. invalidar cache correctamente;
5. usar debounce;
6. medir tiempos;
7. agregar logs de performance solo en modo debug;
8. probar con repositorios grandes;
9. evitar cargar el log completo de miles de commits si basta con paginación/lazy loading;
10. evitar cargar diffs gigantes completos si se puede paginar o truncar con advertencia.

---

# 46. Operaciones destructivas

Requieren confirmación clara:

```text
reset --hard
clean -fd
branch delete -D
remote branch delete
force push
stash clear
reflog-related recovery actions
submodule removal
worktree removal with changes
```

Mostrar exactamente qué va a ocurrir.

Preferir variantes seguras.

Ejemplo:

```text
Force push
```

ofrecer primero:

```text
Force with lease
```

---

# 47. Integridad del working tree

Antes de operaciones complejas como:

```text
checkout
switch
merge
rebase
reset
pull
```

comprobar cambios locales cuando Git pueda sobrescribirlos.

No intentar ser más permisivo que Git.

Mostrar la explicación del problema y opciones seguras.

---

# 48. Testing

Crear tests desde las primeras fases.

## Unit tests

Especialmente para:

```text
porcelain parser
status model
diff parser
log parser
config parsing
path handling
repo root detection
```

## Integration tests

Crear repositorios Git temporales y probar:

```text
clean repo
modified file
staged file
untracked file
ignored file
rename
merge conflict
branch creation
stash
rebase conflict
worktree
submodule where feasible
```

No depender de repositorios personales del usuario para los tests automáticos.

---

# 49. Logging

Crear logging por categorías.

Ejemplo conceptual:

```text
linuxgitshell.core
linuxgitshell.git
linuxgitshell.daemon
linuxgitshell.dolphin
linuxgitshell.gui
linuxgitshell.diff
linuxgitshell.merge
```

Niveles:

```text
normal
debug
trace
```

No loguear secretos.

---

# 50. Manejo de procesos Git

Crear un runner común que maneje:

```text
program
arguments
working directory
environment
stdout
stderr
exit code
cancellation
timeout when appropriate
progress
```

Evitar construir comandos mediante concatenación de strings y `shell -c` si se pueden pasar argumentos separados a `QProcess`.

Esto reduce problemas con espacios, comillas y seguridad.

---

# 51. Paths Linux

Tener en cuenta:

```text
case-sensitive paths
spaces
UTF-8 filenames
symlinks
mount points
external drives
network mounts
very long paths
hidden files
.git file instead of .git directory (worktrees/submodules)
```

Nunca asumir que `.git` siempre es un directorio.

---

# 52. `.git` especiales

Considerar correctamente:

```text
normal repository
bare repository
submodule
worktree
linked worktree
.git file pointing elsewhere
```

Usar Git cuando sea posible para descubrir rutas internas:

```bash
git rev-parse --show-toplevel
git rev-parse --git-dir
git rev-parse --show-superproject-working-tree
```

---

# 53. Iconos

Crear iconos propios y libres.

No copiar assets de TortoiseGit salvo que se revise explícitamente la licencia y compatibilidad.

Estados visuales objetivo:

```text
Clean       check
Modified    warning/red marker
Added       plus
Deleted     minus/delete
Conflict    conflict/exclamation
Untracked   question mark
Ignored     muted/disabled
```

Adaptarlos a tamaños pequeños de overlays.

---

# 54. Licencia

Implementación limpia.

No copiar código fuente de TortoiseGit sin una revisión de licencia específica.

Se puede estudiar su experiencia de usuario y arquitectura pública, pero escribir implementación propia.

Licencia provisional recomendada para el nuevo código:

```text
GPL-3.0-or-later
```

Antes de publicar una versión estable, revisar la estrategia de licencia de todas las dependencias y cualquier código reutilizado.

---

# 55. Packaging

Primero Manjaro/Arch.

Crear posteriormente:

```text
PKGBUILD
```

Luego considerar:

```text
Debian/Ubuntu package
Fedora RPM
openSUSE package
```

No priorizar Flatpak hasta comprobar cómo afecta la integración binaria con Dolphin y el acceso a repositorios del host.

La arquitectura D-Bus debe ayudar a desacoplar componentes en el futuro.

---

# 56. Futuras integraciones con otros file managers

Después de estabilizar Dolphin:

```text
LinuxGitShell Core
        │
        ├── Dolphin
        ├── Nautilus
        ├── Nemo
        └── Thunar
```

Por eso:

- no acoplar GitService a Dolphin;
- no acoplar cache a KDE UI;
- no acoplar settings de Git a un plugin específico;
- definir modelos comunes reutilizables.

---

# 57. Dashboard

Fase posterior al MVP.

Crear una pantalla principal de repositorio:

```text
imovapy-web

Branch
main

Status
3 modified
1 untracked

Remote
origin/main
Ahead 4
Behind 2

Last commit
Fix invoice calculation
2 hours ago

[Commit] [Fetch] [Pull] [Push] [Log] [Graph]
```

Debe servir como entrada amigable a usuarios que prefieren abrir la aplicación en lugar del menú contextual.

---

# 58. UX avanzada

Considerar posteriormente:

```text
command palette
keyboard shortcuts
recent repositories
favorites
multi-repository dashboard
open repository in Dolphin
open terminal here
open IDE here
copy branch name
copy commit hash
copy repository path
```

No convertir la app en IDE. Mantener el foco en Git.

---

# 59. Conventional Commits opcional

No imponer Conventional Commits.

Puede existir un helper opcional:

```text
type
scope
description
body
breaking change
issue reference
```

Debe poder desactivarse completamente.

---

# 60. GitHub / GitLab

No es prioridad del núcleo.

Primero Git puro.

Más adelante se puede diseñar una capa opcional para:

```text
Open repository web page
Open commit online
Open issue/PR associated with branch
Create pull/merge request
CI status
```

No mezclar esto con el backend Git básico.

---

# 61. Fases de implementación

## Fase 0 — Bootstrap (completada)

Objetivo:

- compilar una aplicación Qt/KF6 mínima;
- detectar versiones;
- configurar CMake;
- crear tests básicos;
- crear `STATUS.md`.

Criterio de aceptación:

```text
cmake configure succeeds
build succeeds
app starts
unit test runner works
```

---

## Fase 1 — Git Core (completada en `v0.1.0`)

Implementar:

```text
Git process runner
repository root detection
status porcelain parser
repository status model
basic config reader
```

Criterios:

- detectar repo correctamente;
- listar modified/staged/untracked/conflict;
- tests con repos temporales;
- cero UI compleja todavía.

---

## Fase 2 — Dolphin Context Menu MVP (en curso)

Primer incremento completado: plugin KF6 pequeño, una acción traducible para una selección local,
lanzamiento externo sin shell, instalación de desarrollo y 11 tests totales. Pendiente: resolver el
contexto por snapshots/cache sin bloquear Dolphin y habilitar las acciones específicas siguientes.

Implementar:

```text
Git Clone...
Git Create repository here...
Git > Show Status
Git > Commit...
Git > Pull...
Git > Push...
Git > Show Log
Git > Settings
```

Al principio las acciones pueden abrir ventanas simples, pero deben estar conectadas al Git Core.

Criterio:

- plugin carga en Dolphin;
- menú aparece solo cuando corresponde;
- no bloquea Dolphin.

---

## Fase 3 — Overlay MVP

Implementar:

```text
Clean
Modified
Added
Conflict
Untracked
```

Primero cache simple.

Criterio:

- overlays correctos;
- actualización después de editar archivo;
- no ejecutar `git status` sin control por cada paint/request.

---

## Fase 4 — Daemon + D-Bus

Mover estado a daemon.

Implementar:

```text
repo registry
cache
watcher
debounce
D-Bus status API
```

Criterio:

- Dolphin consulta cache por IPC;
- editar archivo actualiza overlay;
- daemon reiniciable sin cerrar Dolphin.

---

## Fase 5 — Working Tree + Commit

Implementar GUI usable.

Criterio:

- seleccionar archivos;
- stage/unstage;
- diff básico;
- commit;
- amend;
- errores gráficos.

---

## Fase 6 — Diff Viewer

Implementar:

```text
side-by-side
unified
syntax highlighting
line numbers
navigation
```

Criterio:

- comparar working tree vs HEAD;
- commit vs parent;
- dos commits seleccionados.

---

## Fase 7 — Log

Implementar:

```text
commit list
graph lanes
refs
details
changed files
filters
context actions
```

Criterio:

- repos con merges se muestran correctamente;
- lazy loading/pagination.

---

## Fase 8 — Conflict Resolver

Implementar merge de tres vías.

Criterio:

- detectar archivos conflictuados;
- obtener stages BASE/OURS/THEIRS;
- navegar bloques;
- editar resultado;
- marcar resuelto;
- cancelar sin pérdida de datos.

---

## Fase 9 — Branches / Merge / Tags

Implementar administración gráfica estable.

---

## Fase 10 — Sync / Fetch / Pull / Push

Ventanas gráficas completas con progreso y errores.

---

## Fase 11 — Rebase / Cherry-pick / Stash

Reutilizar Conflict Resolver.

---

## Fase 12 — Graph / Reflog / Blame / Repo Browser

Herramientas avanzadas de historial.

---

## Fase 13 — Worktrees / Submodules / Bisect / Patches

Completar flujos avanzados.

---

## Fase 14 — Settings completo

Unificar configuración gráfica.

---

## Fase 15 — Release comunitario

Preparar:

```text
README
screenshots
CONTRIBUTING
CODE_OF_CONDUCT optional
issue templates
release build
PKGBUILD
translations
icons
licenses
```

---

# 62. Criterios de calidad para cada fase

No considerar una fase terminada si:

- no compila;
- rompe funciones anteriores;
- no tiene manejo razonable de errores;
- bloquea Dolphin;
- requiere comandos manuales no documentados;
- deja rutas hardcodeadas del usuario;
- usa credenciales incrustadas;
- no tiene al menos pruebas de la lógica crítica.

---

# 63. STATUS.md

Mantener un archivo `STATUS.md` actualizado después de cada sesión importante.

Formato sugerido:

```markdown
# Project Status

## Current phase
Phase 3 — Overlay MVP

## Working
- Repository detection
- Porcelain v2 parser
- Context menu plugin
- Modified overlay

## In progress
- Conflict overlay
- Daemon prototype

## Known issues
- Large repository refresh is slow
- Folder propagation incomplete

## Next steps
1. Implement debounce
2. Add D-Bus cache query
3. Add integration test
```

Esto es obligatorio para poder continuar el trabajo con Codex en sesiones posteriores.

---

# 64. README inicial

El README debe explicar brevemente:

```text
What LinuxGitShell is
Current development status
Supported desktop/file manager
Screenshots later
Build dependencies
Build instructions
Install instructions
Uninstall instructions
Known limitations
Contributing
License
```

No prometer soporte que todavía no existe.

---

# 65. Build

Objetivo de flujo estándar:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Para desarrollo local, permitir instalación a prefijo de usuario cuando sea viable.

No requerir `sudo` para cada ciclo de desarrollo si puede evitarse.

Si el plugin de Dolphin necesita una ruta de instalación específica, documentar claramente el flujo de desarrollo y el de instalación del sistema.

---

# 66. Desinstalación limpia

Toda instalación debe poder eliminarse sin dejar basura.

Documentar archivos instalados.

No modificar archivos de KDE del usuario de forma irreversible.

---

# 67. Compatibilidad de versiones

Detectar en CMake versiones mínimas razonables de:

```text
Qt6
KF6
KIO
Git
```

No fijar versiones innecesariamente estrictas.

Registrar en diagnósticos las versiones realmente cargadas.

---

# 68. Diseño de código

Preferencias:

- RAII;
- smart pointers cuando correspondan;
- no usar singletons globales innecesarios;
- separar modelo, servicio y UI;
- const correctness;
- signals/slots claros;
- errores tipados;
- evitar exceptions cruzando fronteras Qt si complica el diseño;
- no mezclar parsing con widgets;
- no bloquear GUI esperando procesos largos;
- usar operaciones asíncronas para Git pesado.

---

# 69. Modelo de operación asíncrona

Diseñar operaciones Git largas alrededor de un concepto similar a:

```cpp
GitOperation
```

con señales:

```text
started
progressChanged
stdoutReceived
stderrReceived
finished
failed
cancelled
```

Esto permitirá reutilizar el Progress Dialog.

---

# 70. Cancelación

Permitir cancelar cuando sea seguro:

```text
clone
fetch
pull before mutation stage if applicable
push
long log/diff operations
```

No matar procesos de manera que deje un repositorio inconsistente sin advertencia.

Para operaciones como rebase/merge, diferenciar:

```text
Cancel UI operation
Abort Git operation
```

---

# 71. Conflictos — obtención de BASE/OURS/THEIRS

No depender solamente de marcadores `<<<<<<<` del archivo.

Usar correctamente el index de Git y stages de conflicto cuando corresponda.

Investigar:

```bash
git ls-files -u
```

y blobs stage 1/2/3.

La UI debe representar:

```text
stage 1 = base
stage 2 = ours
stage 3 = theirs
```

según el contexto Git aplicable.

Agregar tests reales de conflictos.

---

# 72. Diff interno

No reinventar un algoritmo de diff complejo en la primera versión.

Inicialmente Git puede generar hunks.

La aplicación debe parsearlos a un modelo estructurado:

```text
DiffFile
DiffHunk
DiffLine
```

Luego el UI side-by-side puede construir alineación visual.

Agregar tests para:

```text
added file
deleted file
renamed file
binary file
no newline at EOF
unicode
large hunks
```

---

# 73. Log y grafos

No depender exclusivamente de `git log --graph` ASCII para la vista gráfica.

Obtener padres y construir un modelo de commits.

La UI debe dibujar lanes/conexiones.

El ASCII puede servir para depuración, no como base visual permanente.

---

# 74. Operaciones de red

No asumir GitHub.

Debe funcionar con cualquier remoto Git compatible:

```text
GitHub
GitLab
Bitbucket
Gitea
Forgejo
servidor SSH propio
servidor HTTPS propio
local filesystem remote
```

GitHub/GitLab son integraciones opcionales futuras.

---

# 75. Menú contextual — seguridad

Antes de mostrar ciertas acciones, resolver correctamente el repo seleccionado.

No permitir que una selección de archivos pertenecientes a repositorios distintos ejecute una operación conjunta peligrosa.

Si la selección atraviesa repositorios, limitar el menú o agrupar por repo solamente cuando esté diseñado explícitamente.

---

# 76. Carpetas ignoradas por performance

Crear una lista de exclusión predeterminada del daemon para pseudo-filesystems Linux cuando se navegue fuera de repositorios:

```text
/proc
/sys
/dev
/run
```

No excluir automáticamente carpetas normales dentro de un repositorio solo por llamarse `build` o `target`, salvo que la política esté claramente diseñada.

`.gitignore` y performance exclusions son conceptos diferentes.

---

# 77. Git LFS

Fase avanzada.

Detectar si Git LFS está disponible.

No asumirlo.

Mostrar información amigable si un repositorio usa LFS y el ejecutable falta.

---

# 78. Sparse checkout

Fase avanzada.

Detectar y no romper repositorios con sparse checkout.

Más adelante puede agregarse administración gráfica.

---

# 79. Bare repos

No mostrar operaciones de working tree en un bare repository.

Adaptar el menú dinámicamente.

---

# 80. Estado de operaciones Git en progreso

Detectar estados especiales del repositorio:

```text
MERGE_HEAD
rebase-merge
rebase-apply
CHERRY_PICK_HEAD
REVERT_HEAD
BISECT_START
```

Preferir mecanismos Git/porcelain cuando existan.

Mostrar una barra/banner en las ventanas:

```text
REBASE IN PROGRESS
MERGE IN PROGRESS
CHERRY-PICK IN PROGRESS
```

con acciones válidas:

```text
Continue
Abort
Skip
Resolve
```

---

# 81. Ventana Settings desde Dolphin

Cuando el usuario ejecute:

```text
Dolphin
→ Git
→ Settings
```

abrir la aplicación de configuración externa pasando el repositorio como contexto, por ejemplo conceptualmente:

```bash
linuxgitshell-settings --repository /path/to/repo
```

El plugin no debe contener toda la ventana de configuración.

---

# 82. Integración con terminal e IDE

Opcional pero útil:

```text
Open Terminal Here
Open Repository in IDE
```

Detectar aplicaciones disponibles.

No hardcodear VS Code como única opción.

---

# 83. File history

Clic derecho sobre archivo:

```text
Git > File history
```

Mostrar solo commits relevantes para ese path.

Permitir:

```text
Follow renames
Compare versions
Open commit
Blame
Save old revision
```

---

# 84. Rename detection

Mostrar renames como rename cuando Git los detecte.

Ejemplo:

```text
R  src/User.java -> src/UserEntity.java
```

No convertir visualmente todo rename en delete+add si Git informa el rename.

---

# 85. Clean

Crear una ventana segura para `git clean`.

Antes de borrar, mostrar preview mediante dry-run.

Debe existir siempre una etapa equivalente a:

```bash
git clean -n
```

antes de permitir una eliminación real desde GUI.

Mostrar exactamente qué archivos/directorios se eliminarán.

---

# 86. Revert / Restore

Diferenciar claramente:

```text
Restore file changes
Revert commit
Reset branch
```

No llamar a todo “Revert”.

La UI debe explicar qué afecta al working tree, index y history.

---

# 87. Reset

Crear diálogo avanzado posteriormente:

```text
Soft
Mixed
Hard
```

Explicar consecuencias.

`Hard` requiere confirmación fuerte.

---

# 88. Export

Permitir exportar snapshot de un commit/branch/tag a una carpeta o archivo cuando Git lo permita.

No incluir `.git` salvo modo explícito.

---

# 89. Repo discovery

Al navegar Dolphin, encontrar repositorio desde cualquier subcarpeta de forma eficiente.

Cachear relación:

```text
path -> repository root
```

Invalidarla correctamente si se mueve/elimina el repositorio.

---

# 90. Multiple repositories

El daemon debe soportar muchos repositorios abiertos/navegados durante una sesión.

No asumir uno solo.

Diseñar límites razonables de cache y políticas de expiración.

---

# 91. Diagnostics para issues de GitHub

Crear un botón que copie algo parecido a:

```text
LinuxGitShell: 0.x
OS: Manjaro Linux
Kernel: ...
Qt: ...
KF6: ...
Plasma: ...
Dolphin: ...
Git: ...
Daemon: running
Context plugin: loaded
Overlay plugin: loaded
Repository type: normal/worktree/submodule/bare
```

Nunca incluir:

```text
username de URL privada
tokens
passwords
private keys
contenido sensible de archivos
```

---

# 92. Contributing

Preparar el proyecto para contribuciones.

Crear posteriormente:

```text
CONTRIBUTING.md
```

con:

```text
how to build
coding style
how to run tests
how to test Dolphin plugin
how to report bugs
how to add translations
how to submit PRs
```

---

# 93. GitHub CI

El repositorio público ya ejecuta CI obligatorio para:

```text
configure
build
unit tests
plugin loading tests
clang-tidy
formatting
secret patterns
dependency review
```

La GUI completa de Dolphin permanece en el checklist manual; CI prueba el módulo real mediante
`KPluginFactory` y el límite seguro de procesos sin depender de una sesión gráfica interactiva.

Separar tests core de integration GUI.

---

# 94. Definition of Done para V1.0

La primera versión 1.0 enfocada en Dolphin debería tener como mínimo:

```text
✓ Dolphin context menu
✓ Git status overlays
✓ daemon/cache
✓ Working Tree
✓ Commit GUI
✓ stage/unstage
✓ diff side-by-side + unified
✓ Log
✓ file history
✓ branch create/switch/delete
✓ tags basic
✓ fetch
✓ pull
✓ push
✓ sync
✓ stash basic
✓ merge
✓ conflict resolver 3-way
✓ rebase basic
✓ cherry-pick basic
✓ reflog
✓ blame
✓ revision graph
✓ repository browser
✓ settings
✓ diagnostics
✓ Arch/Manjaro package
✓ English + Spanish
```

Las siguientes pueden entrar en 1.x si no alcanzan la calidad esperada para 1.0:

```text
advanced interactive rebase
image diff
patch series
full submodule manager
full worktree manager
bisect GUI
Git LFS manager
Nautilus/Nemo/Thunar
```

---

# 95. Primera tarea histórica para Codex (completada)

Esta sección conserva el encargo con el que comenzó el repositorio. Sus resultados fueron publicados
en `v0.1.0`; no representa la próxima tarea actual. Consultar `STATUS.md` y la sección final de
`roadmap-checklist.md` para continuar.

Al comenzar desde un repositorio vacío o casi vacío, realizar **solo esta primera tarea**:

## Objetivo

Preparar el bootstrap técnico de LinuxGitShell para Manjaro KDE Plasma 6.

## Pasos

1. Inspeccionar sistema y repositorio.
2. Detectar:
   - distribución;
   - versión de KDE Plasma;
   - versión de Dolphin;
   - Qt6;
   - KDE Frameworks;
   - Git;
   - CMake;
   - compilador C++.
3. No instalar nada hasta saber qué falta.
4. Si falta una dependencia, mostrar claramente cuál y por qué.
5. Crear CMake raíz.
6. Crear una aplicación Qt/KF6 mínima llamada `linuxgitshell`.
7. Crear librería inicial `gitcore`.
8. Crear `GitProcessRunner` seguro basado en `QProcess`.
9. Crear detección de repositorio.
10. Crear parser inicial de `git status --porcelain=v2 -z`.
11. Crear tests automáticos para estados:
    - clean;
    - modified;
    - staged;
    - untracked.
12. Crear `README.md` mínimo.
13. Crear `STATUS.md`.
14. Compilar.
15. Ejecutar tests.
16. No avanzar todavía a overlays ni plugin de Dolphin hasta que esta base esté estable. Esta
    condición ya se cumplió antes de iniciar la Fase 2.

## Resultado esperado

Al finalizar, quiero poder ejecutar algo como:

```bash
./build/linuxgitshell /ruta/a/repositorio
```

y obtener una ventana mínima que muestre:

```text
Repository: /ruta/a/repositorio
Branch: main
Status:
- 2 modified
- 1 staged
- 1 untracked
```

No tiene que ser bonita todavía.

La prioridad de esta primera tarea es una arquitectura correcta, tests y una base compilable.

---

# 96. Forma de responder de Codex al finalizar cada tarea

Usar este formato:

```markdown
## Resultado

Qué se implementó.

## Archivos principales modificados

- path/file
- path/file

## Verificación

- configure: OK/FAIL
- build: OK/FAIL
- tests: X passed / Y failed

## Cómo probarlo

Comandos exactos.

## Problemas conocidos

Lista concreta.

## Siguiente paso recomendado

Una sola fase o tarea siguiente, no diez cosas a la vez.
```

No responder solo “hecho”.

---

# 97. Principio final del proyecto

El usuario final debería poder hacer la mayor parte de su trabajo diario con Git así:

```text
Dolphin
   ↓
iconos de estado
   ↓
clic derecho
   ↓
Git
   ↓
ventana gráfica
```

Y poder realizar gráficamente:

```text
Clone
Init
Status
Add
Stage
Unstage
Commit
Diff
Log
Pull
Fetch
Push
Sync
Branch
Checkout/Switch
Merge
Conflict Resolution
Rebase
Cherry-pick
Stash
Tag
Blame
Reflog
Revision Graph
Repository Browser
Submodules
Worktrees
Bisect
Patches
Settings
Diagnostics
```

El objetivo no es ocultar Git ni reemplazarlo.

El objetivo es ofrecer **una de las mejores interfaces gráficas de Git integradas al sistema de archivos de Linux**, empezando por KDE Dolphin.
