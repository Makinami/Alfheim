#pragma once

#include "Color.h"
#include "Math/Vector.h"
#include "CommandContext.h"
#include <string>
#include <type_traits>

class GraphicsContext;

namespace TextRenderer
{
    // Initialize the text renderer's resources and designate the dimensions of the drawable
    // view space.  These dimensions do not have to match the actual pixel width and height of
    // the viewport.  Instead they create a coordinate space for placing text within the
    // viewport.  For instance, if you specify a ViewWidth of 2.0f, then CursorX = 1.0f marks
    // the middle of the viewport.
    void Initialize(void);
    void Shutdown(void);

    class Font;
}

class TextContext
{
public:
    TextContext(GraphicsContext& CmdContext, float CanvasWidth = 1920.0f, float CancasHeight = 1080.0f);

    GraphicsContext& GetCommandContext() const { return m_Context; }

    void ResetSettings(void);

    void SetFont(const std::wstring& fontName, float TextSize = 0.0f);

    void SetTextSize(float PixelHieight);

    void ResetCursor(float x, float y);
    void SetLeftMargin(float x);
    void SetCursorX(float x);
    void SetCursorY(float y);
    void NewLine(void);
    float GetLeftMargin(void);
    float GetCursorX(void);
    float GetCursorY(void);

    void SetColor(Color color);

    void Begin(bool EnableHDR = false);
    void End(void);

    void DrawString(const std::string& str);
    void DrawString(const std::wstring& str);

    template <typename S, typename ... Args, typename Char = fmt::char_t<S>>
    void DrawFormattedString(const S& format, Args ... args)
    {
        DrawString(fmt::format(format, args...));
    }

private:
    __declspec(align(16)) struct VertexShaderParams
    {
        Math::Vector4 ViewportTransform;
        float NormalizeX, NormalizeY, TextSize;
        float Scale, DstBorder;
        uint32_t SrcBorder;
    };

    __declspec(align(16)) struct PixelShaderParams
    {
        Color TextColor;
        float ShadowOffsetX, ShadowOffsetY;
        float ShadowHardness;    // More than 1 will cause aliasing
        float ShadowOpacity;     // Should make less opaque when making softer
        float HeightRange;
    };

    void SetRenderState(void);

    __declspec(align(16)) struct TextVert
    {
        float X, Y;           // Upper-left glyph position in screen space
        uint16_t U, V, W, H;  // Upper-left glyph UV and the width in texture space
    };

    UINT FillVertexBuffer(TextVert volatile* verts, const char* str, size_t stride, size_t slen);

    GraphicsContext& m_Context;
    const TextRenderer::Font* m_CurrentFont = nullptr;
    VertexShaderParams m_VSParams;
    PixelShaderParams m_PSParams;
    bool m_VSConstantBufferIsStale;    // Tracks when the CB needs updating
    bool m_PSConstantBufferIsStale;    // Tracks when the CB needs updating
    bool m_TextureIsStale;
    bool m_EnableShadow;
    float m_LeftMargin;
    float m_TextPosX;
    float m_TextPosY;
    float m_LineHeight;
    float m_ViewWidth;           // Width of the drawable area
    float m_ViewHeight;          // Height of the drawable area
    float m_ShadowOffsetX;       // Percentage of the font's TextSize should the shadow be offset
    float m_ShadowOffsetY;       // Percentage of the font's TextSize should the shadow be offset
    BOOL m_HDR = FALSE;
};