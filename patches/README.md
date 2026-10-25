# Submodule patches

The submodules are pinned to commits that live in community forks (see
`.gitmodules`). A few fixes on top of those commits aren't published in any
fork yet, so they're kept here and CMake applies them to the submodule working
tree when you configure the project (`dependencies/submodules/CMakeLists.txt`).
Patches already in the working tree are detected (the newest one that can be reversed marks how far an earlier configure got) and skipped, so re-running CMake is safe.

After configuring, `git status` reports the patched submodule as having
modified content. That's expected; `git -C <submodule> checkout .` undoes it
and the next configure re-applies the patches.

## delta-studio

Pinned commit: `8daecd0` on
[0xsuk/engine-sim-linux-can-delta-studio](https://github.com/0xsuk/engine-sim-linux-can-delta-studio)
(bobsayshilol's SDL port plus a larger SDL audio callback buffer).

| Patch | What it fixes |
| --- | --- |
| `0001` | MSVC build of `physics/src/collision_detector.cpp` (Windows `min`/`max` macros), by David ([Creesic/delta-studio](https://github.com/Creesic/delta-studio) `macos-port`) |
| `0002` | Direct3D 10/11 input layouts after the `std::string` conversion (`SemanticName`) |
| `0003` | `ysWindowsMonitor` calling the removed `InitializeDeviceName()` |
| `0004` | Release builds importing the debug-only `d3dx10d_43.dll`, by Danniel-Porto ([Danniel-Porto/delta-studio](https://github.com/Danniel-Porto/delta-studio) `release-directx-linking`) |
| `0005` | `ysSubclassRegistry` writing one pointer past the device table when the Vulkan device registers (global memory corruption on Windows at startup) |

Without them the port only builds on Linux/macOS; with them it builds on
Windows (Direct3D 11) as well.

## Moving a patch into a fork

If you host your own delta-studio fork, you can turn these into commits and
drop the patch directory:

```sh
cd dependencies/submodules/delta-studio
git checkout -b engine-sim-community 8daecd0
git am ../../../patches/delta-studio/*.patch
git push <your-fork> engine-sim-community
```

Then point `.gitmodules` at the fork, commit the new submodule pointer and
delete `patches/delta-studio`.
