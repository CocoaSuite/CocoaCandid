# CocoaCandid user guide

## Getting started

The first time CocoaCandid opens, it says hello and gives a short tour of the window: scenes and sources, the canvas, the settings panel, sound, and recording and streaming. The last step lets you pick the Classic, Compact or Studio layout (this is also in Settings, General). **Help, Welcome tour** shows it again.

Your first scene already shows your main screen, and your computer sound and microphone are in the mixer. Press **Record** to start.

## The window

- **Scenes** (top left): each scene is a layout. Click one to switch to it, or press F1 to F12. Drag to reorder and double-click to rename. Right-click for duplicate, move and remove. The transition and its length are under the list.
- **Sources** (bottom left): what's in the scene, top first. The eye shows or hides a source, the lock stops it moving. Drag to reorder, double-click or F2 to rename, Delete to remove.
- **Canvas** (middle): exactly what gets recorded or streamed. Click a source to select it, drag to move it, drag a corner to resize it (Shift resizes freely), and use the arrow keys to nudge it (Shift moves 10 px). Sources snap to the canvas edges and centre; hold Ctrl to place freely. Right-click for the source menu.
- **Properties and Filters** (right): the selected source's settings, position, size, rotation, crop, blend mode and scaling filter. Filters add effects such as colour correction, green screen (chroma key), LUTs, sharpen and noise suppression; tick a filter to turn it on and off.
- **Audio** (bottom): every sound in use gets a meter and a volume slider. Click the speaker to mute it. The menu has settings, filters, listening (monitoring), sync delay and downmix to mono.
- **Status bar**: CPU, frame rate, render time, dropped or lagged frames, bitrate while live, the encoder and free disk space.

## Adding sources

**Add source** (the + next to Sources, Ctrl+N) shows a tile for each kind. Pick one and its settings appear underneath, while the source already shows on the canvas, so you can judge it by the real picture. **Fit to the canvas** scales it to fit, and **Resize the canvas to this source** makes the canvas the source's size.

**Area** works like the Snipping Tool: the screens freeze and darken, you drag a rectangle (drag the handles or inside it to adjust), then press **Add as source** or Enter. Esc cancels. **Full screen** adds the whole display. The area is a screen capture cropped to your rectangle; change the crop later under Properties, Crop. Ctrl+Shift+A starts it from anywhere.

## The canvas size

The canvas is the space you arrange sources on; the output is the size recordings and streams are made at (Settings, Video). When the selected screen, window, area or camera is a different size from the canvas, a prompt above the canvas offers **Crop canvas to feed**. It makes the canvas exactly the size of that feed and moves everything in the scene with it. **Undo** appears for a few seconds afterwards. The same button is in the bar under the canvas and in the source menu.

## Recording

**Record** starts and stops; the pause button appears while recording. Recordings go to your Videos folder unless you choose another in Settings, Record. MP4 is the default and is written so a crash or power cut doesn't lose the file. Quality can match the stream, or be High, Indistinguishable or Lossless. CocoaCandid uses your graphics card's encoder (NVIDIA, Intel or AMD) when there is one.

**Replay buffer** (Settings, Record) keeps the last stretch of the canvas in memory; the replay button in the top bar, or its hotkey, saves it as a file. **Screenshot** saves a PNG of the canvas to the same folder.

## Streaming

Add your service and stream key in Settings, Stream (**Get key** opens the right page for Twitch, YouTube, Kick, Facebook and Trovo), or choose a custom RTMP, SRT or WHIP server. **Go live** starts and **End stream** stops; CocoaCandid reconnects on its own if the connection drops, and can lower the bitrate while the connection struggles.

## Studio mode

Studio mode (the two-rectangle button, or the Studio layout) shows the scene you're editing next to what's live. Change scenes and sources freely, then press **Send live** to show it to viewers with the transition.

## Other tools

- **Virtual camera**: lets video-call apps use the canvas as a camera. Choose it when installing.
- **Show what's live in a window** (View): a windowed or full-screen projector on any display. Esc closes it.
- **Hotkeys** (Settings, Hotkeys): record, pause, go live, save replay, screenshot, mute the microphone, send live, and switch to scenes 1 to 9. They work while another app, like a game, is in front.

## Settings

- **General**: theme, layout, app icon, snapping, and asking before ending a stream.
- **Stream**, **Record** and **Video** as above. **Encoder settings** opens every option of the chosen encoder.
- **Audio**: sample rate and channels (after a restart), and the devices for computer sound and the microphone.
- **Advanced**: process priority and the settings and log folders.

Settings, scenes and logs are kept in `%APPDATA%\CocoaCandid`.
