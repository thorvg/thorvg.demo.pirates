[![License](https://img.shields.io/badge/licence-MIT-green.svg?style=flat)](LICENSE)
[![Wikipedia](https://img.shields.io/badge/Wikipedia-000000?style=flat&logo=wikipedia&logoColor=white)](https://en.wikipedia.org/wiki/Thor_Vector_Graphics)
[![Discord](https://img.shields.io/badge/Community-5865f2?style=flat&logo=discord&logoColor=white)](https://discord.gg/n25xj6J6HM)
[![OpenCollective](https://img.shields.io/badge/OpenCollective-84B5FC?style=flat&logo=opencollective&logoColor=white)](https://opencollective.com/thorvg)

# Thor Pirates

<p align="center">
  <img width="600" height="auto" src="https://github.com/thorvg/thorvg.janitor/blob/main/title.png">
</p>

**"Rule the Seas, One Cannonball at a Time!"**

The age of piracy is at its peak. Across the open seas, rival pirate captains battle for treasure, glory, and control of the trade routes. In these dangerous waters, only the most skilled commander can survive.<br />
<br />
Take command of your pirate ship and engage in tactical naval duels against enemy vessels. Every shot counts—aim carefully, adjust for distance, and unleash devastating cannon fire in classic trajectory-based combat inspired by legendary artillery games.<br />
<br />
Sink enemy ships, dominate the battlefield, and prove yourself as the true ruler of the seas. With skill, timing, and a bit of pirate luck, victory is only a cannonball away.
<br />
<br />
Powered entirely by ThorVG, this demo showcases real-time vector rendering, animation, and interactive gameplay on the high seas.<br />

<p align="center">
  <img width="800" height="auto" src="https://github.com/user-attachments/assets/8a4bd16a-bb72-4b41-b007-eadc2220d1eb"/>
</p>

## Build & Run
Install Meson, Ninja, pkg-config, and a C++17 compiler, then install [ThorVG](https://github.com/thorvg/thorvg) and [ThorVG Toolkit](https://github.com/thorvg/thorvg.toolkit). The recommended ThorVG build option is
```
-Dloaders="svg,ttf,jpg"
```
Ensure `thorvg-toolkit` and `thorvg-1` are discoverable by pkg-config. Build and run from the repository root so relative asset paths resolve correctly:
```
$ meson setup build
$ ninja -C build
$ ./build/thorvg-janitor
```

Select the rendering backend with `-e <engine>`. The default is `gl` (OpenGL rendering):
```sh
$ ./build/thorvg-pirates -e sw  # CPU (Software)
$ ./build/thorvg-pirates -e gl  # OpenGL
$ ./build/thorvg-pirates -e wg  # WebGPU
```
GPU backends require the corresponding support in your ThorVG and ThorVG Toolkit builds.

## Features

- Designed as a demo game to showcase the real-time rendering capabilities of the ThorVG engine.
- Built entirely on ThorVG with no external graphics or game engine dependencies. All visuals are rendered as real-time vector graphics.
- Features physics-based gameplay, including ballistic cannon trajectories, ship debris, dynamic water movement, and interactive environmental effects.
- The entire source code was generated with AI (Codex, GPT-5.6 Sol), demonstrating how quickly a complete interactive game can be built on top of ThorVG.

## Authors

* **[Hermet Park](https://github.com/hermet)**
