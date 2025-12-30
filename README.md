<p align="center">
 <img src="https://github.com/RocketRobz/RocketVideoPlayer/blob/master/resources/logo.png"><br>
  <a href="https://gbatemp.net/threads/release-rocket-video-player-play-videos-with-the-ultimate-in-picture-quality.539163/">
   <img src="https://img.shields.io/badge/GBAtemp-Thread-blue.svg" alt="GBAtemp Thread">
  </a>
</p>

To play videos, you'll need to convert them to the Rocket Video (`.rvid`) format using [Vid2RVID](https://github.com/RocketRobz/Vid2RVID/releases).

# Features
- Support for videos up to 60FPS (compatible with even the DS & DS Lite systems)!
- Both 8 BPP (256 colors) and 16 BPP (RGB555/RGB565) videos are supported.
    - Screen color filters are not supported for 16 BPP videos.
- Support for dual-screen videos!
- Support for audio up to 32khz Mono.
- The video UI from the "Nintendo DSi + Internet" app, with the title bar now containing the console's set color.
- Console-based file browser to search for your `.rvid` file.

# Video UI Controls
- `A` or touch `Play`/`Pause` button: Play/Pause video
- `L` shoulder or touch the left tip of the play bar: Stop video
- D-Pad `Left`/`Right`: Jump 5 seconds (30 seconds with `R` shoulder held)
- D-Pad `Down`/`Up`: Jump 10 seconds (1 minute with `R` shoulder held)
- D-Pad `Left`+`Down`/`Up`+`Right`: Jump 15 seconds (2 minutes with `R` shoulder held)
- `SELECT`: Turn off/on bottom screen backlight (only applies to single-screen videos)
- `B` or touch `Return`: Exit video

# Credits
- [AntonioND](https://github.com/AntonioND): [BlocksDS](https://github.com/blocksds/sdk)
- [Gericom](https://github.com/Gericom): LZ77 decompressor code from [EFE/EveryFileExplorer](https://github.com/Gericom/EveryFileExplorer), and frame rate + play bar adjustment code from [FastVideoDSPlayer](https://github.com/Gericom/FastVideoDSPlayer).
- [devkitPro](https://github.com/devkitPro): nds-hb-menu's file browsing code.
