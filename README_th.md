# ![d2df](images/doom2df.png) Doom2D Forever

![GitHub License](https://img.shields.io/github/license/Doom2D/Doom2D-Forever) ![Discord](https://img.shields.io/discord/262745434596966411?label=Discord&link=https%3A%2F%2Fdiscord.gg%2FsGpJwMy) ![Static Badge](https://img.shields.io/badge/Telegram-blue?link=https%3A%2F%2Ft.me%2Fdoom2d)

A modern remake of Doom2D, an old platformer created by Prikol Software, based on classic Doom/Doom II. It contains multiplayer (using ENet), powerful map editor and more.

This project is licensed under GPLv3 (https://github.com/Doom2D/Doom2D-Forever/blob/master/COPYING).

## Prebuilt binaries
Mainline binaries are located on project website (https://doom2d.org). They are temporarily not updated, it is recommended to take from the source below.

Nightly binaries you can get here:
https://github.com/Doom2D/nix_actions/releases


### About Linux
Actual Linux (both x86 and x86-64) you can get here (they built by TerminalHash):

* x86 - http://deadsoftware.ru/files/terminalhash/d2df-linux-i386.tar.gz
* x86-64 - http://deadsoftware.ru/files/terminalhash/d2df-linux-amd64.tar.gz

Before using them, you need install next packages from your repositories:
```
DEB-based:
sudo apt install libenet7 libvorbis0a libopus0 libopusfile0 xmp libxmp4 libgme0 libmodplug1 libopenal4 libsdl2-2.0 mpg123

Mageia:
sudo dnf install libenet7 libsdl2.0_0 openal libvorbis0 libopus0 libopusfile0 libgme0 libxmp4

Void Linux:
sudo xbps-install -S xmp libenet SDL2 libopenal libmodplug libvorbis opus opusfile libgme libmpg123

Pacman-based (Arch, Manjaro, etc.):
sudo pacman -S libgme opus opusfile libvorbis mpg123 openal enet libmodplug sdl2
```

On Arch you need build XMP for yourself from AUR.

## How to build
Requirements:
* FPC >= 3.1.1;
* libenet >= 1.3.13;

Create the `tmp` and `bin` directories and then run:
```
  cd src/game
  fpc -g -gl -O3 -FE../../bin -FU../../tmp Doom2DF.lpr
```

Additionally you can add the following options:
```
  System driver:
    * -dUSE_SDL2        Build with SDL 2.0.x
    * -dUSE_SDL         Build with SDL 1.2.x
    * -dUSE_SYSSTUB     Disable I/O management
  Render driver:
    * -dUSE_OPENGL      Build with desktop OpenGL 2.x
    * -dUSE_GLES1       Build with mobile OpenGLES 1.1
    * -dUSE_GLSTUB      Disable rendering
  Sound driver:
    * -dUSE_FMOD        Build with FMOD Ex (4.30.22, other versions may fail)
    * -dUSE_SDLMIXER    Build with SDL_mixer
    * -dUSE_OPENAL      Build with OpenAL 1.1
    * -dUSE_SOUNDSTUB   Disable sound management
  Sound file drivers (OpenAL only):
    * -dUSE_SDL2        Build with SDL 2.0.x for WAV support
    * -dUSE_SDL         Build with SDL 1.2.x for WAV support
    * -dUSE_VORBIS      Build with libvorbis
    * -dUSE_FLUIDSYNTH  Build with libfluidsynth
    * -dUSE_MODPLUG     Build with libmodplug
    * -dUSE_XMP         Build with linxmp
    * -dUSE_MPG123      Build with libmpg123
    * -dUSE_OPUS        Build with libopus
    * -dUSE_GME         Build with libgme
  Other:
    * -dSDL2_NODPI      Build for old libSDL2
    * -dUSE_MINIUPNPC   Build with libminiupnpc for automatic server port
                        forwarding via UPnP
    * -dENABLE_HOLMES   Build with in-game map debugger
    * -dHEADLESS        Build a headless executable for dedicated servers
```

Windows binaries will require the appropriate DLLs (SDL2.dll, SDL2_mixer.dll or
FMODex.dll, ENet.dll, miniupnpc.dll) unless you choose to link them statically.

It's now possible to link Windows' LibJIT and ENet as static libs. You can use:
```
  -dLIBJIT_WINDOZE_STATIC       -- static LibJIT
  -dLIBENET_WINDOZE_STATIC      -- static ENet
  -dLIBMINIUPNPC_WINDOZE_STATIC -- static MiniUPnPC
  -dVORBIS_WINDOZE_STATIC       -- static libogg/libvorbis (only in AL builds)
  -dOPUS_WINDOZE_STATIC         -- static libogg/libopus (only in AL builds)
```

Don't forget to specify lib*.a location with -Fi<...>