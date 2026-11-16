Licenses
========

Engine Simulator is MIT licensed (engine-sim.txt). This folder holds the
licenses of everything else this build contains:

- delta-studio, piranha, simple-2d-constraint-solver, csv-io and discord-rpc
  are compiled into the programs (MIT and similar licenses, see each file).
- The Silkscreen font by Jason Kottke (silkscreen-font.txt).
- The libraries the build took from vcpkg, one file per package, named after
  the package: SDL2 and SDL2_image (zlib license), libogg and libvorbis (BSD),
  LAME (mp3lame.txt, LGPL), Prism (ethindp-prism.txt, MPL-2.0) and the
  libraries they use.

LAME is a separate DLL (libmp3lame.dll on Windows), so it can be replaced with
another build of the same version. Its source is at https://lame.sourceforge.io
and Prism's at https://github.com/ethindp/prism.

The Windows download also contains the Microsoft Visual C++ runtime DLLs
(vcruntime140*.dll, msvcp140*.dll, concrt140.dll), which Microsoft allows
programs built with Visual Studio to include.

Each release is built from its tagged commit by .github/workflows/release.yml;
that commit and its submodules are the complete source.
