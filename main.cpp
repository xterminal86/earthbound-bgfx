#include <filesystem>
#include <vector>
#include <memory>
#include <chrono>
#include <set>
#include <random>

#include "instant-font.h"
#include "nrs.h"

// =============================================================================

std::mt19937_64 RNG;

double Random01()
{
  return (double)(RNG() % 10001) / 10000.0;
}

// -----------------------------------------------------------------------------

int RandomRange(int min, int max)
{
  if (min == max)
  {
    return min;
  }

  int trueMin = std::min(min, max);
  int trueMax = std::max(min, max);

  int d = std::abs(trueMax - trueMin);

  int random = RNG() % d;

  return trueMin + random;
}

// =============================================================================

const double PIOVER180 = 0.017453292519943295;

using Clock = std::chrono::steady_clock;
using ns    = std::chrono::nanoseconds;

SDL_Renderer* Renderer = nullptr;

SDL_Texture* BgRenderTexture = nullptr;
SDL_Texture* Framebuffer     = nullptr;

const uint16_t kBgWidth  = 256;
const uint16_t kBgHeight = 256;

const uint16_t kBgW2 = kBgWidth * 2;
const uint16_t kBgH2 = kBgHeight * 2;

const uint16_t kScreenWidth  = 800;
const uint16_t kScreenHeight = 600;

const uint16_t kScreenWH = kScreenWidth / 2;
const uint16_t kScreenHH = kScreenHeight / 2;

const uint16_t kBgDisplayX = kScreenWidth - kBgW2 - 16;
const uint16_t kBgDisplayY = 16;

uint32_t FPS = 0;

size_t CurrentBackgroundIndex = 0;

bool IsRunning = true;
bool ShowHelp = false;

NRS DataLoader;

double AngleX = 0.0;
double AngleY = 0.0;

double DeltaTime = 0.0;

// -----------------------------------------------------------------------------
// Modifiable params

double AngleIncreaseX = 0.05;
double AngleIncreaseY = 0.05;

double ScanlineFactorDeltaX = 0.025;
double ScanlineFactorDeltaY = 0.025;

enum class Parameters
{
  SCROLL_SPEED_H = 0,
  SCROLL_SPEED_V,
  ANGLE_INC_X,
  ANGLE_INC_Y,
  SCANLINE_DELTA_X,
  SCANLINE_DELTA_Y,
  SCANLINE_FACTOR_X,
  SCANLINE_FACTOR_Y,
  LAST_ELEMENT
};

size_t CurrentParameterIndex = 0;

// -----------------------------------------------------------------------------

struct BgImage
{
  SDL_Surface* OriginalImage = nullptr;
  SDL_Surface* ImageToDraw   = nullptr;

  SDL_Color Pixels[kBgHeight][kBgWidth]{};
  uint32_t  PixelsByPaletteIndex[kBgHeight][kBgWidth]{};

  int ScrollSpeedH = 0;
  int ScrollSpeedV = 0;

  size_t ScrollPosX = 0;
  size_t ScrollPosY = 0;

  int ScanlineOffsetX = 0;
  int ScanlineOffsetY = 0;

  uint32_t PaletteIndexOffset = 0;

  uint32_t PaletteCycleRate = 0;

  double PaletteCycleDeltaTime = 0.0;

  double ScanlineFactorX = 0.0;
  double ScanlineFactorY = 0.0;

  bool PingPongCycling = false;

  bool PPHitMin = true;
  bool PPHitMax = false;

  std::vector<SDL_Color> PaletteColorByIndex;

  std::string Fname;

  // ---------------------------------------------------------------------------

  void ResetParams()
  {
    ScrollSpeedH = 0;
    ScrollSpeedV = 0;

    ScrollPosX = 0;
    ScrollPosY = 0;

    ScanlineOffsetX = 0;
    ScanlineOffsetY = 0;

    PaletteIndexOffset = 0;

    ScanlineFactorX = 0.0;
    ScanlineFactorY = 0.0;

    AngleIncreaseX = 0.05;
    AngleIncreaseY = 0.05;

    ScanlineFactorDeltaX = 0.025;
    ScanlineFactorDeltaY = 0.025;

    PPHitMin = true;
    PPHitMax = false;
  }

  // ---------------------------------------------------------------------------

  void RandomizeParams()
  {
    ScrollSpeedH = ::RandomRange(-5, 5);
    ScrollSpeedV = ::RandomRange(-5, 5);

    ScrollPosX = 0;
    ScrollPosY = 0;

    ScanlineOffsetX = 0;
    ScanlineOffsetY = 0;

    PaletteIndexOffset = 0;

    ScanlineFactorX = ::Random01() * 10.0;
    ScanlineFactorY = ::Random01() * 10.0;

    AngleIncreaseX = ::Random01();
    AngleIncreaseY = ::Random01();

    ScanlineFactorDeltaX = ::Random01();
    ScanlineFactorDeltaY = ::Random01();

    PPHitMin = true;
    PPHitMax = false;
  }

