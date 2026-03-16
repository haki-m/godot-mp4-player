@tool
extends Control
class_name MP4VideoPlayer

@export_file("*.mp4") var video_path: String = ""
@export var autoplay: bool = true
@export var loop_enabled: bool = false

@onready var tex = preload("res://addons/MP4Player/assets/cover.png")

var _expand_mode: int = TextureRect.EXPAND_IGNORE_SIZE
@export_enum(
	"Keep Size",
	"Ignore Size",
	"Fit Width",
	"Fit Width Proportional",
	"Fit Height",
	"Fit Height Proportional"
) var expand_mode: int = TextureRect.EXPAND_IGNORE_SIZE:
	set(value):
		_expand_mode = value
		_apply_texture_settings()
	get:
		return _expand_mode

var _stretch_mode: int = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
@export_enum(
	"Stretch",
	"Tile",
	"Original Size",
	"Original Size Centered",
	"Fit",
	"Fit Centered",
	"Fill"
) var stretch_mode: int = TextureRect.STRETCH_KEEP_ASPECT_CENTERED:
	set(value):
		_stretch_mode = value
		_apply_texture_settings()
	get:
		return _stretch_mode

var _flip_h: bool = false
@export var flip_h: bool = false:
	set(value):
		_flip_h = value
		_apply_texture_settings()
	get:
		return _flip_h

var _flip_v: bool = false
@export var flip_v: bool = false:
	set(value):
		_flip_v = value
		_apply_texture_settings()
	get:
		return _flip_v

var _playback_speed: float = 1.0
@export_range(0.5, 2.0, 0.25) var playback_speed: float = 1.0:
	set(value):
		_playback_speed = clamp(value, 0.5, 2.0)
		if player != null:
			player.set_playback_speed(_playback_speed)
			if audio_playback != null:
				audio_playback.clear_buffer()
	get:
		return _playback_speed

var player:MP4Player = null
var audio_gen: AudioStreamGenerator = null
var audio_playback = null

@onready var texture_rect: TextureRect = TextureRect.new()
@onready var audio_player: AudioStreamPlayer = AudioStreamPlayer.new()

func _ready() -> void:
	_playback_speed = playback_speed
	_setup_nodes()
	_apply_texture_settings()

	if Engine.is_editor_hint():
		texture_rect.texture = tex
		return

	player = MP4Player.new()
	player.name = "NativeMP4Player"
	add_child(player)

	if video_path != "":
		var ok: bool = open(video_path)
		if ok and autoplay:
			play()

func _setup_nodes() -> void:
	if texture_rect.get_parent() == null:
		texture_rect.name = "TextureRect"
		texture_rect.anchor_left = 0.0
		texture_rect.anchor_top = 0.0
		texture_rect.anchor_right = 1.0
		texture_rect.anchor_bottom = 1.0
		texture_rect.offset_left = 0
		texture_rect.offset_top = 0
		texture_rect.offset_right = 0
		texture_rect.offset_bottom = 0
		add_child(texture_rect)

	if audio_player.get_parent() == null:
		audio_player.name = "AudioStreamPlayer"
		add_child(audio_player)

func _apply_texture_settings() -> void:
	if texture_rect == null:
		return

	texture_rect.expand_mode = _expand_mode
	texture_rect.stretch_mode = _stretch_mode
	texture_rect.flip_h = _flip_h
	texture_rect.flip_v = _flip_v

func open(path: String) -> bool:
	if player == null:
		return false

	player.set_loop_enabled(loop_enabled)

	var ok: bool = player.open_file(path)
	if not ok:
		push_error("Failed to open video: %s" % path)
		return false

	audio_gen = AudioStreamGenerator.new()
	audio_gen.mix_rate = player.get_audio_sample_rate()
	audio_gen.buffer_length = 0.25
	audio_player.stream = audio_gen
	audio_player.play()
	audio_playback = audio_player.get_stream_playback()

	playback_speed = _playback_speed
	video_path = path
	return true

func play() -> void:
	if player != null:
		player.play()
		
		audio_player.stream_paused = false

func pause() -> void:
	if player != null:
		player.pause()
		audio_player.stream_paused = true


func stop() -> void:
	if player != null:
		player.stop()
		if audio_playback != null:
			audio_playback.clear_buffer()

func seek(seconds: float) -> bool:
	if player == null:
		return false

	var ok: bool = player.seek(seconds)
	if ok and audio_playback != null:
		audio_playback.clear_buffer()
	return ok
func is_playing() -> bool:
	return player != null and player.is_playing()

func get_video_position() -> float:
	if player == null:
		return 0.0
	return player.get_position_seconds()

func get_video_duration() -> float:
	if player == null:
		return 0.0
	return player.get_duration_seconds()

func get_video_progress() -> float:
	if player == null:
		return 0.0

	var duration := player.get_duration_seconds()
	if duration <= 0.0:
		return 0.0

	return player.get_position_seconds() / duration

func get_video_width() -> int:
	if player == null:
		return 0
	return player.get_video_width()

func get_video_height() -> int:
	if player == null:
		return 0
	return player.get_video_height()

func get_video_size() -> Vector2i:
	if player == null:
		return Vector2i.ZERO
	return Vector2i(player.get_video_width(), player.get_video_height())

func get_video_fps() -> float:
	if player == null:
		return 0.0
	return player.get_video_fps()

func get_audio_sample_rate() -> int:
	if player == null:
		return 0
	return player.get_audio_sample_rate()

func get_audio_channels() -> int:
	if player == null:
		return 0
	return player.get_audio_channels()

func get_current_video_path() -> String:
	if player == null:
		return ""
	return player.get_current_path()

func get_frame_texture() -> Texture2D:
	if player == null:
		return null
	return player.get_frame_texture()

func has_video() -> bool:
	return player != null and video_path != ""

func is_video_loaded() -> bool:
	if player == null:
		return false
	return player.get_duration_seconds() > 0.0 or player.get_video_width() > 0

func get_remaining_time() -> float:
	if player == null:
		return 0.0
	return max(player.get_duration_seconds() - player.get_position_seconds(), 0.0)
func set_video_position(seconds: float) -> bool:
	return seek(seconds)

func restart() -> void:
	if player != null:
		player.seek(0.0)
		if audio_playback != null:
			audio_playback.clear_buffer()

func play_from_start() -> void:
	restart()
	play()

func toggle_play_pause() -> void:
	if is_playing():
		pause()
	else:
		play()
func get_video_position_text() -> String:
	return _format_time(get_video_position())

func get_video_duration_text() -> String:
	return _format_time(get_video_duration())

func get_remaining_time_text() -> String:
	return _format_time(get_remaining_time())

func _format_time(seconds: float) -> String:
	var total := int(seconds)
	var h := total / 3600
	var m := (total % 3600) / 60
	var s := total % 60

	if h > 0:
		return "%02d:%02d:%02d" % [h, m, s]
	return "%02d:%02d" % [m, s]
	
func _process(delta: float) -> void:
	if Engine.is_editor_hint():
		return

	if player != null and player.update(delta):
		texture_rect.texture = player.get_frame_texture()

	if audio_playback != null and player != null:
		var frames: PackedVector2Array = player.consume_audio_frames()
		if frames.size() > 0:
			var available: int = audio_playback.get_frames_available()
			if available > 0:
				if frames.size() > available:
					frames = frames.slice(0, available)
				audio_playback.push_buffer(frames)
