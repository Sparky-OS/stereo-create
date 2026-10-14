# stereo-create

Make stereo pictures and videos in other stereo formats: "Create Stereo" in the context menu of stereo files (Dolphin and every program with KDE's file item actions), and the `stereo-create` command behind it, for scripts too.
One engine, two front ends.

The menu appears only on stereo content, decided by the shared stereo layer ([stereo-layer](https://github.com/Sparky-OS/stereo-layer)): a mark, a stereo format, or the shape rule, with the user's swap-eyes correction kept with the file.
New files go next to their source and never replace a file (`name (1).ext`, as KIO names copies).
Each run is one job in the notification area, with progress, time left and cancel; a selection of several files is one job.

Part of Sparky Stereo OS, Daniel Ramos's edition of SparkyLinux (by Paweł "pavroo" Pijanowski).

## Pictures

| Target | `--to` | What is written | Specification |
|---|---|---|---|
| Proof pair | `proof` | the MPO and the red/cyan anaglyph below | |
| MPO | `mpo` | left view first, both pictures with their MP Attribute IFD (MP Individual Image Number 1 and 2, Base Viewpoint Number 1), type Multi-frame Disparity | CIPA DC-007 (Multi-Picture Format) |
| JPS | `jps` | one JPEG, the right view on the left (cross view), with the APP3 `_JPSJPS_` descriptor: stereo, side by side, full size, right view first | JPS (Stereo JPEG), the StereoPhoto Maker definition |
| Side by side PNG | `sbs-png` | the left view on the left, the `sTER` chunk mode 1 | PNG Extensions, `sTER` |
| PNS | `pns` | cross view, the `sTER` chunk mode 0 | PNG Extensions, `sTER` |
| Anaglyphs | `anaglyph-red-cyan`, `anaglyph-red-cyan-crt`, `anaglyph-green-magenta`, `anaglyph-amber-blue` | JPEG, or PNG with `--lossless-anaglyph` | Eric Dubois, "A projection method to generate anaglyph stereo images", ICASSP 2001 |

The `sTER` subimages start on a multiple of 8 columns: a view `w` pixels wide is followed by `(8 - w % 8) % 8` padding columns.
MPO and JPS go through KImageFormats' stereo writer; the JPS descriptor is added here.

The anaglyphs mix both views with Dubois's least-squares matrices in linear light (the sRGB transfer, IEC 61966-2-1), as KWin's anaglyph modes do.
Red/cyan has two sets, both those KWin uses: today's screens (Zhang and McAllister) and CRT screens (Sanders and McAllister); green/magenta and amber/blue are Dubois's own, the two other pairs the shared stereo layer reads.

## Videos

The views are taken out of the source's packing (side by side or top and bottom, full or half; row, column and frame-sequential packings through FFmpeg's `stereo3d` filter), each scaled to its own shape, and packed again.
H.265 stays H.265, any other codec becomes H.264.
Sound, subtitles and attachments are copied; the colour description (primaries, transfer, matrix, range) and the bit depth stay, and so do HDR10's mastering display and light level.

| Target | `--to` | Container and marks | Specification |
|---|---|---|---|
| Full side by side | `video-full-sbs` | Matroska, StereoMode 1, frame packing SEI type 3 | RFC 9559 §5.1.4.1.28.3; H.264 Table D-8, H.265 Table D.8 |
| Half side by side, for 3D TVs | `video-half-sbs` | as above, both views in the frame of one | same |
| Top and bottom, for 3D TVs | `video-top-bottom` | Matroska, StereoMode 3, SEI type 4 | same |
| MP4 | `video-mp4` | full side by side, SEI type 3, the `st3d` box (stereo_mode 2) | Spherical Video V2 (`st3d`); H.264/H.265 |
| Anaglyph video | `video-anaglyph` | Matroska, Dubois red/cyan, StereoMode 10 (anaglyph cyan/red) | RFC 9559; Dubois |
| Frame grab | `frame-mpo`, `frame-jps`, `frame-anaglyph` | a frame (by default a third into the video, `--time`) as a picture target | as the pictures |

The frame packing SEI is inserted by the `frame_packing` option of FFmpeg's `h264_metadata` and `hevc_metadata` bitstream filters (the edition's FFmpeg), and StereoMode is set with MKVToolNix's `mkvpropedit`.
The MP4 is muxed from the marked Matroska file; FFmpeg's MP4 muxer also writes Apple's `vexu` box for any stream with Stereo 3D side data, which says only that two views exist and which readers prefer over `st3d`, so it is removed.
An anaglyph video needs a source in standard dynamic range.

## Command line

```
stereo-create --to TARGET [--quality 1-100] [--time SECONDS] [--lossless-anaglyph] [--no-notification] FILES...
stereo-create --probe FILES...
```

The new files are printed, one per line; the exit status is 0, 2 when a file failed, 3 when the job was cancelled.
`--probe` prints how each file is taken: stereo or flat, the packing, the eye order, and where that came from (`mark`, `user`, `format`, `detection`, `shape`).

## Building

CMake with ECM, Qt 6, KF6 (CoreAddons, I18n, JobWidgets, KIO), stereo-layer 1.4 and FFmpeg's libavformat, libavcodec and libavutil.
At run time: FFmpeg, MKVToolNix and KImageFormats with the stereo plugin.

## License

GPL-2.0-or-later.