  // ---------------------------------------------------------------------------

  std::string GetColorDataString()
  {
    std::set<std::string> allColors;

    for (uint16_t y = 0; y < kBgHeight; y++)
    {
      for (uint16_t x = 0; x < kBgWidth; x++)
      {
        SDL_Color& c = Pixels[y][x];

        std::string key = std::to_string(c.r) +
                          "/" +
                          std::to_string(c.g) +
                          "/" +
                          std::to_string(c.b);
        allColors.insert(key);
      }
    }

    std::stringstream ss;

    ss << "\n";

    size_t ind = 1;

    for (auto& entry : allColors)
    {
      ss << ind << " : " << entry << "\n";
      ind++;
    }

    return ss.str();
  }

  // ---------------------------------------------------------------------------

  std::string ToString()
  {
    std::stringstream ss;

    ss << "------ [PIXELS] ------\n";

    for (uint16_t y = 0; y < kBgHeight; y++)
    {
      for (uint16_t x = 0; x < kBgWidth; x++)
      {
        ss << "["
           << (uint16_t)Pixels[y][x].r
           << ";"
           << (uint16_t)Pixels[y][x].g
           << ";"
           << (uint16_t)Pixels[y][x].b
           << "]";
      }

      ss << "\n";
    }

    ss << "------ [PALETTE] ------\n";

    for (uint16_t y = 0; y < kBgHeight; y++)
    {
      for (uint16_t x = 0; x < kBgWidth; x++)
      {
        ss << "[" << PixelsByPaletteIndex[y][x] << "]";
      }

      ss << "\n";
    }

    return ss.str();
  }

  // ---------------------------------------------------------------------------

  void ConstructPaletteMap()
  {
    for (uint16_t y = 0; y < kBgHeight; y++)
    {
      for (uint16_t x = 0; x < kBgWidth; x++)
      {
        const SDL_Color& pixel = Pixels[y][x];

        uint32_t paletteEntry = PaletteColorByIndex.size();

        for (size_t i = 0; i < PaletteColorByIndex.size(); i++)
        {
          const SDL_Color& c = PaletteColorByIndex[i];

          if (pixel.r == c.r and pixel.g == c.g and pixel.b == c.b)
          {
            paletteEntry = i;
            break;
          }
        }

        PixelsByPaletteIndex[y][x] = paletteEntry;
      }
    }
  }

  // ---------------------------------------------------------------------------

  void CyclePalette()
  {
    if (PingPongCycling)
    {
      if (PPHitMin and not PPHitMax)
      {
        PaletteIndexOffset++;
      }
      else if (PPHitMax and not PPHitMin)
      {
        PaletteIndexOffset--;
      }

      if (PaletteIndexOffset == (PaletteColorByIndex.size() - 1))
      {
        PPHitMax = true;
        PPHitMin = false;
      }
      else if (PaletteIndexOffset == 0)
      {
        PPHitMin = true;
        PPHitMax = false;
      }

      //SDL_Log("%u", PaletteIndexOffset);
    }
    else
    {
      PaletteIndexOffset++;
      PaletteIndexOffset %= PaletteColorByIndex.size();
    }
  }
};

std::vector<std::unique_ptr<BgImage>> Backgrounds;

BgImage* CurrentBackground = nullptr;

// =============================================================================

using StringV = std::vector<std::string>;

StringV StringSplit(const std::string& str, char delim)
{
  StringV res;

  int start, end = -1;

  do
  {
    start = end + 1;
    end = str.find(delim, start);
    if (end != -1)
    {
      std::string word = str.substr(start, end - start);
      res.push_back(word);
    }
    else
    {
      if (start < (int)str.length())
      {
        res.push_back(str.substr(start, str.length()));
      }
      else
      {
        res.push_back("");
      }
    }
  }
  while (end != -1);

  return res;
}

// =============================================================================

