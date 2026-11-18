# KdenCut Osmo 360 editor

Open a dual-lens DJI Osmo 360 recording, drag to frame the shot, set camera keyframes and render it with the original audio. The editor keeps the original recording read-only.

## Editing in KdenCut

The KdenCut container includes the editor and its dependencies. Use **Add Clip** or select the OSV through **Browse device files** or the NAS picker. KdenCut opens the 360 editor before adding the clip to the project. Scrub to a frame and drag the picture to aim. Releasing the drag records a camera keyframe. The FOV and Roll sliders also set keyframes. Use the arrow buttons to move between keys. The camera follows the shortest path between headings.

**Pan only (lock pitch)** is on for new recordings. Drag left or right to pan. Vertical mouse movement is ignored, so the view stays at its current elevation. Uncheck it to aim up or down. To tilt the horizon deliberately, scrub to the frame, type an angle next to **Roll**, and click **Key roll**. Releasing the Roll slider also sets a roll key. New recordings start at 0° roll and hold each chosen angle until the next roll key. Panning and zooming have their own keyframes. Existing saved edits keep their original camera movement.

Use **Save edit** to keep an editable `.k360` camera track and reopen it through Add Clip. Keep the source OSV and the edit together when moving media. **Render and import** asks where to save the MP4, renders it, then returns the completed clip to the project bin or the touch timeline that requested it. Choose a writable destination on disk or on a mounted share. Editing an already rendered clip on the timeline uses normal Kdenlive tools. Reopen the camera track to change the framing.

The first still loads directly from the original. Dragging reuses that frame. **Play** prepares an 8 fps silent preview in temporary storage. Final renders use the original frame rate and retain the embedded audio. Preparing a preview and rendering can both be cancelled.

## Output

| Shape | Standard | High | Video |
| --- | --- | --- | --- |
| Landscape | 1920 × 1080 | 3840 × 2160 | H.264, 8-bit |
| Portrait | 1080 × 1920 | 2160 × 3840 | H.264, 8-bit |
| Square | 1080 × 1080 | 2160 × 2160 | H.264, 8-bit |
| 360 panorama | Up to 3840 × 1920 | Up to 7680 × 3840 | HEVC, 10-bit, spherical metadata |

Panorama exports cover the whole sphere and ignore camera keyframes. The editor uses factory lens geometry when available and shows “Generic lens geometry” if it uses the fallback. Close objects crossing the seam can show parallax. The editor has no motion tracking, gyro horizon lock, optical-flow seam correction or D-Log M grading. Full-resolution CPU rendering can take longer than playback.

Lens projection math is adapted from [PanoForge](https://github.com/Belenos-Toutatis/PanoForge) under the included MIT license. The editor and KdenCut integration use GPL-3.0-or-later.
