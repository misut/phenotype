#pragma once

#include <optional>

struct GLFWwindow;

namespace native::renderer {

void init(GLFWwindow* window);
void flush();
void shutdown();

std::optional<unsigned int> hit_test(float x, float y, float scroll_y);

} // namespace native::renderer
