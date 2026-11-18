# CocoaCandid

Record and stream, without the complexity. CocoaCandid is the screen recorder and streaming app of the Cocoa suite: a native Windows app built on libobs, the engine inside OBS Studio, with a simple interface in the style of CocoaShot and CocoaAuthor.

It opens straight to your last scene. There's no splash or recent-files screen.

## Features

- **Sources**: a whole screen, an app window, an **area** you draw like the Snipping Tool, games, cameras and capture cards, microphones, computer sound, sound from one app, images, slideshows, video and sound files, text, colour blocks, nested scenes, and copies of existing sources.
- **Canvas**: drag, resize (corners keep the shape, Shift frees it) and snap feeds to the edges and centre. Set any canvas and output size, or click **Crop canvas to feed** to make the canvas exactly the size of the selected feed, with one-click undo.
- **Scenes**: switch with a click, F1 to F12 or global hotkeys, with fade, cut, swipe, slide and other transitions. **Studio mode** lets you edit a preview and then send it live.
- **Properties and filters**: every plugin setting, position, size, rotation, crop, blend mode and scaling filter. Filters include colour correction, chroma and colour key, LUTs, masks, sharpen, scroll, noise suppression, compressor, limiter, gain, noise gate and the NVIDIA effects.
- **Audio mixer**: level meters, volume, mute, listening (monitoring), sync delay, mono downmix and filters for each sound.
- **Record**: MP4 that survives a crash (hybrid MP4), MKV, MOV, MPEG-TS or FLV, at stream quality, high, indistinguishable or lossless, on NVIDIA, Intel or AMD hardware encoders, with pause.
- **Stream**: Twitch, YouTube, Kick and every service OBS knows, or a custom RTMP, SRT or WHIP server, with automatic reconnect and dynamic bitrate.
- **Replay buffer**, **virtual camera**, **screenshots**, **full-screen or windowed projector**, **global hotkeys** and live stats (CPU, fps, render time, dropped frames, bitrate, free disk space).
- **First launch**: the CocoaSuite hello and a short tour of the window, ending with a choice of Classic, Compact or Studio layout.
- Dark and light themes, and the Garnet or Midnight app icon.

## Install

Download the setup or the portable zip from the [latest release](https://github.com/CocoaSuite/CocoaCandid/releases/latest). Windows 10 (2004) or newer, 64-bit.

## Command line

`--startrecording`, `--startstreaming`, `--startreplaybuffer`, `--startvirtualcam`, `--scene "Name"`, `--minimize`, `--no-welcome` and `--quit-after <seconds>`.

## Building

Releases are built by [GitHub Actions](.github/workflows/build.yml): the workflow fetches OBS Studio's source at a fixed version, puts CocoaCandid in place of OBS's own frontend and builds libobs, the plugins and CocoaCandid together, so OBS's packaging bundles Qt, FFmpeg and every plugin next to `CocoaCandid.exe`.

To publish a release, raise `CANDID_VERSION` in `CMakeLists.txt` and push to `main`: once the build passes, the workflow tags that version and publishes the setup and portable zip as the latest release. Pushing a `v*` tag works too.

For development on Linux, CocoaCandid also builds against an installed libobs:

```sh
sudo apt install libobs-dev obs-plugins obs-studio qt6-base-dev qt6-svg-dev ninja-build
cmake -B build -G Ninja && ninja -C build
./build/CocoaCandid
```

## Licence

CocoaCandid is free software under the GNU General Public License, version 2 or later, because it is built on libobs. See [LICENSE](LICENSE). The Inter font is under the SIL Open Font License ([resources/fonts/LICENSES.txt](resources/fonts/LICENSES.txt)).
