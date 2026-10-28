#pragma once

// OpenGL functions come from different headers on macOS and other platforms.
// Include this project header instead of including an OpenGL header directly.
#if defined(__APPLE__)
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif
#include <OpenGL/gl3.h>
#else
#include <glad/gl.h>
#endif