void ProcessStatic()
{
  for (uint16_t y = 0; y < kBgHeight; y++)
  {
    size_t iy = y + CurrentBackground->ScrollPosY + (size_t)CurrentBackground->ScanlineOffsetY;

    for (uint16_t x = 0; x < kBgWidth; x++)
    {
      CurrentBackground->ScanlineOffsetX = (int)(std::sin(AngleX * PIOVER180) * CurrentBackground->ScanlineFactorX);
      if (CurrentBackground->ScanlineOffsetX < 0)
      {
        CurrentBackground->ScanlineOffsetX += (kBgWidth - 1);
      }

      size_t ix = x + CurrentBackground->ScrollPosX + (size_t)CurrentBackground->ScanlineOffsetX;

      ix %= kBgWidth;
      iy %= kBgHeight;

      SDL_Color& c = CurrentBackground->Pixels[iy][ix];

      SDL_SetRenderDrawColor(Renderer, c.r, c.g, c.b, 255);
      SDL_RenderDrawPoint(Renderer, x, y);

      AngleX += AngleIncreaseX;
      AngleY += AngleIncreaseY;

      if (AngleX > 360.0)
      {
        AngleX = 360.0 - AngleX;
      }

      if (AngleY > 360.0)
      {
        AngleY = 360.0 - AngleY;
      }
    }

    CurrentBackground->ScanlineOffsetY = (int)(std::sin(AngleY * PIOVER180) * CurrentBackground->ScanlineFactorY);
    if (CurrentBackground->ScanlineOffsetY < 0)
    {
      CurrentBackground->ScanlineOffsetY += (kBgHeight - 1);
    }
  }
}

// =============================================================================

void ProcessAnimated()
{
  for (uint16_t y = 0; y < kBgHeight; y++)
  {
    size_t iy = y + CurrentBackground->ScrollPosY + (size_t)CurrentBackground->ScanlineOffsetY;

    for (uint16_t x = 0; x < kBgWidth; x++)
    {
      CurrentBackground->ScanlineOffsetX = (int)(std::sin(AngleX * PIOVER180) * CurrentBackground->ScanlineFactorX);
      if (CurrentBackground->ScanlineOffsetX < 0)
      {
        CurrentBackground->ScanlineOffsetX += (kBgWidth - 1);
      }

      size_t ix = x + CurrentBackground->ScrollPosX + (size_t)CurrentBackground->ScanlineOffsetX;

      ix %= kBgWidth;
      iy %= kBgHeight;

      uint32_t paletteIndex = CurrentBackground->PixelsByPaletteIndex[iy][ix];

      if (paletteIndex == CurrentBackground->PaletteColorByIndex.size())
      {
        SDL_Color& c = CurrentBackground->Pixels[iy][ix];
        SDL_SetRenderDrawColor(Renderer, c.r, c.g, c.b, 255);
        SDL_RenderDrawPoint(Renderer, x, y);
      }
      else
      {
        uint32_t poi = CurrentBackground->PaletteIndexOffset;
        uint32_t newIndex = (paletteIndex + poi);

        newIndex %= CurrentBackground->PaletteColorByIndex.size();

        SDL_Color& c = CurrentBackground->PaletteColorByIndex[newIndex];

        SDL_SetRenderDrawColor(Renderer, c.r, c.g, c.b, 255);
        SDL_RenderDrawPoint(Renderer, x, y);
      }

      AngleX += AngleIncreaseX;
      AngleY += AngleIncreaseY;

      if (AngleX > 360.0)
      {
        AngleX = 360.0 - AngleX;
      }

      if (AngleY > 360.0)
      {
        AngleY = 360.0 - AngleY;
      }
    }

    CurrentBackground->ScanlineOffsetY = (int)(std::sin(AngleY * PIOVER180) * CurrentBackground->ScanlineFactorY);
    if (CurrentBackground->ScanlineOffsetY < 0)
    {
      CurrentBackground->ScanlineOffsetY += (kBgHeight - 1);
    }
  }
}

// =============================================================================

void RenderBackground()
{
  if (CurrentBackground == nullptr)
  {
    return;
  }

  SDL_SetRenderTarget(Renderer, BgRenderTexture);
  SDL_SetRenderDrawColor(Renderer, 0, 0, 0, 255);
  SDL_RenderClear(Renderer);

  if (CurrentBackground->PaletteColorByIndex.empty()
   or CurrentBackground->PaletteCycleRate == 0)
  {
    ProcessStatic();
  }
  else
  {
    ProcessAnimated();
  }
}

// =============================================================================

void BlitToFramebuffer()
{
  static SDL_Rect dst;
  dst.x = kBgDisplayX;
  dst.y = kBgDisplayY;
  dst.w = kBgW2;
  dst.h = kBgH2;

  SDL_SetRenderTarget(Renderer, Framebuffer);
  SDL_SetRenderDrawColor(Renderer, 0, 0, 0, 255);
  SDL_RenderClear(Renderer);

  SDL_RenderCopy(Renderer, BgRenderTexture, nullptr, &dst);

  static SDL_Rect r;

  for (size_t i = 0; i < CurrentBackground->PaletteColorByIndex.size(); i++)
  {
    uint32_t paletteIndex = CurrentBackground->PaletteIndexOffset + i;

    paletteIndex %= CurrentBackground->PaletteColorByIndex.size();

    SDL_Color& c = CurrentBackground->PaletteColorByIndex[paletteIndex];

    r.x = kBgDisplayX + i * 16;
    r.y = kBgDisplayY + kBgH2 + 16;
    r.w = 16;
    r.h = 16;

    SDL_SetRenderDrawColor(Renderer, c.r, c.g, c.b, 255);
    SDL_RenderFillRect(Renderer, &r);
  }
}

