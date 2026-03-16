@tool
extends EditorPlugin

const CUSTOM_NODE_SCRIPT := preload("res://addons/MP4Player/MP4VideoPlayer.gd")
const icon = preload("res://addons/MP4Player/assets/MP4PLAYERICON.png")
func _enter_tree() -> void:
	add_custom_type("MP4VideoPlayer", "Control", CUSTOM_NODE_SCRIPT, icon)

func _exit_tree() -> void:
	remove_custom_type("MP4VideoPlayer")
