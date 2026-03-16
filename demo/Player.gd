extends Control

# Reference to the MP4VideoPlayer node.
@onready var mp4_player = $MP4VideoPlayer

# Timeline slider used to show and change the current video position.
@onready var timeline: HSlider = $Controls/TIMELINE

# Label that shows the current playback time.
@onready var timelabel = $Controls/TIMELABEL

# Label that shows the full video duration.
@onready var durationlabel = $Controls/DURATIONLABEL

# Button used to switch between playback speeds.
@onready var playbackspeed = $Controls/PLAYBACKSPEED

# List of speeds the user can cycle through.
var playbackspeeds = [0.5, 1.0, 1.5, 2.0]

# Default speed index. 1 = 1.0x
var current_speed_index := 1

# Stores the full duration of the loaded video.
var video_duration := 0.0

# True while the user is dragging the timeline.
var is_seeking := false

# Timer used to limit how often seek is applied while dragging.
var seek_preview_timer := 0.0

# Minimum time between preview seeks while dragging.
var seek_preview_interval := 0.15

func _ready() -> void:
	# Read video duration from the player.
	video_duration = mp4_player.get_video_duration()

	# Configure the timeline limits.
	timeline.min_value = 0.0
	timeline.max_value = video_duration
	timeline.value = 0.0

	# Show the total video duration in the UI.
	durationlabel.text = mp4_player.get_video_duration_text()

	# Setup the playback speed button text and current index.
	_init_playback_speed_button()

func _init_playback_speed_button() -> void:
	# Read the current speed already set on the player.
	var current_speed = mp4_player.playback_speed

	var found := false

	# Find the matching speed inside the playbackspeeds list.
	for i in range(playbackspeeds.size()):
		if is_equal_approx(playbackspeeds[i], current_speed):
			current_speed_index = i
			found = true
			break

	# If no match was found, fallback to normal speed (1.0x).
	if not found:
		current_speed_index = 1

	# Apply the selected speed and update button text.
	_apply_playback_speed()

func _apply_playback_speed() -> void:
	# Get the speed value from the current index.
	var speed = playbackspeeds[current_speed_index]

	# Apply it to the video player.
	mp4_player.playback_speed = speed

	# Update button text so the user sees the current speed.
	playbackspeed.text = str(speed) + "x"

func _on_playbackspeed_pressed() -> void:
	# Move to the next speed in the list.
	current_speed_index += 1

	# If we reach the end, go back to the first speed.
	if current_speed_index >= playbackspeeds.size():
		current_speed_index = 0

	# Apply the new speed.
	_apply_playback_speed()

func _on_playpause_toggled(toggled_on: bool) -> void:
	# If the button is toggled on, play the video.
	if toggled_on:
		mp4_player.play()
	else:
		# Otherwise pause the video.
		mp4_player.pause()

func _on_seekright_pressed() -> void:
	# Jump forward 15 seconds from the current position.
	var new_pos = mp4_player.get_video_position() + 15.0
	mp4_player.seek(new_pos)

func _on_seekleft_pressed() -> void:
	# Jump backward 15 seconds from the current position.
	var new_pos = mp4_player.get_video_position() - 15.0
	mp4_player.seek(new_pos)

func _process(delta: float) -> void:
	# While the user is not dragging the slider,
	# keep the UI synced with the real video time.
	if not is_seeking:
		update_time()
	else:
		# While dragging, count time for throttled preview seeking.
		seek_preview_timer += delta

func update_time() -> void:
	# Update the timeline position.
	timeline.value = mp4_player.get_video_position()

	# Update the current time text.
	timelabel.text = mp4_player.get_video_position_text()

func _on_timeline_drag_started() -> void:
	# Mark that the user started dragging.
	is_seeking = true

	# Reset the seek preview timer.
	seek_preview_timer = 0.0

	# Pause playback while dragging for smoother seeking.
	mp4_player.pause()

func _on_timeline_drag_ended(value_changed: bool) -> void:
	# User stopped dragging.
	is_seeking = false

	# Only seek if the slider value actually changed.
	if value_changed:
		mp4_player.seek(timeline.value)

	# Resume playback after dragging ends.
	mp4_player.play()

func _on_timeline_value_changed(value: float) -> void:
	# While dragging, seek only every small interval
	# to avoid too many seek calls and audio stutter.
	if is_seeking and seek_preview_timer >= seek_preview_interval:
		seek_preview_timer = 0.0
		mp4_player.seek(value)
