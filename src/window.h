#pragma once

#include "utils.h"

/// GLFW ///
struct GLFWwindow;

typedef struct GLFWwindow PapertrailWindow;
typedef void (*PapertrailResizeCallbackFn)(PapertrailWindow *, i32, i32);
typedef void (*PapertrailRefreshCallbackFn)(PapertrailWindow *);

typedef u32 PTRAIL_RESULT; // currently = VK_RESULT

typedef struct PapertrailWindowInitInfo {
    u32 height, width;
    u32 min_height, min_width;
    const char *title;
} PapertrailWindowCreateInfo;

void ptrail_window_free(PapertrailWindow *window);

void ptrail_window_set_resize_callback(PapertrailWindow *window, PapertrailResizeCallbackFn fn_ptr);
void ptrail_window_set_refresh_callback(PapertrailWindow *window, PapertrailRefreshCallbackFn fn_ptr);
// a client can store a pointer to any data in the window, for later use (e.g inside callbacks)
void ptrail_window_set_client_state(PapertrailWindow *window, void *state);

void ptrail_window_get_size(PapertrailWindow *window, u32 *width, u32 *height);
void *ptrail_window_get_client_state(PapertrailWindow *window);

bool ptrail_window_is_open(PapertrailWindow *window);
void ptrail_window_poll_events();
void ptrail_window_wait_events();

extern inline f64 ptrail_get_time();

/// VULKAN ///

#include <vulkan/vulkan_core.h>

PapertrailWindow *ptrail_vk_window_init(PapertrailWindowCreateInfo *info);
const char **ptrail_vk_get_required_instance_exts(u32 *count);
PTRAIL_RESULT ptrail_vk_window_surface_init(VkInstance instance, PapertrailWindow *window, VkSurfaceKHR *surface);
