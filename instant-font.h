#ifndef INSTANT_FONT_H
#define INSTANT_FONT_H

#include <SDL2/SDL.h>

#include <vector>
#include <string>
#include <cstring>

//
// IF for "Instant Font"
//
// (c) xterminal86 14.07.2024
//
class IF
{
  public:
    // -----------------------------------------------------------------------

    ~IF()
    {
      SDL_DestroyTexture(_fontAtlas);
    }

    // -----------------------------------------------------------------------

    static IF& Instance()
    {
      static IF instance;
      return instance;
    };

    // -----------------------------------------------------------------------

    bool Init(SDL_Renderer* renderer)
    {
      if (_initialized)
      {
        SDL_Log("Font manager already initialized!");
        return true;
      }

      _atlasWidth  = _numTilesH * _fontSize;
      _atlasHeight = _numTilesV * _fontSize;

      _rendererRef = renderer;
      _fontAtlas = SDL_CreateTexture(_rendererRef,
                                SDL_PIXELFORMAT_RGBA32,
                                SDL_TEXTUREACCESS_STREAMING,
                                _atlasWidth, _atlasHeight);
      if (_fontAtlas == nullptr)
      {
        SDL_Log("%s", SDL_GetError());
        return false;
      }

      uint8_t* bytes = nullptr;
      int pitch = 0;
      int res = SDL_LockTexture(_fontAtlas, nullptr, (void**)(&bytes), &pitch);
      if (res < 0)
      {
        SDL_Log("%s", SDL_GetError());
        return false;
      }

      auto PutPixel = [this, bytes](size_t x,
                                    size_t y,
                                    const uint8_t (&pixel)[4])
      {
        std::memcpy(&bytes[ (x * _atlasWidth + y) * sizeof(pixel) ],
                    pixel,
                    sizeof(pixel));
      };

      size_t charsDefined = _charMap.size();

      size_t charInd = 0;

      for (size_t x = 0; x < _atlasHeight; x += _fontSize)
      {
        size_t xx = x;

        for (size_t y = 0; y < _atlasWidth; y += _fontSize)
        {
          if (charInd < charsDefined)
          {
            size_t yy = y;

            const auto& glyph = _charMap[charInd];

            for (const auto& line : glyph)
            {
              uint16_t toModify = line;
              for (size_t i = 0; i < _fontSize; i++)
              {
                bool isOne = (toModify & 0x1);
                PutPixel(xx, yy, isOne ? kColorWhite : kColorBlack);
                toModify >>= 1;
                yy++;
              }

              yy = y;
              xx++;
            }

            xx = x;
          }

          charInd++;
        }
      }

      SDL_UnlockTexture(_fontAtlas);


      res = SDL_SetTextureBlendMode(_fontAtlas, SDL_BLENDMODE_BLEND);
      if (res < 0)
      {
        SDL_Log("%s", SDL_GetError());
        return false;
      }

      _initialized = true;

      return true;
    }

    // -------------------------------------------------------------------------

    enum class TextAlignment
    {
      LEFT = 0,
      CENTER,
      RIGHT
    };

    // -------------------------------------------------------------------------

    struct TextParams
    {
      TextAlignment Align = TextAlignment::LEFT;
      uint32_t      Color = 0xFFFFFF;
      double        Scale = 1.0;

      static TextParams Set(uint32_t color = 0xFFFFFF,
                            TextAlignment align = TextAlignment::LEFT,
                            double scale = 1.0)
      {
        static TextParams params;
        params.Color = color;
        params.Align = align;
        params.Scale = scale;
        return params;
      }
    };

    // -------------------------------------------------------------------------

