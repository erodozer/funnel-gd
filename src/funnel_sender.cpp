#include "funnel_sender.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/classes/viewport.hpp>

#include "funnel_gd.h"

FunnelSender::FunnelSender()
	: sender_name("")
	, target_viewport(nullptr)
	, stream(nullptr)
	, buf(nullptr) {

}

FunnelSender::~FunnelSender() {
	this->stop_stream();

	if (this->command_buffer != nullptr) {
		RID placeholder;

		RenderingServer *rs = RenderingServer::get_singleton();
		RenderingDevice *rd = rs->get_rendering_device();
		VkDevice device = (VkDevice) rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_LOGICAL_DEVICE, placeholder, 0);

		vkFreeCommandBuffers(device, command_pool, 1, &this->command_buffer);
	}
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

	UtilityFunctions::print("[libfunnel] stopping stream");

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

	RID placeholder;

	RenderingServer *rs = RenderingServer::get_singleton();
	RenderingDevice *rd = rs->get_rendering_device();
	VkInstance instance = (VkInstance) rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_TOPMOST_OBJECT, placeholder, 0);
	VkPhysicalDevice phy_device = (VkPhysicalDevice) rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_PHYSICAL_DEVICE, placeholder, 0);
	VkDevice device = (VkDevice) rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_LOGICAL_DEVICE, placeholder, 0);

	ERR_FAIL_COND_MSG(instance == VK_NULL_HANDLE, "[libfunnel/vulkan] instance not ready");
	ERR_FAIL_COND_MSG(phy_device == VK_NULL_HANDLE, "[libfunnel/vulkan] instance not ready");
	ERR_FAIL_COND_MSG(device == VK_NULL_HANDLE, "[libfunnel/vulkan] instance not ready");

	if (this->command_pool == nullptr) {
		uint32_t queue_family_index = rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_QUEUE_FAMILY, placeholder, 0);

		VkCommandPoolCreateInfo create_info;
        create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        create_info.queueFamilyIndex = queue_family_index;
        create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		VkResult result = vkCreateCommandPool(device, &create_info, NULL, &this->command_pool);
		ERR_FAIL_COND_MSG(result != VK_SUCCESS, "[libfunnel/vulkan] unable to allocate command pool");
	}

	if (this->command_buffer == nullptr) {
		VkCommandBufferAllocateInfo alloc_info;
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.commandPool = this->command_pool;
		alloc_info.commandBufferCount = 1;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		VkResult result = vkAllocateCommandBuffers(device, &alloc_info, &this->command_buffer);
		ERR_FAIL_COND_MSG(result != VK_SUCCESS, "[libfunnel/vulkan] unable to allocate command buffer");
	}

	struct funnel_stream *stream;

	int ret;
	ret = funnel_stream_create(ctx, (const char*)this->sender_name.ptrw(), &stream);
	ERR_FAIL_COND_MSG(ret != 0, "[libfunnel] unable to create stream");

	ret = funnel_stream_init_vulkan(stream, instance, phy_device, device);
	ERR_FAIL_COND_MSG(ret == -EOPNOTSUPP, "[libfunnel/vulkan] unable to init from vulkan (missing extensions)");
	ERR_FAIL_COND_MSG(ret == -EPROTONOSUPPORT, "[libfunnel/vulkan] unable to init from vulkan (GPU driver not supported / Pipewire version too old)");
	ERR_FAIL_COND_MSG(ret == -ENODEV, "[libfunnel/vulkan] unable to init from vulkan (could not locate DRM render mode)");
	
	funnel_stream_set_mode(stream, FUNNEL_ASYNC);
	funnel_stream_set_rate(stream, FUNNEL_RATE_VARIABLE, FUNNEL_FRACTION(1, 1), FUNNEL_FRACTION(1000, 1));
    funnel_stream_vk_set_usage(stream, VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	
	funnel_stream_vk_add_format(stream, VK_FORMAT_R8G8B8A8_SRGB, true, VK_FORMAT_FEATURE_BLIT_DST_BIT);
	funnel_stream_vk_add_format(stream, VK_FORMAT_B8G8R8A8_SRGB, true, VK_FORMAT_FEATURE_BLIT_DST_BIT);
	funnel_stream_vk_add_format(stream, VK_FORMAT_R8G8B8A8_SRGB, false, VK_FORMAT_FEATURE_BLIT_DST_BIT);
	funnel_stream_vk_add_format(stream, VK_FORMAT_B8G8R8A8_SRGB, false, VK_FORMAT_FEATURE_BLIT_DST_BIT);

	Vector2i size = this->target_viewport->get_visible_rect().size;
	ERR_FAIL_COND_MSG(size.x == 0 || size.y == 0, "[libfunnel] viewport dimensions must be larger than (0,0)");
    funnel_stream_set_size(stream, size.x, size.y);
	funnel_stream_configure(stream);
	
	ret = funnel_stream_start(stream);
	ERR_FAIL_COND_MSG(ret != 0, "[libfunnel] unable to start stream, id:" + this->sender_name);

	this->stream = stream;
	UtilityFunctions::print("[libfunnel] stream started, id: ", this->sender_name);
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
	ERR_FAIL_COND_MSG(ret == -EINVAL, "[libfunnel] invalid stream state");
	ERR_FAIL_COND_MSG(ret == -EBUSY, "[libfunnel] attempted to dequeue multiple buffers");
	ERR_FAIL_COND_MSG(ret == -EIO, "[libfunnel] invalid pipewire context");
	ERR_FAIL_COND_MSG(ret == -ESHUTDOWN, "[libfunnel] stream is stopped");

	if (!buf) {
		// no buffer
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
		funnel_stream_return(stream, buf);
		return;
	}

	this->buf = buf;
}

