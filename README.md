# Engine Simulator
![Alt text](docs/public/screenshots/screenshot_v01.png?raw=true)
---
# Engine Simulator has moved!

To get the newest releases of the game, [click here](https://github.com/Engine-Simulator/engine-sim-community-edition).
---

## About this branch

This branch collects work from the community forks of the original source code (which is no longer maintained) on top of the last upstream commit:

- **Builds on Windows and Linux** (bobsayshilol's SDL port, with Windows fixes) and no longer needs Boost
- **Forced induction**: superchargers and turbochargers ([docs/forced-induction.md](docs/forced-induction.md))
- **Camshaft reduction ratio, `sparks_per_cycle` and intake atmosphere** for 2-stroke and steam engines and altitude/boost setups
- **Smart gear shifting** (rev-matched up/down shifts, toggled with `K`), brakes (`.`), instant or average fuel consumption and an odometer
- **Audio fixes** for underruns and a data race in the synthesizer, plus a UDP RPM input ([RPM_SYNC.md](RPM_SYNC.md))
- **Bug fixes** to the physics, rendering and memory handling, and a test suite that passes
- **Screen reader support**: speech and braille through NVDA, JAWS, Narrator and others, keys that read out every gauge, and keyboard alternatives for everything the mouse did ([docs/accessibility.html](docs/accessibility.html))
- **Engine builder**: a web form ([tools/engine-builder/index.html](tools/engine-builder/index.html), works offline and with screen readers) that writes engine scripts from plain values, for 14 layouts from singles to V12s, and reopens them for editing
- **Game audio export**: `engine-sim-export` renders seamless RPM loops, one-shots and RPM sweeps as Ogg, WAV and MP3 with a JSON manifest, ready for Unity, Unreal, Godot, FMOD or Wwise ([docs/game-audio-export.md](docs/game-audio-export.md))
- **76 community engine scripts** and 7 extra UI themes (metric variants, Catppuccin), each checked to compile and run

See [COMMUNITY.md](COMMUNITY.md) for where every change came from.

## What is this?

This is a real-time internal combustion engine simulation **designed specifically to produce engine audio and simulate engine response characteristics.** It is NOT a scientific tool and cannot be expected to provide accurate figures for the purposes of engineering or engine tuning.

## How do I install it?

This is a code repository and might not look like other software that you're used to downloading and installing (if you're not familiar with programming). To download a ready-to-use version of the application, navigate to the [releases page](https://github.com/ange-yaghi/engine-sim/releases), find the most recent release (ex. `v0.1.5a`), click "Assets" and download the .zip file with a name that starts with `engine-sim-build`. Unzip this file, then run `bin/engine-sim-app.exe`. The simulator should then start normally.

Check out [our Frequently Asked Questions](https://github.com/ange-yaghi/engine-sim/wiki/Frequently-Asked-Questions) if you need more details.

## How do I use it?

The UI is extremely minimalistic and there are only a few controls used to interact with the engine:

| Key/Input | Action |
| :---: | :---: |
| A | Toggle ignition |
| S | Hold for starter |
| D | Enable dyno |
| H | Enable RPM hold (see below for instructions) |
| G + Scroll | Change hold speed |
| F | Enter fullscreen mode |
| I | Display dyno stats in the information panel |
| Shift | Clutch (hold spacebar at the same time to slowly engage/disengage) |
| Up Arrow | Up Gear |
| Down Arrow | Down Gear |
| . | Brake (hold) |
| K | Toggle smart shifting (off by default): the clutch and throttle are worked for you to rev-match each shift, at most half a second |
| L | Cycle the volume leveler: ORIGINAL (default, the original peak leveler), OFF (fixed gain with only a limiter, so revving gets squashed hard) and LOUDNESS (steady loudness from idle to full revs) |
| Z + Scroll | Volume |
| X + Scroll | Convolution Level |
| C + Scroll | High frequency gain |
| V + Scroll | Low frequency noise |
| B + Scroll | High frequency noise |
| N + Scroll | Simulation frequency |
| M | Increase view layer |
| , | Decrease view layer |
| Enter | Reload engine script |
| Escape | Exit the program |
| Q, W, E, R | Change throttle position |
| Space + Scroll | Fine throttle adjustment |
| 1, 2, 3, 4, 5 | Simulation time warp |
| Tab | Change screen |
| P | Toggle Average/Instant fuel consumption |
| Page Up / Page Down | Same as one step of the mouse wheel, with any of the keys above that use Scroll |
| O | Open an engine script (same as the LOAD ENGINE button). With speech on, a spoken list of engines by name instead (see the accessibility guide) |
| [ and ] | Previous or next engine, in the same order as the engine list (the two keys to the right of P) |
| F5, F6, F7, F8, F9, F11, F12 | Screen reader: read out the engine, vehicle, dyno, air, fuel, engine info and sound settings |
| Home | Screen reader: repeat the last announcement |
| J | Screen reader: change how much is announced, or turn speech off (also turns on the Windows voice without a screen reader) |
| F4 | Open the accessibility guide |
| LOAD ENGINE button | Pick an engine script (`.mr`) to run instead of the one in `main.mr` |
| LOAD THEME button | Pick a theme script (`assets/themes`) |
| RELOAD button | Reload the current script (same as Enter) |
| EXIT button | Exit the program (same as Escape) |

An engine or theme picked with the buttons lasts until the app is closed; edit `assets/main.mr` to change the default. If a script has an error, the current engine keeps running and the message panel points to `error_log.log`.

### Using the RPM hold
The RPM hold feature will hold the engine at a specific RPM and also measure the engine's horsepower and torque at that RPM. You can enable RPM hold by pressing the `H` key. **You must then enable the dynomometer** (press the `D` key) in order for the RPM hold to take effect. To change the hold speed, hold the `G` key and scroll with the mouse wheel. The RPM hold will be shown on the `DYNO. SPEED` gauge in the lower left of the screen.

## Why is the code so sloppy?

I wrote this to demo in a [YouTube video](https://youtu.be/RKT-sKtR970), not as a real product. If you would like it to become a usable product please reach out to me or join my Discord (link can be found in the description of the aforementioned YouTube video). I use this codebase for my own purposes and so it might change frequently and without warning.

## How do I build it? (Ignore this section if you're not a developer!)

The project builds on Windows (Direct3D 11) and Linux (SDL2 + OpenGL). Boost is no longer needed.

### Step 1 - Clone the repository
```git clone --recurse-submodules <url of this repository>```

If you already cloned it, run `git submodule update --init`. A few fixes for the pinned delta-studio submodule are applied automatically when CMake configures the project, see [patches/README.md](patches/README.md).

### Step 2 - Install the dependencies

You need CMake, a C++17 compiler, SDL2, SDL2_image, Flex and Bison.

**Windows** (Visual Studio 2022): install SDL2 with [vcpkg](https://vcpkg.io) and Flex/Bison with [WinFlexBison](https://github.com/lexxmark/winflexbison) (for example `choco install winflexbison3`):

```
vcpkg install sdl2:x64-windows sdl2-image:x64-windows libvorbis:x64-windows mp3lame:x64-windows
```

**Linux** (Debian/Ubuntu):

```
sudo apt install build-essential cmake libsdl2-dev libsdl2-image-dev flex bison
```

libvorbis and LAME are optional: they let `engine-sim-export` write Ogg Vorbis and MP3 (WAV always works). CMake looks for their CMake packages (`Vorbis`, `Ogg`, `mp3lame`), which vcpkg provides; on Linux use `vcpkg install libvorbis mp3lame` and pass vcpkg's toolchain file to CMake. The configure output says which formats were enabled.

[Prism](https://github.com/ethindp/prism) is optional too: it gives the simulator speech and braille output through screen readers (`vcpkg install ethindp-prism:x64-windows-static-md`). Without it the screen reader keys stay silent. Its port needs CMake 4 for MSVC; with an older vcpkg (such as the one bundled with Visual Studio) add `--overlay-ports=cmake/vcpkg-overlay-ports`, which builds it as C17. The configure output says whether screen reader output is enabled.

### Step 3 - Build and Run

Windows, from the root directory of the project:

```
cmake -S . -B build -A x64 -DCMAKE_TOOLCHAIN_FILE=<vcpkg root>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

Linux:

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Run the app with the build directory as the working directory so that it finds `../assets`: `cd build`, then `./engine-sim-app` on Linux or `Release\engine-sim-app.exe` on Windows (Visual Studio's debugger already uses that directory). On Windows the vcpkg toolchain copies the SDL2 DLLs next to the executable. On Linux, scripts in `~/.local/share/engine-sim/assets` take priority over the bundled ones, and `ENGINE_SIM_DATA_ROOT` points the app at a different data directory.

With CMake 4 add `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`, since some submodules still declare a very old minimum version.

`ctest --test-dir build -C Release` runs the unit tests, `engine-sim-export --out <dir>` exports a game audio set for the engine in `assets/main.mr` (or `--script <file.mr>`, see [docs/game-audio-export.md](docs/game-audio-export.md)), and `boost-probe <script.mr> [rpm ...]` runs an engine script headlessly on the dyno (handy for checking a new engine). `shift-probe <script.mr>` drives the car through a set of up- and downshifts and prints whether each one completes.

## Patreon Supporters

This project was made possible by the generous donations of the following individuals!

### Grease Monkeys

|<!-- -->|<!-- -->|<!-- -->|<!-- -->|<!-- -->|
|-|-|-|-|-|
|Devin@Hondatuningsuite|nut|Devin C Martinez|WelcomeCat|Saints Sasha|
|Ida 8858|Emily|Steelorse |Kruddy|Sgt. Fluff|
|darcuter|FatFluffyFox|Benton1234|Jim C K Flaten|The Zuck|
|Blade Skydancer|Ye' old apple|Hayden Henderson|AlphaX|Lucas Martins Bündchen|
|Jay Dog|damo|IBS-IS-CRAP|Snowy|Noah Greenberg|
|Eisberg|Brendan M.|Alex Layton|Lukas Bartee|Thibaut Dubuisson|
|The Cheeze Ity|JoeJimTom|MichaelB450|Björn|Bartdavy|
|sasha bandelier|Caleb Black|COOKIES|Andrew Cooper|asimo3089|
|Vim Wizard|Kevin Arsenault|Carl Linden|Kele Tappi|Kroklethon|
|labourateur|viperfan7|SlimmyJimmy|Jason Becker|Sascha Kamp|
|ves|Supernalboot |BeamNG|Paul Harrison|Tyler Russell (Nytelife26)|
|nicholas jacobs|DrDotMadness|AVeryPlainTyler|Zach Perez|Paul Schaefer|
|Clay Bauer|CR33DYM0N14|julien nadeau|Patt313|Philip Edwards|
|RegularRuby670|Mateusz Ładosz|FémLol Stúdió|Crazy Yany|Elden|
|Tristan Walker|Matthew McDonald|Jan-Sander Huiting|Mitchell Almstedt|Dylan Lebiedz|
|Name Here|LoganBoi FNAF|Epic Randomness|MrPiThon|mike |
|dung|Alvaro ArroyoZamora|Skinna Godwin|BeppoBarone|レナVA|
|Sabata |Brady Fulham|Powerpuncher |NK10K|Gavin Osowski|
|Orbitstrider|Steven Doyle|Jaksu2696|Toni |Devin Abolins|
### Tuners

|<!-- -->|<!-- -->|<!-- -->|<!-- -->|<!-- -->|
|-|-|-|-|-|
|Boosted Media|Matthew McLennan|Venican|Lyan le Golmuth|Alberto R.|
|BetaToaster|Akira Takemoto|J Anderson|Apolly007|LexLuther|
|xilophor|Robert K|viktor lind|Adrian Kucinski|sarowie .|
|Chris Fischer|Marlod|Chase Hansen|Aidan Szalanski|Andrew Taylor|
|Jason Hwang|Juuso Natunen|Ian Moss|PickleRick |Beljim46|
|RSOFT92|UCD|Sped|OldManJenkins|James Hart|
|Kalle Nilsson|XxBrasta455xX |Colin Sandage|Dakota Mackinnon|Carter Kopp|
|Jakub Kozak|CJ Plessas|Loizeau|Charles Mills|YellowLight|
|Didrik Esbjug|Alessandro Dal Pino|Carter Williams|Robert D|Cadence Plume|
|BLANK|Provenance EMU|Dylan Engler|Nathan Rojas|Cornelius|
|Acid|larsloveslegos|Maxime Desages|GM|BreadForMen|
|Devin Freeman|Lieven RYCKEBOER|Amelia Taylor|Jelle Plukker|sodmo |
|Jimmy Briscoe|Cirithor|Martin .K|DMartland|Lucas Diem|
|Richard Budíček|Jack Sheppeard|Meemen|Anderson Huynh|NPException|
|Mattia Villa|C|AIDAN POWELL|Brenn_the_Otter|Lane Mosier|
|Ceze |oranjest1|Jw|ISON |Mathew Graham|
|MACHINA|John Crowell|Asher Blythe|Cronos Skies|Matt Amott|
|CpTKugelHagel|Simon Krayer|Caleb Bek|Monster Man25|GeneralMoineau|
|EsuKurimu|Caleb Sartin|Jared L.|Hunter Wood|Ben Poole|
|Steven Victoria|Jordan Zondlak|Agelessgod|Christopher Fahs|Jonathan Vincent|
|Dalton Guillot|Simon Stojanovic|Andrew Urbanczyk|deniaL|Tyler Hughes|
|vPam |Justin Kruithof|Curtis C Coomber|Sawyer Clark|Mike Hart|
|Ciro Rancourt|Miles Guo|Rewind |E=mc^2|Keaton Call|
|J.Es|Jeremy B|Chance Hall|Jack Tompkins|Race Sim Studio|
|Quentin ZAOUI|Floyd Henderson|James Haylow|Milkshiekh |Wyatt Todd|
|User 2820|Leon Schutte|CYBERBUG_JR|sebiii|Keegan|
|Victor Cosiuga|Rolly !|Elias Pettersson|Tyson Makovec|Bill McDermott|
|Phontonic|Simon Armstrong|avec |KidozyGAME (Dead)|Stephan Cote|
|Justin Biggerstaff|Jabba Jubba|notD34THNIGHT|Inventor|Wesley Bear|
|Supersonic2510|Pixel|Simon Bernhardt|Bas Vangermeersch|ToyotaCipra|
|kyle crawford|ApatheticWood|Ben Vaughn|Erich Westhoven|Zack Myers|
|Tbjoern|Vetle Høgås|Derek Thom|Aaron Beck||
### Junior Mechanics

|<!-- -->|<!-- -->|<!-- -->|<!-- -->|<!-- -->|
|-|-|-|-|-|
|Karol Szép|Leon Jordan|Nathan Higginson|Patrick F|Samuel Picard|
|Alexander Fritsch|Lucas Scarpi|Jack Humbert|G2Eneko|SweCreations|
|Marius Becker|Cedric Wille|infernap12 |Julian Dinges|Wamuthas|
|Alex Mason|Hawar Karem|Melonenstrauch|Jacek Dębski|Alex Eastman|
|Darren Taing|Po Wang|Giorgio Iannucci|Levis|Eden|
|Alin Chiparatu|Arjun Mandakath|A.M. |Dylan Ryan|Noah Entrekin|
|GT130|Josh D|generic|Henrik Cohrs|Nic Yetter|
|Dan Fredriksen|153AN1MJ|Rasmus|EpicEcho|Kaur Hendrikson|
|Maddox Partridge|L33TIFY_|Zack Fletcher|teiiio|Mike Zaite|
|Evan Sonin|Christopher Zimmerman|PrefacedVase |funtomr|Triton Alabaster|
|appelpie|Julien Ferluc|AnomalousFerret_|Miles Orozco|Spencer Teeter|
|ThatCanadian|Harry Prabowo|Dylan Rogerson|Jaedyn Allen|Zephyr Sefira|
|Alexander Stone|Mason Little|Wojciech Czop|ryzen5 |Kosta Diamantis|
|Karol Stodolak|Tim van der Linde|Loïc Ruttner|jonthefuzz|AsgarK|
|James Morgan|Elijah |1ntl|Tobias Johansson|Mome |
|P|SOPA_|Shingekuro|Sean King|Russell Marsh|
|Alyx Ranas|Naters305 |ChrisakaMrXD |Nic |sean|
|Zach Hagedorn|Jhon lenon|Everett Butts|Kyan|ranger Nation|
|Hiago Oliveira|Texi|MrRhody|Inglorious Bastard|Marty Mitchell|
|Justin Chao|ManuelS|Cornelius Rössing|Pedro Freire|Anthony Stuart|
|Hubba Nubba|Skychii|Joe Underwood|Xander_|Notbigdank|
|Sander D.|Lars Joosten|Danksa|Metrostation |Myles Wommack|
|Derrick Sampson|Corey Hannen|Matteo La Corte|Octothorp Obelus|David Baril|
|Soyuz Kafire|Ivan Coha|BigElbowski|Apolepth|Julian Krad|
|David Soulieres|Eric Huang|Léo Vias|Riccardo Mariani|Vic Viper|
|Shinkaaaa|Mumaransa |Michael Banovsky|Hendrik Voss|Inverted Blackhat|
|skipyC |Tobias Moor|jaky3 .|Clément LEGRAND|Ian C. Simpson|
|Challier|Jan Przemysław Drabik|Dsand23|Smooth DLX|The German Dude|
|CrazyEagle |Jordon Goodman|HenryWithaG .|Oscar Krula|Brayden Moore|
|Steven|Nall Wolfert|papajonk|Andrew|Ben Kingston|
|Julian Vogl|Maxime Lubrano|MrMekouil|Doudimme|Jacob Hultberg|
|Nolan Orloff|Mike|tobi9899 |Danila Frolkin|Xecotcovach|
|Aj|Carcar404|John Martin|Dominik Greinert|Lukas Stadler|
|Oliver Yang|sonax51|Marcel Kliment|Chris|David Rush|
|LethalVenom13|Dave Osterhoff|Anto1709|Ben|Morgan Munroe|
|Ivor Forrest|Sam Hopkins|Atte |Dax |William Bergström|
|homelessmeme|Thanleft|Zaxerg |Robeloox|Maximilian-Lukas Marz|
|Morgn|Seth Monteleone|playfulmean videos|Lanimations LA|Bram G|
|Benoit Fournier|Bernar Lepiller|Nicolas Baur|the |Snekers|
|Darkmount|Tobiasz Michalik|Aidas Ri|Daniel Postler|Skim_Beeblez|
|Impetus|Thunderbird324|Fred Joss|Krzysztof Radowski|Azerrty|
|Harrison Speck|Matt Baker|BigLynch|Markus Pelto|IMBIBE|
|James L Plummer|Rose Giles|Jonas Brekka|HASTRX|Lepoucehumain|
|Naomi Humin|qkrrudgks|Johann Gross|Janis Knappich|WhatTheDuck|
|테루|Glimple Bort|Jacob Tudisco|Tanner|Julian kaspi|
|nathan gould|Randal Rainis Kruus|Beppierre|Craig Martin|Thomas Bukovsky|
|Colaxe|Robert Oram|Matsuy15 L|Kacpe|Alex Sedlic|
|Mark Benson|Mhenn!|Anders Nelson|Dingus|Rustle|
|Marco Schulz|stratum |brochier gabriel|Thomas|brody of hillcountry|
|Thomas Afford|Brody Blaskie|Martien Gaming|Adrien MC|William A Grubbs|
|Trevo Ph.D.|Donovan Gibson|Polish R3t4rd|Keith Price|LAWL CAKE|
|Rhien Schultz|FireThrow13|Seraphim|Titus Standing|Matt Miklos|
|B Dub|Jonathan Ekman|Al Pomeroy|Vestii|Wil|
|adrian|Airatise|TJ Sinkoski|Shotts SilverStone|Reagan Carbaugh|
|WarAestheticsRebooted|Aidan Case|Casey Bryant Goodwin|Konrad|Adam Larcher|
|Kazar Xin Xiao|Riccardo Marcaccio|William S.|Francis Filion|Loïc |
|Kenny Deane|Blackspots|mike |MXT|Joshua Gibson|
|milky boi|Hagen|gunmaster929 |jgvan |Benny 282|
|Sean Wehner|Christian Poole|Ethan|Tsukiyama Shuu|Ooof_uhhh_haah|
|sano ken ch|Diego Martinez|Chuck|GalaxyFrogs|TheGeForce |
|Chriphost|Carthage|Greg L|Chipskate|Muhammed Mehmood|
|Hamilton Sjoberg|Amina Moh|vSiiFT|Jeremy Wren|Esteban Acosta|
|John A Ullenberg|Michael Morozov|Andrew Webberley|Nathaniel Lim|Aaron Ksicinski|
|Apocalypt|Josh batuzich|Ed|Hunter |Gene Brockoff|
|Redheadspellslinger.|Pablo Magariños|Nilz|Jose Manuel Silva Calvo|AJ|
|Ethan Wille |Aurora|Derek Shunia|Jan|Nope Mircea|
|Giancarlo Cestari|Tanner Edge|brad.|Connor Merrick|Martin Scholer|
|Deppy|Dan Smith|Tyson |Jac Comeau|Itemfinder |
|Tischer Games|Pedro Henrique|BeenWashedUp|martin wolff|Kurt Houben|
|Thomas Onslow|Brendan Puglisi|Kai Anquetil|Rudolph Ignatenko|CloudHackIX|
|Zach Carreau|Jonathan Vanderlyn|Krobivnov|ienergy|Leifster|
|Mikael Kaaronen|Glen|H.Helsing|ange|The Nobles|
|Johnathan Johnson|Juha Merentie|Jim Fares|Tom Marshall|Superferrariman|
|Zakary Zisa|JustTy|晟道 杜|Dnialibr Williams|Takumi Fujiwara|
|Koen van Hal|Jonathan Hill|Marco Siciliano|Kevindosenfutter|Angry Prawn|
|Natharic 67|Rafael Monteiro|Jacob Ashline|ChironTheFloof|Caleb Dauphinee|
|Tony |Zac L|AlainMoto FPV|eirik johan johnsen|Elderet|
|Miles Longmore|lemon head|Viccy|Casey|Kajetan Cupa|
|Conejero00|Bill Gricko|A cow wearing a turban|Danni Nowicki|Udo Schmidt|
|Tyler Swords|Constellation Gaming|Manimo|valentine|Jules Schattenberg|
|Brandon Crotts|Philipp Popetschnigg|Tiziano Della Fazia|goodgamer1109|Joshua Thomas|
|Jeff Testa|Avery Snyder|Josh Kern|Triptagram|Bayon Antoine|
|Iván Juárez Núñez|Amery Martinat|ElArGee|Cory Green|lucas Di lorenzo|
|Caleb Sandersier|||||