// =============================================================================

void PrintParams()
{
  if (CurrentBackground == nullptr)
  {
    return;
  }

  IF::Instance().Printf(0, 0,
                        IF::TextParams::Set(),
                        "AngleX = %.2f",
                        AngleX);

  IF::Instance().Printf(0, 16 * 1,
                        IF::TextParams::Set(),
                        "AngleY = %.2f",
                        AngleY);

  IF::Instance().Printf(0, 16 * 2,
                        IF::TextParams::Set(),
                        "ScrollPosX = %lu",
                        CurrentBackground->ScrollPosX);

  IF::Instance().Printf(0, 16 * 3,
                        IF::TextParams::Set(),
                        "ScrollPosY = %lu",
                        CurrentBackground->ScrollPosY);

  IF::Instance().Printf(0, 16 * 4,
                        IF::TextParams::Set(),
                        "ScanlineOffsetX = %d",
                        CurrentBackground->ScanlineOffsetX);

  IF::Instance().Printf(0, 16 * 5,
                        IF::TextParams::Set(),
                        "ScanlineOffsetY = %d",
                        CurrentBackground->ScanlineOffsetY);

  IF::Instance().Printf(0, 16 * 6,
                        IF::TextParams::Set(),
                        "PaletteIndexOffset = %u",
                        CurrentBackground->PaletteIndexOffset);
}

// =============================================================================

int CursorPositionY = 0;

const std::string kCursorLine = "-----------------------------";

void PrintModifiableParams()
{
  Parameters currentParam = (Parameters)CurrentParameterIndex;

  switch (currentParam)
  {
    case Parameters::SCROLL_SPEED_H:    { CursorPositionY = 16 * 9;  } break;
    case Parameters::SCROLL_SPEED_V:    { CursorPositionY = 16 * 10; } break;
    case Parameters::ANGLE_INC_X:       { CursorPositionY = 16 * 11; } break;
    case Parameters::ANGLE_INC_Y:       { CursorPositionY = 16 * 12; } break;
    case Parameters::SCANLINE_DELTA_X:  { CursorPositionY = 16 * 13; } break;
    case Parameters::SCANLINE_DELTA_Y:  { CursorPositionY = 16 * 14; } break;
    case Parameters::SCANLINE_FACTOR_X: { CursorPositionY = 16 * 15; } break;
    case Parameters::SCANLINE_FACTOR_Y: { CursorPositionY = 16 * 16; } break;

    default:
      break;
  }

  // ---------------------------------------------------------------------------
  // Cursor

  IF::Instance().Print(0, CursorPositionY + 6, kCursorLine, 0x00FF00);
  IF::Instance().Print(0, CursorPositionY - 6, kCursorLine, 0x00FF00);

  // ---------------------------------------------------------------------------

  IF::Instance().Printf(0, 16 * 9,
                        IF::TextParams::Set(),
                        "ScrollSpeedH = %d",
                        CurrentBackground->ScrollSpeedH);

  IF::Instance().Printf(0, 16 * 10,
                        IF::TextParams::Set(),
                        "ScrollSpeedV = %d",
                        CurrentBackground->ScrollSpeedV);

  IF::Instance().Printf(0, 16 * 11,
                        IF::TextParams::Set(),
                        "AngleIncreaseX = %.2f",
                        AngleIncreaseX);

  IF::Instance().Printf(0, 16 * 12,
                        IF::TextParams::Set(),
                        "AngleIncreaseY = %.2f",
                        AngleIncreaseY);

  IF::Instance().Printf(0, 16 * 13,
                        IF::TextParams::Set(),
                        "ScanlineFactorDeltaX = %.4f",
                        ScanlineFactorDeltaX);

  IF::Instance().Printf(0, 16 * 14,
                        IF::TextParams::Set(),
                        "ScanlineFactorDeltaY = %.4f",
                        ScanlineFactorDeltaY);

  IF::Instance().Printf(0, 16 * 15,
                        IF::TextParams::Set(),
                        "ScanlineFactorX = %.2f",
                        CurrentBackground->ScanlineFactorX);

  IF::Instance().Printf(0, 16 * 16,
                        IF::TextParams::Set(),
                        "ScanlineFactorY = %.2f",
                        CurrentBackground->ScanlineFactorY);
}

