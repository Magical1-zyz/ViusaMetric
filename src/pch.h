#pragma once

// --- 标准库 (Standard Libraries) ---
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <sstream>
#include <unordered_map>
#include <vector>

// --- OpenGL 基础库 (OpenGL Basics) ---
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// ==========================================
// [修复] 启用 GLM 实验性功能
// 必须写在 include <glm/...> 之前！
// ==========================================
#define GLM_ENABLE_EXPERIMENTAL

// --- 数学库 (Math) ---
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>