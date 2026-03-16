# MP4 Video Player for Godot

A ready-to-use MP4 video player addon for Godot with audio playback, seeking, playback speed control, timeline-friendly functions, and display options.

This addon provides a custom node called `MP4VideoPlayer`.

---

## Features

* MP4 video playback
* Audio playback
* Play, pause, stop
* Seek to any position
* Restart and play from start
* Toggle play/pause
* Playback speed from `0.5x` to `2.0x`
* Loop support
* Video position, duration, progress, and remaining time helpers
* Formatted time text helpers
* Video width, height, size, fps, and audio info
* Display controls:

  * Expand mode
  * Stretch mode
  * Flip horizontal
  * Flip vertical

---

## Installation

Copy the `addons/MP4Player` folder into your Godot project:

```text
your_project/
├─ addons/
│  └─ MP4Player/
```

Then enable the plugin in:

**Project > Project Settings > Plugins**

---

## Add the Node

Add this node to your scene:

```text
MP4VideoPlayer
```

You can set a video directly from the Inspector using:

* `video_path`
* `autoplay`
* `loop_enabled`

---

## Inspector Properties

### Playback

* `video_path`
  Path to the MP4 file.
* `autoplay`
  Automatically starts playback after opening the file.
* `loop_enabled`
  Restarts the video automatically when it ends.
* `playback_speed`
  Playback speed from `0.5` to `2.0`.

### Display

* `expand_mode`
* `stretch_mode`
* `flip_h`
* `flip_v`

These properties control how the video is displayed inside the node.

---

## Basic Example

```gdscript
extends Control

@onready var player = $MP4VideoPlayer

func _ready() -> void:
	player.open("res://video.mp4")
	player.play()
```

---

## Main Functions

### Playback control

```gdscript
player.open(path)
player.play()
player.pause()
player.stop()
player.seek(seconds)
player.restart()
player.play_from_start()
player.toggle_play_pause()
```

### State helpers

```gdscript
player.is_playing()
player.has_video()
player.is_video_loaded()
```

### Time helpers

```gdscript
player.get_video_position()
player.get_video_duration()
player.get_video_progress()
player.get_remaining_time()
player.set_video_position(seconds)
```

### Formatted time text

```gdscript
player.get_video_position_text()
player.get_video_duration_text()
player.get_remaining_time_text()
```

### Video info

```gdscript
player.get_video_width()
player.get_video_height()
player.get_video_size()
player.get_video_fps()
```

### Audio info

```gdscript
player.get_audio_sample_rate()
player.get_audio_channels()
```

### Other helpers

```gdscript
player.get_current_video_path()
player.get_frame_texture()
```

---

## Seek Example

```gdscript
func _on_forward_pressed() -> void:
	var new_pos = player.get_video_position() + 15.0
	player.seek(new_pos)

func _on_backward_pressed() -> void:
	var new_pos = player.get_video_position() - 15.0
	player.seek(new_pos)
```

---

## Playback Speed Example

```gdscript
player.playback_speed = 0.5
player.playback_speed = 1.0
player.playback_speed = 1.5
player.playback_speed = 2.0
```

---

## Timeline Example

```gdscript
extends Control

@onready var mp4_player = $MP4VideoPlayer
@onready var timeline: HSlider = $Controls/TIMELINE
@onready var time_label = $Controls/TIMELABEL
@onready var duration_label = $Controls/DURATIONLABEL

var is_seeking := false

func _ready() -> void:
	timeline.min_value = 0.0
	timeline.max_value = mp4_player.get_video_duration()
	timeline.value = 0.0
	duration_label.text = mp4_player.get_video_duration_text()

func _process(_delta: float) -> void:
	if not is_seeking:
		timeline.value = mp4_player.get_video_position()
		time_label.text = mp4_player.get_video_position_text()

func _on_timeline_drag_started() -> void:
	is_seeking = true
	mp4_player.pause()

func _on_timeline_drag_ended(value_changed: bool) -> void:
	is_seeking = false
	if value_changed:
		mp4_player.seek(timeline.value)
	mp4_player.play()
```

---

## Demo

A demo project is included in the `demo` folder.

It shows how to:

* play and pause video
* seek forward and backward
* use a timeline slider
* show current time and duration
* change playback speed

---

## Notes

* The addon is designed for MP4 playback.
* In the editor, a preview cover image is shown instead of playing the real video.
* Audio buffering is handled automatically by the node.
* For smoother timeline dragging, it is better to seek in controlled intervals or when dragging ends.

---

## Third-Party Library

This addon uses **FFmpeg**.

FFmpeg is licensed separately under the **LGPL**.
See the `thirdparty/ffmpeg` folder for license information.

---

## License

This project is licensed under the MIT License.