// =============================================================================

void PrintHelp()
{
  static SDL_Rect bg;
  bg.x = kScreenWidth - 340;
  bg.y = kScreenHeight - 144;
  bg.w = 340;
  bg.h = 104;

  SDL_SetRenderDrawColor(Renderer, 128, 128, 128, 220);
  SDL_RenderFillRect(Renderer, &bg);

  IF::Instance().Print(kScreenWidth - 340 + 16, kScreenHeight - 144 + 16,
                       "UP DOWN    - move cursor",
                       0xFFFFFF,
                       IF::TextAlignment::LEFT);

  IF::Instance().Print(kScreenWidth - 340 + 16, kScreenHeight - 144 + 16 * 2,
                       "LEFT RIGHT - change parameter value",
                       0xFFFFFF,
                       IF::TextAlignment::LEFT);

  IF::Instance().Print(kScreenWidth - 340 + 16, kScreenHeight - 144 + 16 * 3,
                       "[ ]        - change background image",
                       0xFFFFFF,
                       IF::TextAlignment::LEFT);

  IF::Instance().Print(kScreenWidth - 340 + 16, kScreenHeight - 144 + 16 * 4,
                       "'r'        - randomize params",
                       0xFFFFFF,
                       IF::TextAlignment::LEFT);

  IF::Instance().Print(kScreenWidth - 340 + 16, kScreenHeight - 144 + 16 * 5,
                       "'SPACE'    - reset params",
                       0xFFFFFF,
                       IF::TextAlignment::LEFT);
}

// =============================================================================

void PrintText()
{
  if (Backgrounds.empty())
  {
    IF::Instance().Print(kScreenWH,
                         kScreenHH,
                         "No images!",
                         0xFFFFFF,
                         IF::TextAlignment::CENTER,
                         4.0);
    return;
  }

  PrintParams();
  PrintModifiableParams();

  IF::Instance().Print(kScreenWidth - 16, kScreenHeight - 16,
                       "'H' - toggle help",
                       0xFFFFFF,
                       IF::TextAlignment::RIGHT);

  IF::Instance().Printf(kScreenWidth - 16, kScreenHeight - 32,
                        IF::TextParams::Set(0xFFFFFF,
                                            IF::TextAlignment::RIGHT,
                                            1.0),
                        "%u/%u",
                        (CurrentBackgroundIndex + 1), Backgrounds.size());

  IF::Instance().Printf(8, kScreenHeight - 32,
                        IF::TextParams::Set(0xFFFFFF,
                                            IF::TextAlignment::LEFT,
                                            2.0),
                        "FPS: %u",
                        FPS);

  if (ShowHelp)
  {
    PrintHelp();
  }
}

// =============================================================================

void BlitToScreen()
{
  SDL_SetRenderTarget(Renderer, nullptr);
  SDL_SetRenderDrawColor(Renderer, 0, 0, 0, 255);
  SDL_RenderClear(Renderer);

  SDL_RenderCopy(Renderer, Framebuffer, nullptr, nullptr);

  PrintText();

  SDL_RenderPresent(Renderer);
}

// =============================================================================

void Display()
{
  RenderBackground();
  BlitToFramebuffer();
  BlitToScreen();
}

// =============================================================================

void ProcessCurrentParamIncrease()
{
  if (CurrentBackground == nullptr)
  {
    return;
  }

  Parameters currentParam = (Parameters)CurrentParameterIndex;

  switch (currentParam)
  {
    case Parameters::SCROLL_SPEED_H:   { CurrentBackground->ScrollSpeedH++; } break;
    case Parameters::SCROLL_SPEED_V:   { CurrentBackground->ScrollSpeedV++; } break;
    case Parameters::ANGLE_INC_X:      { AngleIncreaseX += 0.01;            } break;
    case Parameters::ANGLE_INC_Y:      { AngleIncreaseY += 0.01;            } break;
    case Parameters::SCANLINE_DELTA_X: { ScanlineFactorDeltaX += 0.005;     } break;
    case Parameters::SCANLINE_DELTA_Y: { ScanlineFactorDeltaY += 0.005;     } break;

    case Parameters::SCANLINE_FACTOR_X:
    {
      CurrentBackground->ScanlineFactorX += ScanlineFactorDeltaX;
    }
    break;

    case Parameters::SCANLINE_FACTOR_Y:
    {
      CurrentBackground->ScanlineFactorY += ScanlineFactorDeltaY;
    }
    break;

    default:
      break;
  }
}

// =============================================================================

