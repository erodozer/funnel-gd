#include "funnel_sender.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/classes/viewport.hpp>

#include <funnel.h>
#include <funnel-vk.h>

#include "funnel_gd.h"

FunnelSender::FunnelSender()
	: sender_name("")
	, target_viewport(nullptr)
	, stream(nullptr)
	, buf(nullptr) {

}

FunnelSender::~FunnelSender() {
	this->stop_stream();
}

void FunnelSender::_notification(int p_what) {
    if (p_what == NOTIFICATION_READY && !Engine::get_singleton()->is_editor_hint()) {
		auto _prepare = callable_mp(this, &FunnelSender::prepare_buffer);
		
		RenderingServer::get_singleton()->connect(
			"frame_pre_draw",
			_prepare
		);

		auto _update = callable_mp(this, &FunnelSender::send_texture);
		RenderingServer::get_singleton()->connect(
			"frame_post_draw",
			_update
		);
    }
}

void FunnelSender::stop_stream() {
	if (this->stream == nullptr) {
		return;
	}

	UtilityFunctions::print("[libfunnel] stream exists, stopping");

	funnel_stream_stop(this->stream);
	funnel_stream_destroy(this->stream);

	this->stream = nullptr;
	this->buf = nullptr;
}

void FunnelSender::start_stream() {
	if (this->target_viewport == nullptr || this->sender_name.is_empty()) {
		return;
	}

	// stop stream if it's currently running
	this->stop_stream();

	ERR_FAIL_COND_MSG(ctx == nullptr, "[libfunnel] ctx is not properly initialized");

	struct funnel_stream *stream;
	RID placeholder;

	RenderingServer *rs = RenderingServer::get_singleton();
	RenderingDevice *rd = rs->get_rendering_device();
	VkInstance instance = (VkInstance) rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_TOPMOST_OBJECT, placeholder, 0);
	VkPhysicalDevice phy_device = (VkPhysicalDevice) rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_PHYSICAL_DEVICE, placeholder, 0);
	VkDevice device = (VkDevice) rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_LOGICAL_DEVICE, placeholder, 0);

	ERR_FAIL_COND_MSG(instance == VK_NULL_HANDLE, "[libfunnel] vulkan instance not ready");
	ERR_FAIL_COND_MSG(phy_device == VK_NULL_HANDLE, "[libfunnel] vulkan instance not ready");
	ERR_FAIL_COND_MSG(device == VK_NULL_HANDLE, "[libfunnel] vulkan instance not ready");

	int ret;
	ret = funnel_stream_create(ctx, (const char*)this->sender_name.ptr(), &stream);
	ERR_FAIL_COND_MSG(ret != 0, "[libfunnel] unable to create stream");

	ret = funnel_stream_init_vulkan(stream, instance, phy_device, device);
	ERR_FAIL_COND_MSG(ret == -EOPNOTSUPP, "[libfunnel] unable to init from vulkan (missing extensions)");
	ERR_FAIL_COND_MSG(ret == -EPROTONOSUPPORT, "[libfunnel] unable to init from vulkan (GPU driver not supported / Pipewire version too old)");
	ERR_FAIL_COND_MSG(ret == -ENODEV, "[libfunnel] unable to init from vulkan (could not locate DRM render mode)");
	
	funnel_stream_set_mode(stream, FUNNEL_ASYNC);
    
	funnel_stream_vk_add_format(stream, VK_FORMAT_R8G8B8A8_SRGB, true, VK_FORMAT_FEATURE_BLIT_DST_BIT);
	funnel_stream_vk_add_format(stream, VK_FORMAT_B8G8R8A8_SRGB, true, VK_FORMAT_FEATURE_BLIT_DST_BIT);
	funnel_stream_vk_add_format(stream, VK_FORMAT_R8G8B8A8_SRGB, false, VK_FORMAT_FEATURE_BLIT_DST_BIT);
	funnel_stream_vk_add_format(stream, VK_FORMAT_B8G8R8A8_SRGB, false, VK_FORMAT_FEATURE_BLIT_DST_BIT);

	Vector2i size = this->target_viewport->get_visible_rect().size;
	ERR_FAIL_COND_MSG(size.x == 0 || size.y == 0, "[libfunnel] viewport dimensions must be larger than (0,0)");
    funnel_stream_set_size(stream, size.x, size.y);
	
	ret = funnel_stream_start(stream);
	ERR_FAIL_COND_MSG(ret != 0, "[libfunnel] unable to start stream: " + this->sender_name);

	this->stream = stream;
	UtilityFunctions::print("[libfunnel] stream started (id: ", this->sender_name, ")");
}

void FunnelSender::set_sender_name(String name) {
	this->sender_name = name;

	this->start_stream();
}

void FunnelSender::set_target_viewport(Viewport *viewport) {
	this->target_viewport = viewport;

	this->start_stream();
}

void FunnelSender::prepare_buffer() {
	if (this->stream == nullptr) {
		return;
	}

	struct funnel_buffer *buf;

	int ret = funnel_stream_dequeue(this->stream, &buf);
	if (ret < 1) {
		// no buffer available
		return;
	}
	
	uint32_t width, height;
	uint32_t new_width = this->target_viewport->get_visible_rect().size.x;
	uint32_t new_height = this->target_viewport->get_visible_rect().size.y;

	funnel_buffer_get_size(buf, &width, &height);

	if ((width != new_width || height != new_height) && (new_width > 0 && new_height > 0)) {
		UtilityFunctions::print("[libfunnel] updating size (%d,%d)", new_width, new_height);
		funnel_stream_set_size(stream, new_width, new_height);
        funnel_stream_configure(stream);
		return;
	}

	RenderingServer *rs = RenderingServer::get_singleton();
	RenderingDevice *rd = rs->get_rendering_device();
	RID rid = this->target_viewport->get_viewport_rid();
	VkImage image = (VkImage) rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_TEXTURE, rid, 0);
	ERR_FAIL_COND_MSG(image == VK_NULL_HANDLE, "[libfunnel] vkimage not available");

	ret = funnel_buffer_get_vk_image(buf, &image);
	ERR_FAIL_COND_MSG(ret != 0, "[libfunnel] unable to bind image to buffer");

	this->buf = buf;
}

void FunnelSender::send_texture() {
	if (this->buf == nullptr || this->stream == nullptr) {
		return;
	}

	funnel_stream_enqueue(this->stream, this->buf);

	this->buf == nullptr;
}

Viewport* FunnelSender::get_target_viewport() {
	return this->target_viewport;
}

String FunnelSender::get_sender_name() {
	return this->sender_name;
}
