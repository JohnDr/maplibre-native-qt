# AGENTS.md — maplibre-native-qt (QMapLibre fork)

## Build & run policy (agreed with John)

**Split of responsibility:**

- **MapLibreEarth (the app) is built and run through Qt Creator, always.** Use the
  Qt Creator MCP tools (`qt-creator_switch_build_config`, `qt-creator_build`,
  `qt-creator_run_project` / `qt-creator_debug`). Never do ad-hoc command-line app
  builds or invent alternate app build dirs.
- **This library (QMapLibre) and other dependencies are built from the command
  line** into the canonical CMake trees below. Do **not** try to drive the library
  through the Qt Creator MCP (only the app is on the MCP path).

## If you cannot follow an instruction — ask John

If for any reason you cannot follow an instruction in this file (or in
`MapLibreEarth/AGENTS.md`), **stop and come back to John for a decision**. Do not
silently deviate, and do not substitute a workaround on your own initiative. State
plainly what you could not do and why, and wait.

This applies in particular to the build & run policy above — e.g. if a Qt Creator
MCP tool blocks or fails (such as `qt-creator_run_project` never returning for a
long-running GUI app), if a build tree / install prefix does not match, or if you
find yourself wanting to build or run the app outside Qt Creator (ad-hoc
command-line app builds, launching the app exe directly, hand-copying runtime
DLLs). In all such cases: ask first.

## The handoff guarantee

When you hand the app back, John may run **either** Debug **or** Release. Both must
already be on the latest code, because the app resolves its QMapLibre runtime and
`MapLibre` QML plugin from the matching install prefix (Debug app cannot load a
Release plugin). So, after **any** library change:

1. **Refresh BOTH installs** — build and install the Release *and* Debug trees.
   One shot: `%LOCALAPPDATA%\Temp\opencode\build-libs-both.bat` (prints
   `LIB_REFRESH_OK`). Or do the two trees manually (below).
2. **Rebuild the app** in the config you're handling via Qt Creator, so its
   POST_BUILD copies the matching runtime next to the exe.
3. A stale Debug install is the classic failure: the app aborts at startup with
   `Cannot assign to non-existent property "terrainExaggeration"` because it
   loaded an old `MapLibre` QML plugin.

| Config  | QMapLibre install prefix |
|---------|--------------------------|
| Release | `C:\hda\dev\tmp\install\maplibre-native-qt` |
| Debug   | `C:\hda\dev\tmp\install-maplibre-native-qt-debug` |

## Canonical library build dirs (generator: NMake Makefiles JOM)

- Release: `C:\hda\dev\tmp\build-maplibre-native-qt-Desktop_Qt_6_12_0_MSVC2022_64bit-Release`
- Debug:   `C:\hda\dev\tmp\build-maplibre-native-qt-Desktop_Qt_6_12_0_MSVC2022_64bit-Debug`

```powershell
# from a vcvars64 shell with jom on PATH (see %LOCALAPPDATA%\Temp\opencode\build-libs-both.bat)
cmake --build   <Release tree>
cmake --install <Release tree> --config Release --prefix C:\hda\dev\tmp\install\maplibre-native-qt
cmake --build   <Debug tree>
cmake --install <Debug tree>   --config Debug   --prefix C:\hda\dev\tmp\install-maplibre-native-qt-debug
```

## Sanity check before handoff

- The install plugins must contain the expected API:
  `declarative_maplibre.dll` / `declarative_maplibred.dll` under
  `<prefix>\qml\MapLibre\` should contain `terrainExaggeration`.
- No stale build trees: see the forbidden list below.

## Forbidden build paths

MinGW app builds, Ninja app builds, `build-pr1-opengl`, `build-QmlEarth-*`, example
builds, and any other one-off trees. The only sanctioned dirs are the two library
trees above, the app's Qt Creator dirs (in `MapLibreEarth/AGENTS.md`), and the
Android trees listed in the wiki.

## Where the app lives

- App repo: `C:\hda\dev\Projects\MapLibreEarth`
- App Qt Creator build dirs: `C:\hda\dev\tmp\build-MapLibreEarth-Desktop_Qt_6_12_0_MSVC2022_64bit-{Debug,Release}`

## When in doubt

Read the MapLibreEarth wiki runbooks:
`wiki/runbooks/qt-creator-debug-release.md`, `wiki/runbooks/build-windows.md`,
and `wiki/architecture/environment.md`.

## Vendored core unit tests (mbgl-test-runner) — Linux

Canonical test tree (configure once; command line only, never the app path):

```bash
export PATH=$HOME/Qt/Tools/Ninja:$PATH
cmake -G Ninja -S vendor/maplibre-native -B ~/dev/tmp/build-maplibre-native-core-tests \
  -DCMAKE_BUILD_TYPE=Release -DMLN_WITH_VULKAN=ON -DMLN_WITH_OPENGL=OFF -DMLN_WITH_GLFW=OFF \
  -DMLN_WITH_WERROR=OFF -DMLN_TEXT_SHAPING_HARFBUZZ=OFF
ninja -C ~/dev/tmp/build-maplibre-native-core-tests mbgl-test-runner
cd vendor/maplibre-native && ~/dev/tmp/build-maplibre-native-core-tests/mbgl-test-runner   # run from the source dir
```

Qt-level test: `ctest` in each library tree (`QT_QPA_PLATFORM=xcb QSG_RENDER_LOOP=basic`).
Known failures not caused by terrain work (2026-10-16): `StyleConversion.Projection`, `CustomDrawableLayer.Line/Fill`.

Render tests (per group, because one process over all groups exhausts thread-local keys and aborts):
`ninja -C ~/dev/tmp/build-maplibre-native-core-tests mbgl-render-test-runner`, then for each dir in
`metrics/integration/render-tests` run `mbgl-render-test-runner -p metrics/linux-vulkan.json -f render-tests/<group>/`
from `vendor/maplibre-native`. Result 2026-10-16: 1200 pass; `fill-extrusion-pattern/tile-buffer` fails and
`text-font/{burmese,khmer,devanagari}` error — identical without patch 0040's cover/frustum code.
Benchmarks: `mbgl-benchmark-runner --benchmark_filter=TileCover`.

## Remotes (2026-10-17)

`origin` = `johndr/maplibre-native-qt`. The vendored core is `vendor/maplibre-native` (pinned at upstream `c7506d6`, working tree = patches applied, never commit it). The patched core is also published as branch `maplibreearth-patches` of `johndr/maplibre-native` (one commit per `patches/*.patch`); keep it in sync when adding a patch (see the MapLibreEarth wiki runbook `linux-terrain-test-and-debug`).

## GL JS alignment rule (agreed with John, 2026-10-17)

MapLibre GL JS is the reference implementation. Where this fork does the same job as GL JS
(terrain, tile covers, camera, projection, drape, shaders), follow GL JS's algorithm, units
and constants, and read its source (github.com/maplibre/maplibre-gl-js) before designing.

Any deviation must be justified and agreed with John BEFORE it is implemented:
- State what GL JS does (file/function), what you propose instead, and why GL JS's approach
  cannot be used here.
- Wait for a decision. Do not ship a stop-gap, heuristic or fork-only feature unprompted.
- Temporary stop-gaps agreed with John must be recorded in the alignment audit with the
  intended GL JS-aligned replacement.

Deviations already in the code are listed in `wiki/references/gljs-alignment.md` (MapLibreEarth
repo); new work must not add to that list without agreement.