void ProcessCurrentParamDecrease()
{
  if (CurrentBackground == nullptr)
  {
    return;
  }

  Parameters currentParam = (Parameters)CurrentParameterIndex;

  switch (currentParam)
  {
    case Parameters::SCROLL_SPEED_H:   { CurrentBackground->ScrollSpeedH--; } break;
    case Parameters::SCROLL_SPEED_V:   { CurrentBackground->ScrollSpeedV--; } break;
    case Parameters::ANGLE_INC_X:      { AngleIncreaseX -= 0.01;            } break;
    case Parameters::ANGLE_INC_Y:      { AngleIncreaseY -= 0.01;            } break;
    case Parameters::SCANLINE_DELTA_X: { ScanlineFactorDeltaX -= 0.005;     } break;
    case Parameters::SCANLINE_DELTA_Y: { ScanlineFactorDeltaY -= 0.005;     } break;

    case Parameters::SCANLINE_FACTOR_X:
    {
      CurrentBackground->ScanlineFactorX -= ScanlineFactorDeltaX;
    }
    break;

    case Parameters::SCANLINE_FACTOR_Y:
    {
      CurrentBackground->ScanlineFactorY -= ScanlineFactorDeltaY;
    }
    break;

    default:
      break;
  }

  if (AngleIncreaseX < 0.0) { AngleIncreaseX = 0.0; }
  if (AngleIncreaseY < 0.0) { AngleIncreaseY = 0.0; }
}

// =============================================================================

void RandomizeParams()
{
  AngleIncreaseX = 0.05;
  AngleIncreaseY = 0.05;

  ScanlineFactorDeltaX = 0.025;
  ScanlineFactorDeltaY = 0.025;

  if (CurrentBackground != nullptr)
  {
    CurrentBackground->RandomizeParams();
  }
}

// =============================================================================

void ResetParams()
{
  AngleIncreaseX = 0.05;
  AngleIncreaseY = 0.05;

  ScanlineFactorDeltaX = 0.025;
  ScanlineFactorDeltaY = 0.025;

  if (CurrentBackground != nullptr)
  {
    CurrentBackground->ResetParams();
  }
}

// =============================================================================

void HandleEvent(const SDL_Event& evt)
{
  switch (evt.type)
  {
    case SDL_KEYDOWN:
    {
      switch (evt.key.keysym.sym)
      {
        case SDLK_ESCAPE:
          IsRunning = false;
          break;

        case SDLK_RIGHTBRACKET:
        {
          CurrentBackgroundIndex++;
          CurrentBackgroundIndex %= Backgrounds.size();

          if (not Backgrounds.empty())
          {
            CurrentBackground = Backgrounds[CurrentBackgroundIndex].get();
          }
        }
        break;

        case SDLK_LEFTBRACKET:
        {
          if (not Backgrounds.empty())
          {
            if (CurrentBackgroundIndex == 0)
            {
              CurrentBackgroundIndex = Backgrounds.size() - 1;
            }
            else
            {
              CurrentBackgroundIndex--;
            }

            CurrentBackgroundIndex %= Backgrounds.size();

            CurrentBackground = Backgrounds[CurrentBackgroundIndex].get();
          }
        }
        break;

        case SDLK_DOWN:
        {
          if (CurrentParameterIndex < (size_t)Parameters::LAST_ELEMENT - 1)
          {
            CurrentParameterIndex++;
          }
        }
        break;

        case SDLK_UP:
        {
          if (CurrentParameterIndex != 0)
          {
            CurrentParameterIndex--;
          }
        }
        break;

        case SDLK_LEFT:
          ProcessCurrentParamDecrease();
          break;

        case SDLK_RIGHT:
          ProcessCurrentParamIncrease();
          break;

        case SDLK_r:
          RandomizeParams();
          break;

        case SDLK_SPACE:
          ResetParams();
          break;

        case SDLK_h:
          ShowHelp = not ShowHelp;
          break;

        case SDLK_p:
        {
          for (auto& item : Backgrounds)
          {
            SDL_Log("\n-------- '%s' --------\n", item->Fname.data());
            SDL_Log("%s", item->GetColorDataString().data());
            SDL_Log("\n--------\n");
          }
        }
        break;
      }
    }
  }
}

// =============================================================================

