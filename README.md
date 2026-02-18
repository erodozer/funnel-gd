# funnel-gd

[Libfunnel](https://github.com/hoshinolina/libfunnel) bindings for Godot

## Building

Initialize the submodules via `git submodule update --init`

Follow steps to first [build libfunnel](https://github.com/hoshinolina/libfunnel?tab=readme-ov-file#building)

build the project with

```shell
scons
```

**Important:** Godot is currently missing support for certain required vulkan extensions for sharing Vulkan dma buf.  The example project depends on a build of Godot patched with [PR 114940](https://github.com/godotengine/godot/pull/114940) to be able to send textures.

For your project to include the required extensions, within the `project.godot` file add

```
[rendering]
rendering_device/vulkan/additional_device_extensions=PackedStringArray("VK_EXT_external_memory_dma_buf","VK_KHR_external_semaphore_fd")
```

## Usage

Only available for Godot projects using Forward+ Renderer, as the integration is currently restricted to Vulkan.

Add a `FunnelSender` node to any scene, set it's name, and assign a ViewportTexture to it to send only a specific rendered context.

If you'd like to send the contents of the viewport the node is within, such as sending the whole window's contents, you can acquire and assign the viewport texture from a script

```
extends Node

@onready var funnel_sender: FunnelSender = $FunnelSender

func _ready():
	funnel_sender.set_target_viewport(get_viewport())
```