    void Print(int x, int y,
               const std::string& text,
               uint32_t color = 0xFFFFFF,
               TextAlignment align = TextAlignment::LEFT,
               double scaleFactor = 1.0)
    {
      if (not _initialized)
      {
        SDL_Log("Font manager is not initialized!");
        return;
      }

      SaveColor();

      const auto& clr = HTML2RGB(color);

      SDL_SetTextureColorMod(_fontAtlas, clr.r, clr.g, clr.b);

      size_t ln = text.length();

      int xOffset = 0;

      switch (align)
      {
        // --------------------------
        case TextAlignment::LEFT:
          xOffset = 0;
          break;
        // --------------------------
        case TextAlignment::RIGHT:
          xOffset = -ln;
          break;
        // --------------------------
        case TextAlignment::CENTER:
          xOffset = -(ln / 2);
          break;
        // --------------------------
        default:
          break;
        // --------------------------
      }

      for (char c : text)
      {
        size_t charInd = c - 32;

        if (c < 32 or c > 127)
        {
          charInd = _charMap.size() - 1;
        }

        size_t xx = (charInd % _numTilesH);
        size_t yy = (charInd / _numTilesH);

        static SDL_Rect fromAtlas;

        fromAtlas.x = xx * _fontSize;
        fromAtlas.y = yy * _fontSize;

        fromAtlas.w = _fontSize;
        fromAtlas.h = _fontSize;

        int scaled = (int)( (double)(xOffset * _fontSize) * scaleFactor );

        static SDL_Rect dst;
        dst.x = x + scaled;
        dst.y = y;
        dst.w = (int)( (double)_fontSize * scaleFactor );
        dst.h = (int)( (double)_fontSize * scaleFactor );

        SDL_RenderCopy(_rendererRef, _fontAtlas, &fromAtlas, &dst);

        x += (int)( (double)_fontSize * scaleFactor );
      }

      RestoreColor();
    }

    // -------------------------------------------------------------------------

    template <typename ... Args>
    void Printf(int x, int y,
                TextParams params,
                const std::string& formatString,
                Args ... args)
    {
      if (not _initialized)
      {
        SDL_Log("Font manager is not initialized!");
        return;
      }

      size_t size = ::snprintf(nullptr,
                               0,
                               formatString.data(),
                               args ...);
      if (size == 0)
      {
        return;
      }

      static std::string s;
      s.resize(size);

      char* buf = (char*)s.data();
      ::snprintf(buf, size + 1, formatString.data(), args ...);

      Print(x, y, s, params.Color, params.Align, params.Scale);
    }

    // -------------------------------------------------------------------------

    //
    // Draw the whole bitmap font atlas to the screen.
    // Used for debugging purposes during development.
    //
    void ShowFontBitmap()
    {
      static SDL_Rect dst;

      dst.x = 0;
      dst.y = 0;
      dst.w = _atlasWidth * 5;
      dst.h = _atlasHeight * 5;

      SDL_RenderCopy(_rendererRef, _fontAtlas, nullptr, &dst);
    }

    // -------------------------------------------------------------------------

  private:
    IF() = default;

    // -------------------------------------------------------------------------

    const SDL_Color& HTML2RGB(const uint32_t& colorMask)
    {
      _drawColor.r = (colorMask & _maskR) >> 16;
      _drawColor.g = (colorMask & _maskG) >> 8;
      _drawColor.b = (colorMask & _maskB);
      _drawColor.a = 0xFF;

      return _drawColor;
    }

    // -------------------------------------------------------------------------

    void SaveColor()
    {
      SDL_GetTextureColorMod(_fontAtlas,
                             &_oldColor.r,
                             &_oldColor.g,
                             &_oldColor.b);
    }

    // -------------------------------------------------------------------------

    void RestoreColor()
    {
      SDL_SetTextureColorMod(_fontAtlas,
                             _oldColor.r,
                             _oldColor.g,
                             _oldColor.b);
    }

    const uint32_t _maskR = 0x00FF0000;
    const uint32_t _maskG = 0x0000FF00;
    const uint32_t _maskB = 0x000000FF;
    const uint32_t _maskA = 0xFF000000;

    SDL_Color _drawColor;
    SDL_Color _oldColor;

    bool _initialized = false;

