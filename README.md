[![License](https://img.shields.io/badge/licence-MIT-green.svg?style=flat)](LICENSE)
[![Wikipedia](https://img.shields.io/badge/Wikipedia-000000?style=flat&logo=wikipedia&logoColor=white)](https://en.wikipedia.org/wiki/Thor_Vector_Graphics)
[![Discord](https://img.shields.io/badge/Community-5865f2?style=flat&logo=discord&logoColor=white)](https://discord.gg/n25xj6J6HM)
[![OpenCollective](https://img.shields.io/badge/OpenCollective-84B5FC?style=flat&logo=opencollective&logoColor=white)](https://opencollective.com/thorvg)

# Thor Pirates

<p align="center">
  <img width="800" height="auto" alt="image" src="https://github.com/user-attachments/assets/f9d0de55-5144-4a84-950e-017ce2c441f4" />
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
<br />
<p align="center">
  <img width="800" height="auto" alt="thorvg" src="https://github.com/user-attachments/assets/cda8e26e-acbc-44fb-9b16-4c50b3b991f6" />
</p>

<p align="center">
  <strong><a href="https://youtu.be/eBvL9oUaQ3A?si=AKmsjYn25uMLLoFB">Watch the full video!</a></strong>
</p>

## Build & Run
Install Meson, Ninja, pkg-config, and a C++17 compiler, then install [ThorVG](https://github.com/thorvg/thorvg) and [ThorVG Toolkit](https://github.com/thorvg/thorvg.toolkit). The recommended ThorVG build option is
```
-Dloaders="ttf, svg" -Dengines=gl (or wg)
```
Ensure `thorvg-toolkit` and `thorvg-1` are discoverable by pkg-config. Build and run from the repository root so relative asset paths resolve correctly:
```
$ meson setup build
$ ninja -C build
$ ./build/thorvg-piriates
```

Select the rendering backend with `-e <engine>`. The default is `gl`:
```sh
$ ./build/thorvg-pirates -e gl  # OpenGL
$ ./build/thorvg-pirates -e wg  # WebGPU
$ ./build/thorvg-pirates -e sw  # CPU (Software)
```
GPU backends require the corresponding support in your ThorVG and ThorVG Toolkit builds.

## Features

- Demonstrates how well ThorVG lends itself to AI code generation through a complete, interactive demo game.
- All source code was generated using Codex (GPT-5.6 Sol), from rendering and physics to gameplay and environmental effects.
- Built entirely on ThorVG, with no external graphics or game engine dependencies. All visuals are rendered as vector graphics in real time.
- Features physics-based gameplay, including ballistic cannon trajectories, ship debris, dynamic water movement, and interactive environmental effects.

## Authors

* **[Hermet Park](https://github.com/hermet)**
* **Codex (GPT-5.6 Sol)**
