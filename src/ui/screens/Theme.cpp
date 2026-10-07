#include "ui/screens/Theme.h"

#include "ui/screens/SystemFonts.h"

#include <imgui_internal.h>  // ImDrawListSharedData (drawList->_Data->TexUvWhitePixel) is only forward-declared in imgui.h

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>

namespace romantasy::ui::theme
{
    namespace
    {
        constexpr float kPi = 3.14159265f;

        bool FileExists(const std::string& path)
        {
            std::FILE* file = nullptr;
            if (fopen_s(&file, path.c_str(), "rb") != 0 || !file) {
                return false;
            }
            std::fclose(file);
            return true;
        }

        ImFont* ResolveFont(ImFont* font)
        {
            return font ? font : ImGui::GetFont();
        }
    }

    bool LoadFonts(ImGuiIO& io, const std::string& fontDir, Fonts& out)
    {
        // `fallback` is the Segoe UI file closest to the bundled font's weight.
        struct Spec
        {
            const char* file;
            ImFont** slot;
            const wchar_t* fallback;
        };
        const Spec specs[] = {
            { "montserrat-latin-900-normal.ttf", &out.ceremonial, L"seguibl.ttf" },
            { "poppins-latin-300-normal.ttf", &out.bodyLight, L"segoeuil.ttf" },
            { "poppins-latin-400-normal.ttf", &out.body, L"segoeui.ttf" },
            { "poppins-latin-500-normal.ttf", &out.bodyMedium, L"seguisb.ttf" },
            { "poppins-latin-600-normal.ttf", &out.bodySemi, L"seguisb.ttf" },
            { "poppins-latin-700-normal.ttf", &out.bodyBold, L"segoeuib.ttf" },
            { "montecarlo-latin-400-normal.ttf", &out.script, L"segoeui.ttf" },
        };

        bool all = true;
        for (const auto& spec : specs) {
            const std::string path = fontDir + "/" + spec.file;
            if (!FileExists(path)) {
                *spec.slot = nullptr;
                all = false;
                continue;
            }
            *spec.slot = io.Fonts->AddFontFromFileTTF(path.c_str(), 0.0f);
            if (!*spec.slot) {
                all = false;
                continue;
            }
            // The bundled font draws everything it has; a letter it lacks comes
            // from Windows' fonts merged behind it (see SystemFonts.h).
            for (const auto* data : FallbackFonts(spec.fallback)) {
                ImFontConfig config;
                config.MergeMode = true;
                config.FontDataOwnedByAtlas = false;  // shared, process-lifetime data
                io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(data->data()), static_cast<int>(data->size()), 0.0f, &config);
            }
        }
        if (io.Fonts->Fonts.Size == 0) {
            io.Fonts->AddFontDefault();
        }
        out.loaded = all;
        return all;
    }

    Canvas Canvas::Fit(ImVec2 displaySize) noexcept
    {
        Canvas canvas;
        canvas.display = displaySize;
        canvas.scale = (std::min)(displaySize.x / Width, displaySize.y / Height);
        canvas.origin = ImVec2(
            (displaySize.x - Width * canvas.scale) * 0.5f,
            (displaySize.y - Height * canvas.scale) * 0.5f);
        return canvas;
    }

    void DrawPanel(ImDrawList* drawList, ImVec2 min, ImVec2 max, float radius, ImU32 fill, ImU32 border, float borderThickness)
    {
        drawList->AddRectFilled(min, max, fill, radius);
        if ((border >> IM_COL32_A_SHIFT) & 0xFF) {
            drawList->AddRect(min, max, border, radius, 0, borderThickness);
        }
    }

    void DrawGlow(ImDrawList* drawList, ImVec2 min, ImVec2 max, float radius, ImU32 color, float spread, int layers)
    {
        const float baseAlpha = static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
        for (int i = layers; i >= 1; --i) {
            const float t = static_cast<float>(i) / static_cast<float>(layers + 1);
            const float expand = spread * t;
            const float alpha = baseAlpha * (1.0f - t) * 0.6f;
            drawList->AddRectFilled(
                ImVec2(min.x - expand, min.y - expand),
                ImVec2(max.x + expand, max.y + expand),
                WithAlpha(color, alpha),
                radius + expand);
        }
    }

    void BuildHeartPath(ImVec2 center, float size, ImVec2* outPoints) noexcept
    {
        // Classic parametric heart: x = 16 sin^3 t, y = 13 cos t - 5 cos 2t - 2 cos 3t - cos 4t.
        // x spans [-16, 16]; scale so the heart is `size` wide, flip y for screen space.
        const float unit = size / 32.0f;
        for (int i = 0; i < HeartPointCount; ++i) {
            const float t = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(HeartPointCount);
            const float s = std::sin(t);
            const float x = 16.0f * s * s * s;
            const float y = 13.0f * std::cos(t) - 5.0f * std::cos(2.0f * t) - 2.0f * std::cos(3.0f * t) - std::cos(4.0f * t);
            outPoints[i] = ImVec2(center.x + x * unit, center.y - y * unit);
        }
    }

    void DrawHeart(ImDrawList* drawList, ImVec2 center, float size, ImU32 fill, bool wounded)
    {
        ImVec2 points[HeartPointCount];
        BuildHeartPath(center, size, points);
        drawList->AddConcavePolyFilled(points, HeartPointCount, fill);

        if (!wounded) {
            return;
        }
        // Crack from the top notch toward the tip, in heart units (y up).
        const float unit = size / 32.0f;
        const ImVec2 crack[] = {
            ImVec2(center.x + 0.0f * unit, center.y - 5.0f * unit),
            ImVec2(center.x - 2.0f * unit, center.y - 0.0f * unit),
            ImVec2(center.x + 1.5f * unit, center.y + 5.0f * unit),
            ImVec2(center.x - 1.0f * unit, center.y + 10.0f * unit),
            ImVec2(center.x + 0.5f * unit, center.y + 14.0f * unit),
        };
        drawList->AddPolyline(crack, 5, Obsidian, ImDrawFlags_None, (std::max)(1.5f, size * 0.06f));
    }

    void DrawText(ImDrawList* drawList, ImFont* font, float size, ImVec2 pos, ImU32 color, const char* text)
    {
        drawList->AddText(ResolveFont(font), size, pos, color, text);
    }

    ImVec2 MeasureText(ImFont* font, float size, const char* text)
    {
        return ResolveFont(font)->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
    }

    void DrawTextAligned(ImDrawList* drawList, ImFont* font, float size, ImVec2 anchor, ImU32 color, const char* text, float alignX)
    {
        const ImVec2 extent = MeasureText(font, size, text);
        DrawText(drawList, font, size, ImVec2(anchor.x - extent.x * alignX, anchor.y), color, text);
    }

    void GlowCircle(ImDrawList* drawList, ImVec2 center, float radius, ImU32 color, float peakAlpha, int segments)
    {
        segments = std::max(segments, 12);
        const ImVec2 uv = drawList->_Data->TexUvWhitePixel;
        const ImU32 outer = WithAlpha(color, 0.0f);
        // Three nested fans (r, 2r/3, r/3) each carrying a third of the peak: a soft, near-quadratic falloff with no rings.
        const float radii[3] = { 1.0f, 2.0f / 3.0f, 1.0f / 3.0f };
        for (float k : radii) {
            const float r = radius * k;
            const ImU32 inner = WithAlpha(color, peakAlpha / 3.0f);
            drawList->PrimReserve(segments * 3, segments + 1);
            const ImDrawIdx centerIdx = static_cast<ImDrawIdx>(drawList->_VtxCurrentIdx);
            drawList->PrimWriteVtx(center, uv, inner);
            for (int i = 0; i < segments; ++i) {
                const float a = 6.28318530f * static_cast<float>(i) / static_cast<float>(segments);
                drawList->PrimWriteVtx({ center.x + std::cos(a) * r, center.y + std::sin(a) * r }, uv, outer);
            }
            for (int i = 0; i < segments; ++i) {
                drawList->PrimWriteIdx(centerIdx);
                drawList->PrimWriteIdx(static_cast<ImDrawIdx>(centerIdx + 1 + i));
                drawList->PrimWriteIdx(static_cast<ImDrawIdx>(centerIdx + 1 + (i + 1) % segments));
            }
        }
    }

    void DrawHeartOutline(ImDrawList* drawList, ImVec2 center, float size, ImU32 color, float thickness)
    {
        ImVec2 points[HeartPointCount];
        BuildHeartPath(center, size, points);
        drawList->AddPolyline(points, HeartPointCount, color, ImDrawFlags_Closed, thickness);
    }
}