int copy_image(VkCommandBuffer command_buffer, VkImage source, VkImage image, int32_t width, int32_t height) {
	ERR_FAIL_COND_V_MSG(command_buffer == nullptr, -EINVAL, "[libfunnel/vulkan] command buffer is not initialized");

	VkResult ret;
	VkCommandBufferBeginInfo begin_info;
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	ret = vkBeginCommandBuffer(command_buffer, &begin_info);
	ERR_FAIL_COND_V_MSG(ret != VK_SUCCESS, -EINVAL, "[libfunnel/vulkan] command buffer is not initialized");

	// copy godot texture to pw buffer
	VkImageBlit region = {
		.srcSubresource =
			{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = 0,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		.srcOffsets = {{0, 0, 0}, {width, height, 1}},
		.dstSubresource =
			{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = 0,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		.dstOffsets = {{0, 0, 0}, {width, height, 1}},
	};

	vkCmdBlitImage(command_buffer, source,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region,
				VK_FILTER_NEAREST);

	ret = vkEndCommandBuffer(command_buffer);
	ERR_FAIL_COND_V_MSG(ret != VK_SUCCESS, -EINVAL, "[libfunnel/vulkan] command buffer could not close");

	return 0;
}

void FunnelSender::send_texture() {
	if (this->buf == nullptr || this->stream == nullptr) {
		return;
	}

	uint32_t bwidth, bheight;
	funnel_buffer_get_size(this->buf, &bwidth, &bheight);

	RenderingServer *rs = RenderingServer::get_singleton();
	RenderingDevice *rd = rs->get_rendering_device();
	RID rid = rs->viewport_get_texture(this->target_viewport->get_viewport_rid());
	VkImage source = (VkImage) rs->texture_get_native_handle(rid, true);
	if (source == VK_NULL_HANDLE) {
		goto cleanup;
	}
	
	int ret;
	VkImage image;
	ret = funnel_buffer_get_vk_image(this->buf, &image);
	if (ret == -EINVAL) {
		goto cleanup;
	}
	
	ret = copy_image(this->command_buffer, source, image, bwidth, bheight);
	if (ret == 0) {
		ret = funnel_stream_enqueue(this->stream, this->buf);
		goto cont;
	}

cleanup:
	funnel_stream_return(this->stream, this->buf);
cont:
	this->buf = nullptr;
	
	ERR_FAIL_COND_MSG(ret == -EINVAL, "[libfunnel] invalid argument or stream is in invalid state");
}

Viewport* FunnelSender::get_target_viewport() {
	return this->target_viewport;
}

String FunnelSender::get_sender_name() {
	return this->sender_name;
}