bool LoadImage(const std::string& fname)
{
  using namespace std::filesystem;

  SDL_Surface* s = SDL_LoadBMP(fname.data());
  if (s == nullptr)
  {
    SDL_LogWarn(SDL_LOG_PRIORITY_WARN,
                "'%s' - failed to load image: %s",
                fname.data(), SDL_GetError());
    return false;
  }

  if (s->w != kBgWidth or s->h != kBgHeight)
  {
    SDL_LogWarn(SDL_LOG_PRIORITY_WARN,
                "'%s' - wrong image size! All background images must be 24 bit "
                "BMPs of %ux%u size! Skipping this one.",
                fname.data(), kBgWidth, kBgHeight);
    SDL_FreeSurface(s);
    return false;
  }

  std::unique_ptr<BgImage> image = std::make_unique<BgImage>();

  image->Fname = fname;

  auto Exit = [&image]()
  {
    Backgrounds.push_back(std::move(image));
    return true;
  };

  uint8_t* pixels = (uint8_t*)s->pixels;

  uint8_t bpp = s->format->BytesPerPixel;

  uint32_t realWidth  = s->w * bpp;
  uint32_t realHeight = s->h * bpp;

  uint16_t arrayIndexWidth  = 0;
  uint16_t arrayIndexHeight = 0;

  for (uint32_t y = 0; y < realHeight; y += bpp)
  {
    arrayIndexWidth = 0;
    for (uint16_t x = 0; x < realWidth; x += bpp)
    {
      //
      // NOTE: assuming little endian.
      //
      uint8_t r = pixels[(x + 2 + y * s->w)];
      uint8_t g = pixels[(x + 1 + y * s->w)];
      uint8_t b = pixels[(x     + y * s->w)];

      SDL_Color& c = image->Pixels[arrayIndexHeight][arrayIndexWidth];
      c.r = r;
      c.g = g;
      c.b = b;

      arrayIndexWidth++;
    }

    arrayIndexHeight++;
  }

  auto spl = StringSplit(fname, '.');

  std::string imgDataFname = spl[0] + ".txt";

  path p{imgDataFname};

  if (not exists(p))
  {
    SDL_Log("'%s' - no accompanying data file found.",
            fname.data());
    return Exit();
  }

  if (not is_regular_file(p))
  {
    SDL_LogWarn(SDL_LOG_PRIORITY_WARN,
                "'%s' - not a regular file!",
                p.c_str());
    return Exit();
  }

  NRS r;

  NRS::LoadResult lr = r.Load(imgDataFname);
  if (lr != NRS::LoadResult::LOAD_OK)
  {
    SDL_LogWarn(SDL_LOG_PRIORITY_WARN,
                "'%s' - failed to parse image data file: %s",
                imgDataFname.data(), NRS::LoadResultToString(lr));
    return Exit();
  }

  if (not r.Has("palette"))
  {
    SDL_LogWarn(SDL_LOG_PRIORITY_WARN,
                "'palette' section was not found - palette information "
                "will be ignored");
    return Exit();
  }

  NRS& pn = r["palette"];

  if (not pn.Has("colors"))
  {
    SDL_LogWarn(SDL_LOG_PRIORITY_WARN,
                "No color information was found in 'palette' section!");
    return Exit();
  }

  NRS& n = r.GetNode("palette.colors");

  size_t itemsCount = n.ChildrenCount();
  for (size_t i = 0; i < itemsCount; i++)
  {
    // NOTE: operator[] doesn't work sometimes.
    std::string ind = std::to_string(i + 1);

    uint8_t r = n.GetNode(ind).GetInt(0);
    uint8_t g = n.GetNode(ind).GetInt(1);
    uint8_t b = n.GetNode(ind).GetInt(2);

    SDL_Color pc;
    pc.r = r;
    pc.g = g;
    pc.b = b;

    image->PaletteColorByIndex.push_back(pc);
  }

  if (not image->PaletteColorByIndex.empty())
  {
    image->ConstructPaletteMap();
  }
  else
  {
    SDL_LogWarn(SDL_LOG_PRIORITY_WARN,
                "No data was found in palette section!");
  }

  if (not pn.Has("cycleRate"))
  {
    SDL_LogWarn(SDL_LOG_PRIORITY_WARN,
                "'cycleRate' is not present - assuming 0");
    image->PaletteCycleRate = 0;
  }
  else
  {
    int64_t cycleRate = r.GetNode("palette.cycleRate").GetInt();
    image->PaletteCycleRate = (uint32_t)cycleRate;

    if (cycleRate != 0)
    {
      image->PaletteCycleDeltaTime = 1.0 / (double)cycleRate;
    }
  }

  if (pn.Has("pingPong"))
  {
    image->PingPongCycling = r.GetNode("palette.pingPong").GetInt();
  }

  //SDL_Log("%s", image->ToString().data());

  Backgrounds.push_back(std::move(image));

  return true;
}

// =============================================================================

