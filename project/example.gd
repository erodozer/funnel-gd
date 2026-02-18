extends Node


func _ready() -> void:
	var example := FunnelSender.new()
	example.set_sender_name("Funnel Example")
	example.set_viewport(get_viewport())
	add_child(example)
