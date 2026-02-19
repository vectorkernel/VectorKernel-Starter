#include "HersheyTextBuilder.h"
#include <algorithm>
#include <string>
#include <vector>
#include "hersheyfont.h"

namespace
{
    // Hershey font metrics are in "Hershey units" (glyph verts use integer coords).
    // These constants match the existing demo behavior.
    constexpr float kLineHeight = 30.0f;
    constexpr float kAscent = 21.0f;
    constexpr float kTopPadding = 24.0f; // moves text downward inside the box

    static float GlyphAdvance(hershey_font* font, unsigned char c, float scale)
    {
        if (!font) return 0.0f;
        hershey_glyph* g = hershey_font_glyph(font, (unsigned int)c);
        if (!g) return 0.0f;
        return (float)g->width * scale;
    }

    static float StringWidth(hershey_font* font, const std::string& s, float scale)
    {
        float w = 0.0f;
        for (unsigned char c : s)
        {
            if (c < 32 || c > 126) continue;
            w += GlyphAdvance(font, c, scale);
        }
        return w;
    }

    static std::vector<std::string> SplitExplicitLines(const std::string& text)
    {
        std::vector<std::string> lines;
        std::string cur;
        for (char c : text)
        {
            if (c == '\n') { lines.push_back(cur); cur.clear(); }
            else cur.push_back(c);
        }
        lines.push_back(cur);
        return lines;
    }

    static std::vector<std::string> WrapTextWords(
        const std::string& text,
        hershey_font* font,
        float scale,
        float maxWidthWorld)
    {
        std::vector<std::string> lines;
        std::string current;
        float currentW = 0.0f;

        const float spaceW = GlyphAdvance(font, (unsigned char)' ', scale);

        auto pushLine = [&]()
            {
                lines.push_back(current);
                current.clear();
                currentW = 0.0f;
            };

        std::size_t i = 0;
        while (i < text.size())
        {
            if (text[i] == '\n')
            {
                pushLine();
                ++i;
                continue;
            }

            while (i < text.size() && text[i] == ' ')
                ++i;

            if (i >= text.size())
                break;

            std::size_t start = i;
            while (i < text.size() && text[i] != ' ' && text[i] != '\n')
                ++i;

            std::string word = text.substr(start, i - start);
            float wordW = StringWidth(font, word, scale);

            if (current.empty())
            {
                current = word;
                currentW = wordW;
            }
            else
            {
                if (currentW + spaceW + wordW <= maxWidthWorld)
                {
                    current += " " + word;
                    currentW += spaceW + wordW;
                }
                else
                {
                    pushLine();
                    current = word;
                    currentW = wordW;
                }
            }
        }

        if (!current.empty() || lines.empty())
            lines.push_back(current);

        return lines;
    }

    static void EmitGlyphLines(
        const hershey_glyph* g,
        float scale,
        const glm::vec3& penOrigin,
        const glm::vec4& color,
        float strokeWidth,
        std::vector<LineEntity>& out)
    {
        if (!g) return;

        for (hershey_path* p = g->paths; p; p = p->next)
        {
            if (p->nverts < 2) continue;

            for (unsigned int i = 0; i + 1 < p->nverts; ++i)
            {
                glm::vec3 a(
                    penOrigin.x + (float)p->verts[i].x * scale,
                    penOrigin.y + (float)p->verts[i].y * scale,
                    penOrigin.z);

                glm::vec3 b(
                    penOrigin.x + (float)p->verts[i + 1].x * scale,
                    penOrigin.y + (float)p->verts[i + 1].y * scale,
                    penOrigin.z);

                LineEntity e;
                e.start = a;
                e.end = b;
                e.color = color;
                e.width = strokeWidth;
                out.push_back(e);
            }
        }
    }
}

namespace HersheyTextBuilder
{
    float MeasureTextWidth(const std::string& s, hershey_font* font, float scale)
    {
        return StringWidth(font, s, scale);
    }

    std::vector<std::string> BuildTextLines(const TextEntity& text)
    {
        if (text.wordWrapEnabled)
            return WrapTextWords(text.text, text.font, text.scale, text.boxWidth);
        return SplitExplicitLines(text.text);
    }

    void BuildLines(const TextEntity& text, std::vector<LineEntity>& outLines)
    {
        if (!text.font) return;

        const float scale = text.scale;

        const float topY = text.position.y + text.boxHeight;
        float yTop = topY - (kTopPadding * scale);

        std::vector<std::string> lines = BuildTextLines(text);

        for (const std::string& line : lines)
        {
            const float lineW = StringWidth(text.font, line, scale);

            float x = text.position.x;
            if (text.hAlign == TextHAlign::Right)
                x += std::max(0.0f, text.boxWidth - lineW);
            else if (text.hAlign == TextHAlign::Center)
                x += std::max(0.0f, (text.boxWidth - lineW) * 0.5f);

            const float baselineY = yTop - (kAscent * scale);

            for (unsigned char c : line)
            {
                if (c < 32 || c > 126)
                    continue;

                hershey_glyph* g = hershey_font_glyph(text.font, (unsigned int)c);
                if (g)
                {
                    EmitGlyphLines(
                        g,
                        scale,
                        glm::vec3(x, baselineY, text.position.z),
                        text.color,
                        text.strokeWidth,
                        outLines);
                }

                x += GlyphAdvance(text.font, c, scale);
            }

            yTop -= (kLineHeight * scale);
        }
    }
}

