extends Node

func _ready() -> void:
	var example := FunnelSender.new()
	example.set_sender_name("Funnel Example")
	example.set_viewport_texture(get_viewport().get_texture())
	add_child(example)
