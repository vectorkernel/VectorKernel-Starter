#pragma once
#include "glad.h"

GLuint CompileShader(GLenum type, const char* source);
GLuint CreateProgram(const char* vsSource, const char* fsSource);

