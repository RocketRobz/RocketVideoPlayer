<p align="center">
 <img src="https://github.com/RocketRobz/RocketVideoPlayer/blob/master/resources/logo.png"><br>
  <a href="https://gbatemp.net/threads/release-rocket-video-player-play-videos-with-the-ultimate-in-picture-quality.539163/">
   <img src="https://img.shields.io/badge/GBAtemp-Thread-blue.svg" alt="GBAtemp Thread">
  </a>
</p>

To play videos, you'll need to convert them to the Rocket Video (`.rvid`) format using [Vid2RVID](https://github.com/RocketRobz/Vid2RVID/releases).

# Features
- Support for videos up to 60FPS (compatible with even the DS & DS Lite systems)!
- 256 color video playback (combined with dithering to simulate more colors) in the RGB565 color format!
- Support for dual-screen videos (up to 30FPS)!
- Support for audio up to 32khz Mono.
- The video UI from the "Nintendo DSi + Internet" app, with the title bar now containing the console's set color.
- Console-based file browser to search for your `.rvid` file.

# Video UI Controls
- `A` or touch `Play`/`Pause` button: Play/Pause video
- `L` shoulder or touch the left of the play bar: Stop video
- D-Pad `Left`/`Right`: Jump 5 seconds (30 seconds with `R` shoulder held)
- D-Pad `Down`/`Up`: Jump 10 seconds (1 minute with `R` shoulder held)
- D-Pad `Left`+`Down`/`Up`+`Right`: Jump 15 seconds (2 minutes with `R` shoulder held)
- `B` or touch `Return`: Exit video

# Why 256 colors?

The choice for 256 color video was made due to full bitmap video loading being slow (ex. 4:3 24FPS videos slowing down later during playback on DS & DS Lite), and allows videos that are 24-30FPS or higher to play without slowdown.     
The DS and DSi consoles support 256 colors by storing them in the background palette space, and each pixel of the video frames would point to the color's number, cutting each frame size in half when compared to storing each pixel as the individual colors.

Before Vid2RVID makes the video file, dithering gets applied to the frames in order to simulate/achieve more on-screen colors to reduce color banding. Results may vary depending on the frame or video.

# Credits
* [Gericom](https://github.com/Gericom): LZ77 decompressor code from [EFE/EveryFileExplorer](https://github.com/Gericom/EveryFileExplorer), and frame rate adjustment code from [FastVideoDSPlayer](https://github.com/Gericom/FastVideoDSPlayer) (though not used for 24FPS/48FPS videos).
* [devkitPro](https://github.com/devkitPro): nds-hb-menu's file browsing code, and the use of devkitPro, devkitARM, libnds, and libfat.
