#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

class TextRenderer
{
public:
    void Init();
    void BeginFrame();
    void SubmitText(const std::string& text,
        const glm::vec2& position,
        float scale,
        const glm::vec4& color);
    void Flush(const glm::mat4& projection,
        const glm::mat4& view);

private:
    struct LineVertex
    {
        glm::vec3 Position;
        glm::vec4 Color;
    };

private:
    std::vector<LineVertex> m_Vertices;

    unsigned int m_VAO = 0;
    unsigned int m_VBO = 0;
};

