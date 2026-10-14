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
