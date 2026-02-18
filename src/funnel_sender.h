#pragma once

#include <godot_cpp/classes/global_constants.hpp>
#include "godot_cpp/classes/wrapped.hpp"
#include "godot_cpp/classes/viewport_texture.hpp"
#include "godot_cpp/classes/node.hpp"
#include "godot_cpp/variant/variant.hpp"

#include <funnel.h>

using namespace godot;

class FunnelSender : public Node {
	GDCLASS(FunnelSender, Node)

private:
	struct funnel_stream *stream;
	struct funnel_buffer *buf;

	String sender_name;
	ViewportTexture *texture;

	void start_stream();
	void stop_stream();

	void prepare_buffer();
	void send_texture();

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("get_sender_name"), &FunnelSender::get_sender_name);
        ClassDB::bind_method(D_METHOD("set_sender_name"), &FunnelSender::set_sender_name);
        ADD_PROPERTY(PropertyInfo(Variant::STRING, "sender_name"), "set_sender_name", "get_sender_name");

		ClassDB::bind_method(D_METHOD("get_viewport_texture"), &FunnelSender::get_viewport_texture);
        ClassDB::bind_method(D_METHOD("set_viewport_texture"), &FunnelSender::set_viewport_texture);
        ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "viewport_texture", PROPERTY_HINT_RESOURCE_TYPE, "ViewportTexture"), "set_viewport_texture", "get_viewport_texture");
	}
	void _notification(int p_what);

public:
	FunnelSender();
	~FunnelSender();

	Ref<ViewportTexture> get_viewport_texture();
	void set_viewport_texture(ViewportTexture *viewport);

	String get_sender_name();
	void set_sender_name(String name);
};