void LoadBackgrounds()
{
  // FIXME: debug
  //LoadImage("bg/bg03.bmp");

  using namespace std::filesystem;

  path p{"bg"};

  if (not exists(p))
  {
    SDL_Log("'bg' folder is not present!");
    return;
  }

  if (not is_directory(p))
  {
    SDL_Log("'bg' is not a directory type of file!");
    return;
  }

  //
  // Because directory_iterator doesn't sort.
  //
  std::set<std::string> files;
  
  for (const directory_entry& item : directory_iterator(p))
  {
    files.insert(item.path().string());
  }

  for (const std::string& fname : files)
  {
    auto spl = StringSplit(fname, '.');

    if (spl.size() == 1)
    {
      SDL_LogWarn(SDL_LOG_PRIORITY_WARN,
                  "'%s' - this file has no extension! Skipping.",
                  fname.data());
      continue;
    }

    std::string extension = spl[1];

    if (extension == "bmp")
    {
      LoadImage(fname);
    }
  }

  CurrentBackgroundIndex = 0;

  if (not Backgrounds.empty())
  {
    CurrentBackground = Backgrounds[CurrentBackgroundIndex].get();
  }
}

// =============================================================================

int main(int argc, char* argv[])
{
  RNG.seed(Clock::now().time_since_epoch().count());

  if (SDL_Init(SDL_INIT_VIDEO) != 0)
  {
    printf("SDL_Init Error: %s\n", SDL_GetError());
    return 1;
  }

  SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);

  SDL_Window* window = SDL_CreateWindow("earthbound-bgfx",
                                        0, 0,
                                        kScreenWidth, kScreenHeight,
                                        SDL_WINDOW_SHOWN);

  const char* driverHint = "opengl";

  SDL_bool ok = SDL_SetHint(SDL_HINT_RENDER_DRIVER, driverHint);
  if (ok == SDL_FALSE)
  {
    SDL_Log("Hint value '%s' could not be set! (SDL_HINT_RENDER_DRIVER)",
            driverHint);
  }

  Renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (Renderer == nullptr)
  {
    SDL_Log("Failed to create renderer with SDL_RENDERER_ACCELERATED"
            " - falling back to software");

    Renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (Renderer == nullptr)
    {
      SDL_Log("Failed to create renderer: %s", SDL_GetError());
      return 1;
    }
  }

  if (Renderer == nullptr)
  {
    printf("Couldn't create renderer: maybe render to texture isn't supported.\n");
    return 1;
  }

  IF::Instance().Init(Renderer);

  BgRenderTexture = SDL_CreateTexture(Renderer,
                                      SDL_PIXELFORMAT_RGBA32,
                                      SDL_TEXTUREACCESS_TARGET,
                                      kBgWidth,
                                      kBgHeight);

  if (BgRenderTexture == nullptr)
  {
    SDL_LogError(SDL_LOG_PRIORITY_ERROR,
                 "Failed to create render texture for background: %s",
                 SDL_GetError());
    return 1;
  }

  Framebuffer = SDL_CreateTexture(Renderer,
                                  SDL_PIXELFORMAT_RGBA32,
                                  SDL_TEXTUREACCESS_TARGET,
                                  kScreenWidth,
                                  kScreenHeight);

  int res = SDL_SetRenderDrawBlendMode(Renderer, SDL_BLENDMODE_BLEND);
  if (res < 0)
  {
    SDL_LogError(SDL_LOG_PRIORITY_ERROR,
                 "Failed to set renderer blend mode: %s",
                 SDL_GetError());
    return 1;
  }

  LoadBackgrounds();

  SDL_Event evt;

  Clock::time_point tpStart;
  Clock::time_point tpEnd;

  ns dt = ns{0};

  double dtAcc     = 0.0;
  double cycleAcc  = 0.0;

  uint32_t fpsCount = 0;

  while (IsRunning)
  {
    tpStart = Clock::now();
    tpEnd   = tpStart;

    while (SDL_PollEvent(&evt))
    {
      HandleEvent(evt);
    }

    Display();

    fpsCount++;

    tpEnd = Clock::now();
    dt = tpEnd - tpStart;

    DeltaTime = std::chrono::duration<double>(dt).count();

    dtAcc    += DeltaTime;
    cycleAcc += DeltaTime;

    if (dtAcc > 1.0)
    {
      FPS = fpsCount;
      fpsCount = 0;
      dtAcc = 0.0;
    }

    if (CurrentBackground != nullptr)
    {
      CurrentBackground->ScrollPosX += CurrentBackground->ScrollSpeedH;
      CurrentBackground->ScrollPosY += CurrentBackground->ScrollSpeedV;

      CurrentBackground->ScrollPosX %= kBgWidth;
      CurrentBackground->ScrollPosY %= kBgHeight;

      if (CurrentBackground->PaletteCycleRate > 0
      and cycleAcc > CurrentBackground->PaletteCycleDeltaTime)
      {
        cycleAcc = 0.0;
        CurrentBackground->CyclePalette();
      }
    }
  }

  SDL_Quit();

  printf("Goodbye!\n");

  return 0;
}
