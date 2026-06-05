# Fragments

Fragments is a desktop app for building and exporting scripted playlists from video and audio fragments.


> Documentation: [https://ahunior.github.io/fragments/guide.html](https://ahunior.github.io/fragments/guide.html)

## What It Does

Fragments helps you build repeatable playlists from small pieces of video and audio.

- Trim exact ranges, reorder fragments, add delay, mute or keep audio, inspect fragment drafts, recover missing media, and play or export the final playlist.
- Use it when you need a fast fragment-based workflow instead of a full timeline editor.
- The original media files are not modified.

## Quick Start

1. Add local video or audio files to create fragments.
2. Trim each fragment by setting the start and end points visually.
3. Add optional delay, labels, notes, audio settings, volume, speed, and delay color.
4. Use Play All to run the full playlist like one assembled video made of fragments.
5. Save the playlist JSON or export it to MP4 or GIF when you are ready.

The playlist format is JSON:

```json
{
  "version": 1,
  "items": [
    {
      "file": "file:///path/to/video-a.mp4",
      "start": 1.0,
      "end": 12.0,
      "delayBefore": 0.0,
      "delayColor": "#000000",
      "audio": true,
      "volume": 1.0,
      "speed": 1.0
    }
  ]
}
```

## Build

Requirements:

- Qt 6.5 or newer
- CMake 3.24 or newer
- A C++17 compiler

```bash
cmake -S . -B build-desktop \
  -DCMAKE_PREFIX_PATH=/home/jr/Qt/6.11.0/gcc_64 \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++
cmake --build build-desktop
./build-desktop/fragments
```

## Current Scope

- Add media fragments from local files
- Scrub source media and adjust start/end handles visually
- Edit start/end time, delay, delay background color, audio, volume, speed, label, and notes
- Undo and redo playlist edits
- Duplicate fragments
- Keyboard shortcuts for save/open, undo/redo, fragment preview, duplicate, delete, reorder, and setting start/end from the playhead
- Save and load playlist JSON files
- Reopen recent playlists
- Recover missing local media by relinking individual files or scanning a folder
- Play the playlist sequentially with optional delay before each item, so it behaves like one assembled video made of fragments
- Export valid local playlists to a combined MP4 render with ffmpeg
  - Export normalizes mixed media to 1280x720, 30 fps, H.264 video and 48 kHz stereo AAC audio
  - Audio-only fragments render over the chosen delay color; muted or video-only fragments get silent audio
  - Fragment delays export as colored/silent gaps before the fragment starts
- Export valid local playlists to an animated GIF preview
  - GIF export has no audio, renders at 15 fps, scales to 640 px wide, and uses an ffmpeg palette pass

## Package

See [PACKAGING.md](PACKAGING.md) for platform-specific release steps.

Expected release downloads:

- Windows: `fragments-0.44.6-windows-x86_64.zip`
- macOS: `fragments-0.44.6-macos-universal.dmg`
- Linux: `fragments-0.44.6-Linux-x86_64.tar.gz`

Build and run the test suite before packaging:

```bash
cmake --build build-desktop
ctest --test-dir build-desktop --output-on-failure
```

Create the current Linux install tarball from the build tree:

```bash
cpack --config build-desktop/CPackConfig.cmake
```

The generated CPack tarball contains the Fragments binary plus desktop, metainfo, and icon
files. It expects compatible Qt runtime libraries on the target system.

## Install Notes

Fragments export features require [FFmpeg](https://ffmpeg.org/download.html) to be available on `PATH`.
FFmpeg normally includes `ffprobe`, which Fragments uses to inspect media before export.

Recommended checks:

```bash
ffmpeg -version
ffprobe -version
```

MP4 export requires a FFmpeg build with the `libx264` H.264 encoder, AAC encoder, and standard
scale/pad/fps/format/tpad/atempo/adelay/aformat filters. GIF export additionally requires
FFmpeg's palettegen and paletteuse filters.

## License

Fragments is released under the GPL-3.0-or-later license. See [LICENSE](LICENSE).
