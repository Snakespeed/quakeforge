#ifndef __vid_vulkan_h
#define __vid_vulkan_h

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

typedef struct vulkan_ctx_s {
	void        (*load_vulkan) (struct vulkan_ctx_s *ctx);
	void        (*unload_vulkan) (struct vulkan_ctx_s *ctx);

	const char **required_extensions;
	struct vulkan_presentation_s *presentation;
	int (*get_presentation_support) (struct vulkan_ctx_s *ctx,
									 VkPhysicalDevice physicalDevice,
									 uint32_t queueFamilyIndex);
	VkSurfaceKHR (*create_surface) (struct vulkan_ctx_s *ctx);

	struct VulkanInstance_s *vtx;
	VkInstance  instance;
#define EXPORTED_VULKAN_FUNCTION(fname) PFN_##fname fname;
#define GLOBAL_LEVEL_VULKAN_FUNCTION(fname) PFN_##fname fname;
#include "QF/Vulkan/funclist.h"
} vulkan_ctx_t;

#endif//__vid_vulkan_h
