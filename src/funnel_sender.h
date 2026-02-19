#pragma once

#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/wrapped.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <funnel.h>
#include <funnel-vk.h>

using namespace godot;

class FunnelSender : public Node {
	GDCLASS(FunnelSender, Node)

private:
	struct funnel_stream *stream;
	struct funnel_buffer *buf;
	
	VkCommandPool command_pool;
	VkCommandBuffer command_buffer;

	String sender_name;
	Viewport *target_viewport;

	void start_stream();
	void stop_stream();

	void prepare_buffer();
	void send_texture();

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("get_sender_name"), &FunnelSender::get_sender_name);
        ClassDB::bind_method(D_METHOD("set_sender_name"), &FunnelSender::set_sender_name);
        ADD_PROPERTY(PropertyInfo(Variant::STRING, "sender_name"), "set_sender_name", "get_sender_name");

		ClassDB::bind_method(D_METHOD("get_target_viewport"), &FunnelSender::get_target_viewport);
        ClassDB::bind_method(D_METHOD("set_target_viewport"), &FunnelSender::set_target_viewport);
        ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "target_viewport", PROPERTY_HINT_NODE_TYPE, "Viewport"), "set_target_viewport", "get_target_viewport");
	}
	void _notification(int p_what);

public:
	FunnelSender();
	~FunnelSender();

	Viewport* get_target_viewport();
	void set_target_viewport(Viewport *viewport);

	String get_sender_name();
	void set_sender_name(String name);
};
