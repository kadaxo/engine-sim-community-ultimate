Licenses
========

Engine Simulator is MIT licensed (engine-sim.txt). This folder holds the
licenses of everything else this download contains:

- delta-studio, piranha, simple-2d-constraint-solver, csv-io and discord-rpc
  are compiled into the programs (MIT and similar licenses, see each file).
- The Silkscreen font by Jason Kottke (silkscreen-font.txt).
- The libraries the build took from vcpkg, one file per package, named after
  the package: SDL2 and SDL2_image (zlib license; the Linux download uses the
  system's instead), libogg and libvorbis (BSD), LAME (mp3lame.txt, LGPL),
  Prism (ethindp-prism.txt, MPL-2.0) and the libraries they use. On Linux
  those include GLib, glibmm, libsigc++, libspeechd and gettext's libintl,
  which are LGPL too.
- The Linux programs also contain GCC's C++ library (libstdc++), which the
  GCC Runtime Library Exception allows.

On Windows, LAME is a separate DLL (libmp3lame.dll), so it can be replaced
with another build of the same version. On macOS and Linux all of these
libraries are linked into the programs; to use a modified version of one,
rebuild the programs from the source below with it.

Each release is built from its tagged commit by .github/workflows/release.yml.
That commit, its submodules and the vcpkg commit the workflow names (vcpkg
fetches each package's original source) are the complete source and build
recipe. LAME's own source is at https://lame.sourceforge.io and Prism's at
https://github.com/ethindp/prism.

The Windows download also contains the Microsoft Visual C++ runtime DLLs
(vcruntime140*.dll, msvcp140*.dll, concrt140.dll), which Microsoft allows
programs built with Visual Studio to include.
