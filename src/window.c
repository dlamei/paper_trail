#include "window.h"

#include <vulkan/vulkan.h>
#include <glfw/glfw3.h>

// PapertrailWindow * == GLFWWindow *

PapertrailWindow *ptrail_vk_window_init(PapertrailWindowInitInfo *info) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    ASSERT(glfwVulkanSupported());
    GLFWwindow *glfw_window = glfwCreateWindow(1000, 800, "Papertrail", NULL, NULL);
    glfwSetWindowSizeLimits(glfw_window, 1, 1, GLFW_DONT_CARE, GLFW_DONT_CARE);
    return glfw_window;
}

void ptrail_window_free(PapertrailWindow *window) {
    glfwDestroyWindow(window);
    glfwTerminate();
}

void ptrail_window_set_resize_callback(PapertrailWindow *window, PapertrailResizeCallbackFn fn_ptr) {
    glfwSetFramebufferSizeCallback(window, fn_ptr);
}

void ptrail_window_set_refresh_callback(PapertrailWindow *window, PapertrailRefreshCallbackFn fn_ptr) {
    glfwSetWindowRefreshCallback(window, fn_ptr);
}

void ptrail_window_set_client_state(PapertrailWindow *window, void *state) {
    glfwSetWindowUserPointer(window, state);
}

void ptrail_window_get_size(PapertrailWindow *window, u32 *width, u32 *height) {
    i32 w, h;
    glfwGetWindowSize(window, &w, &h);
    ASSERT_MSG(w >= 0 && h >= 0, "negative window size");
    *width = (u32)w;
    *height = (u32)h;
}

void *ptrail_window_get_client_state(PapertrailWindow *window) {
   return glfwGetWindowUserPointer(window);
}

bool ptrail_window_is_open(PapertrailWindow *window) {
    return !glfwWindowShouldClose(window);
}

void ptrail_window_poll_events() {
    glfwPollEvents();
}

void ptrail_window_wait_events() {
    glfwWaitEvents();
}

const char **ptrail_vk_get_required_instance_exts(u32 *count) {
    return glfwGetRequiredInstanceExtensions(count);
}

PTRAIL_RESULT ptrail_vk_window_surface_init(VkInstance instance, PapertrailWindow *window, VkSurfaceKHR *surface) {
    return glfwCreateWindowSurface(instance, window, NULL, surface);
}