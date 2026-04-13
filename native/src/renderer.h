#pragma once

struct GLFWwindow;

namespace native::renderer {

void init(GLFWwindow* window);
void flush();
void shutdown();

} // namespace native::renderer
