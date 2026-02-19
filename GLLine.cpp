#include <string>
#include <iostream>
#include "GLLine.h"
#include "createShaderProgram.h"

GLuint GLLine::shaderProgram_ = 0;

GLuint GLLine::GetShaderProgram() {
    if (shaderProgram_ == 0) {
        const std::string vertexSource = R"(
            #version 330 core
            layout(location = 0) in vec3 aPos;
            uniform mat4 projection;
            uniform mat4 view;
            void main() {
                gl_Position = projection * view * vec4(aPos, 1.0);
            }
        )";

        const std::string fragmentSource = R"(
            #version 330 core
            uniform vec4 color;
            out vec4 FragColor;
            void main() {
                FragColor = color;
            }
        )";

        shaderProgram_ = createShaderProgram(vertexSource, fragmentSource);
        if (shaderProgram_ == 0) {
            std::cout << "Failed to create shader program." << std::endl;
            return 0;
        }
    }
    return shaderProgram_;
}

void GLLine::DeleteShaderProgram() {
    if (shaderProgram_ != 0) {
        glDeleteProgram(shaderProgram_);
        shaderProgram_ = 0;
    }
}