    const uint8_t kColorWhite[4] = { 255, 255, 255, 255 };
    const uint8_t kColorBlack[4] = {   0,   0,   0,   0 };

    const uint8_t _numTilesH = 16;
    const uint8_t _numTilesV = 6;

    const uint8_t _fontSize = 9;

    uint16_t _atlasWidth  = 0;
    uint16_t _atlasHeight = 0;

    SDL_Texture*  _fontAtlas   = nullptr;
    SDL_Renderer* _rendererRef = nullptr;

    using GlyphData = std::vector<std::vector<uint16_t>>;
    const GlyphData _charMap =
    {
      { 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 16, 56, 56, 56, 16, 0, 16, 0 },
      { 0, 198, 198, 68, 0, 0, 0, 0, 0 },
      { 0, 108, 108, 254, 108, 254, 108, 108, 0 },
      { 16, 124, 214, 22, 124, 208, 214, 124, 16 },
      { 0, 0, 70, 38, 16, 8, 100, 98, 0 },
      { 56, 108, 108, 56, 440, 236, 198, 230, 444 },
      { 0, 24, 24, 8, 4, 0, 0, 0, 0 },
      { 0, 112, 24, 12, 12, 12, 24, 112, 0 },
      { 0, 28, 48, 96, 96, 96, 48, 28, 0 },
      { 0, 0, 84, 56, 254, 56, 84, 0, 0 },
      { 0, 0, 16, 16, 124, 16, 16, 0, 0 },
      { 0, 0, 0, 0, 0, 48, 48, 16, 8 },
      { 0, 0, 0, 0, 254, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 0, 48, 48, 0 },
      { 0, 384, 192, 96, 48, 24, 12, 6, 0 },
      { 0, 56, 68, 198, 214, 198, 68, 56, 0 },
      { 0, 48, 56, 60, 48, 48, 48, 252, 0 },
      { 0, 124, 198, 96, 48, 24, 204, 254, 0 },
      { 0, 124, 198, 192, 112, 192, 198, 124, 0 },
      { 0, 120, 108, 102, 254, 96, 96, 240, 0 },
      { 0, 126, 6, 126, 192, 192, 198, 124, 0 },
      { 0, 120, 12, 6, 126, 198, 198, 124, 0 },
      { 0, 254, 198, 96, 48, 24, 24, 24, 0 },
      { 0, 124, 198, 198, 124, 198, 198, 124, 0 },
      { 0, 124, 198, 198, 252, 192, 96, 60, 0 },
      { 0, 0, 48, 48, 0, 0, 48, 48, 0 },
      { 0, 0, 48, 48, 0, 48, 48, 24, 0 },
      { 0, 192, 48, 12, 3, 12, 48, 192, 0 },
      { 0, 0, 0, 124, 0, 124, 0, 0, 0 },
      { 0, 3, 12, 48, 192, 48, 12, 3, 0 },
      { 0, 120, 204, 204, 96, 48, 48, 0, 48 },
      { 0, 60, 114, 74, 74, 50, 130, 124, 0 },
      { 0, 16, 56, 108, 198, 254, 198, 198, 0 },
      { 0, 126, 198, 198, 126, 198, 198, 126, 0 },
      { 0, 124, 198, 6, 6, 6, 198, 124, 0 },
      { 0, 126, 204, 204, 204, 204, 204, 126, 0 },
      { 0, 254, 204, 12, 60, 12, 204, 254, 0 },
      { 0, 254, 204, 12, 60, 12, 12, 62, 0 },
      { 0, 124, 198, 6, 6, 230, 198, 252, 0 },
      { 0, 198, 198, 198, 254, 198, 198, 198, 0 },
      { 0, 120, 48, 48, 48, 48, 48, 120, 0 },
      { 0, 240, 96, 96, 96, 102, 102, 60, 0 },
      { 0, 198, 198, 102, 62, 198, 198, 198, 0 },
      { 0, 30, 12, 12, 12, 12, 204, 254, 0 },
      { 0, 198, 238, 254, 214, 198, 198, 198, 0 },
      { 0, 198, 206, 222, 246, 230, 198, 198, 0 },
      { 0, 56, 68, 198, 198, 198, 68, 56, 0 },
      { 0, 126, 204, 204, 124, 12, 12, 30, 0 },
      { 0, 124, 198, 198, 198, 214, 102, 188, 0 },
      { 0, 126, 204, 204, 124, 204, 204, 206, 0 },
      { 0, 60, 102, 14, 60, 112, 102, 60, 0 },
      { 0, 126, 126, 90, 24, 24, 24, 60, 0 },
      { 0, 198, 198, 198, 198, 198, 198, 124, 0 },
      { 0, 198, 198, 198, 198, 108, 56, 16, 0 },
      { 0, 198, 198, 198, 198, 214, 254, 108, 0 },
      { 0, 198, 238, 124, 56, 124, 238, 198, 0 },
      { 0, 102, 102, 102, 60, 24, 24, 60, 0 },
      { 0, 254, 198, 96, 48, 24, 204, 254, 0 },
      { 0, 124, 12, 12, 12, 12, 12, 124, 0 },
      { 0, 3, 6, 12, 24, 48, 96, 192, 0 },
      { 0, 124, 96, 96, 96, 96, 96, 124, 0 },
      { 0, 16, 40, 68, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 0, 0, 0, 511 },
      { 0, 24, 24, 16, 32, 0, 0, 0, 0 },
      { 0, 0, 60, 96, 124, 102, 102, 220, 0 },
      { 0, 14, 12, 60, 108, 204, 204, 124, 0 },
      { 0, 0, 124, 198, 6, 6, 198, 124, 0 },
      { 0, 112, 96, 120, 108, 102, 102, 220, 0 },
      { 0, 0, 124, 198, 254, 6, 198, 124, 0 },
      { 112, 216, 24, 24, 60, 24, 24, 60, 0 },
      { 0, 0, 188, 198, 198, 252, 192, 198, 124 },
      { 0, 14, 12, 108, 220, 204, 204, 206, 0 },
      { 48, 48, 0, 56, 48, 48, 48, 120, 0 },
      { 96, 96, 0, 112, 96, 96, 96, 108, 56 },
      { 0, 14, 204, 108, 60, 60, 108, 206, 0 },
      { 0, 56, 48, 48, 48, 48, 48, 120, 0 },
      { 0, 0, 110, 254, 214, 214, 198, 198, 0 },
      { 0, 0, 110, 220, 204, 204, 204, 204, 0 },
      { 0, 0, 124, 198, 198, 198, 198, 124, 0 },
      { 0, 0, 118, 204, 204, 124, 12, 12, 30 },
      { 0, 0, 220, 102, 102, 124, 96, 96, 240 },
      { 0, 0, 118, 204, 12, 12, 12, 30, 0 },
      { 0, 0, 124, 198, 28, 112, 198, 124, 0 },
      { 0, 16, 24, 126, 24, 24, 216, 112, 0 },
      { 0, 0, 102, 102, 102, 102, 118, 236, 0 },
      { 0, 0, 198, 198, 198, 108, 56, 16, 0 },
      { 0, 0, 198, 198, 198, 214, 254, 108, 0 },
      { 0, 0, 198, 108, 56, 56, 108, 198, 0 },
      { 0, 0, 198, 198, 198, 252, 192, 124, 0 },
      { 0, 0, 254, 102, 48, 24, 204, 254, 0 },
      { 0, 112, 24, 24, 14, 24, 24, 112, 0 },
      { 0, 16, 16, 16, 0, 16, 16, 16, 0 },
      { 0, 28, 48, 48, 224, 48, 48, 28, 0 },
      { 0, 156, 214, 98, 0, 0, 0, 0, 0 },
      { 511, 387, 325, 297, 273, 297, 325, 387, 511 },
    };
};

#endif
