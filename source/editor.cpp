#include "editor.h"
#include "ids.h"

#include <memory>
#include <string>
#include <cstring>
#include <cmath>
#include <cstdio>

#ifdef _WIN32
  #include <windows.h>
  #include <windowsx.h>   // GET_X_LPARAM / GET_Y_LPARAM
  #include <gdiplus.h>
  #include <shlwapi.h>
  #pragma comment(lib, "gdiplus.lib")
  #pragma comment(lib, "Shlwapi.lib")
#endif

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace SvenderBass {

#ifdef _WIN32

static ULONG_PTR g_gdiplusToken = 0;

static void ensureGdiPlusStarted()
{
  if (g_gdiplusToken != 0)
    return;

  Gdiplus::GdiplusStartupInput in;
  if (Gdiplus::GdiplusStartup(&g_gdiplusToken, &in, nullptr) != Gdiplus::Ok)
    g_gdiplusToken = 0;
}

static std::wstring getModuleDirW()
{
  wchar_t path[MAX_PATH] = {};
  HMODULE hm = nullptr;

  // With GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, lpModuleName is treated as an address.
  GetModuleHandleExW(
    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
    reinterpret_cast<LPCWSTR>(&getModuleDirW),
    &hm
  );

  if (!hm)
    hm = GetModuleHandleW(nullptr);

  GetModuleFileNameW(hm, path, MAX_PATH);
  PathRemoveFileSpecW(path);
  return std::wstring(path);
}

static std::wstring buildResourcePathW(const wchar_t* fileName)
{
  // Typical module dir:
  //   ...\SvenderBass.vst3\Contents\x86_64-win
  // Resources are:
  //   ...\SvenderBass.vst3\Contents\Resources\<fileName>

  wchar_t p[MAX_PATH] = {};
  std::wstring dllDir = getModuleDirW();
  wcsncpy_s(p, dllDir.c_str(), _TRUNCATE);

  // Move from x86_64-win -> Contents
  PathRemoveFileSpecW(p);

  // Append Resources\<fileName>
  PathAppendW(p, L"Resources");
  PathAppendW(p, fileName);

  return std::wstring(p);
}

static constexpr UINT_PTR kAnimTimerId = 1;
static constexpr UINT kAnimTimerMs = 16;
static constexpr int kKnobCount = 6;

static const Steinberg::Vst::ParamID kKnobParams[kKnobCount] =
{
  kParamInputGain,
  kParamBass,
  kParamMid,
  kParamTreble,
  kParamDrive,
  kParamOutput
};

static int knobIndexFromParam(Steinberg::Vst::ParamID paramId)
{
  for (int i = 0; i < kKnobCount; ++i)
  {
    if (kKnobParams[i] == paramId)
      return i;
  }
  return -1;
}

static float clamp01(float v)
{
  if (v < 0.0f) return 0.0f;
  if (v > 1.0f) return 1.0f;
  return v;
}

static float normalizedToAngleRad(float v)
{
  // Map [0..1] to a 240-degree sweep (-120 to +120), where 0 is top center.
  const float startDeg = -120.0f;
  const float endDeg = 120.0f;
  const float deg = startDeg + (endDeg - startDeg) * clamp01(v);
  const float mathDeg = deg - 90.0f;
  const float pi = 3.14159265358979323846f;
  return mathDeg * (pi / 180.0f);
}

static bool pointInRect(const POINT& p, const RECT& r)
{
  return p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom;
}

static std::wstring formatDisplayValueOneDecimal(float normalized)
{
  const float clamped = clamp01(normalized);
  const float display = clamped * 11.0f; // 0.0 to 11.0

  wchar_t buf[32] = {};
  swprintf_s(buf, L"%.1f", display);
  return std::wstring(buf);
}

static RECT getInputDisplayRect(const RECT& knobRect)
{
  const int cx = (knobRect.left + knobRect.right) / 2;
  const int cy = (knobRect.top + knobRect.bottom) / 2;
  const int knobW = knobRect.right - knobRect.left;
  const int knobH = knobRect.bottom - knobRect.top;
  const int w = (int)(knobW * 0.60f);
  const int h = (int)(knobH * 0.18f);

  RECT r{};
  r.left = cx - w / 2;
  r.right = r.left + w;
  r.top = cy - h / 2;
  r.bottom = r.top + h;
  return r;
}

static RECT offsetRect(const RECT& r, float dx, float dy)
{
  RECT out{};
  out.left = (int)(r.left + dx);
  out.right = (int)(r.right + dx);
  out.top = (int)(r.top + dy);
  out.bottom = (int)(r.bottom + dy);
  return out;
}

static const float kTextOffsetX = 20.0f; // 40 right + 10 left + 20 left + 10 right
static const float kTextOffsetY = 46.0f; // 40 down + 4 down + 4 down - 2 up
static const float kDriveTextOffsetX = 5.0f;

static RECT getTextRectForKnob(const RECT& knobRect, Steinberg::Vst::ParamID paramId)
{
  RECT rc = offsetRect(getInputDisplayRect(knobRect), kTextOffsetX, kTextOffsetY);
  if (paramId == kParamDrive)
    rc = offsetRect(rc, kDriveTextOffsetX, 0.0f);
  return rc;
}

static RECT scaleRect(const RECT& r, float sx, float sy)
{
  RECT out{};
  out.left = (int)(r.left * sx + 0.5f);
  out.top = (int)(r.top * sy + 0.5f);
  out.right = (int)(r.right * sx + 0.5f);
  out.bottom = (int)(r.bottom * sy + 0.5f);
  return out;
}

static void getClientScale(HWND hWnd, float& sx, float& sy)
{
  RECT rc{};
  GetClientRect(hWnd, &rc);
  const float designW = 1200.0f;
  const float designH = 450.0f;
  const float w = (float)(rc.right - rc.left);
  const float h = (float)(rc.bottom - rc.top);
  sx = (designW > 0.0f) ? (w / designW) : 1.0f;
  sy = (designH > 0.0f) ? (h / designH) : 1.0f;
}

struct EditorWin32State
{
  HWND hwnd = nullptr;

  std::unique_ptr<Gdiplus::Image> faceplate;
  std::wstring faceplatePath;

  // EditController provides begin/perform/endEdit helpers.
  Steinberg::Vst::EditController* controller = nullptr;

  bool draggingParam = false;
  Steinberg::Vst::ParamID activeParam = 0;
  int lastY = 0;

  float animValues[kKnobCount]{};
  float animTargets[kKnobCount]{};
  bool animInitialized = false;
  UINT_PTR animTimer = 0;

  // Hardcoded hit area for the Input knob on a 1200x450 faceplate.
  RECT inputKnobRect{ 87, 140, 257, 310 };
  // Bass knob is 198px to the right of input (moved +98px), same size.
  RECT bassKnobRect{ 286, 140, 456, 310 };
  // Mid knob is 125px to the right of bass, same size.
  RECT midKnobRect{ 418, 140, 588, 310 };
  // Treble knob is 132px to the right of mid, same size.
  RECT trebleKnobRect{ 566, 140, 736, 310 };
  // Drive knob is 132px to the right of treble, same size.
  RECT driveKnobRect{ 698, 140, 868, 310 };
  // Output knob is 150px to the right of drive, same size.
  RECT outputKnobRect{ 914, 140, 1084, 310 };
  // Ultra Low toggle: center from faceplate@2x (784, 680) -> (392, 340), ~40px wide at 2x.
  RECT ultraLowRect{ 382, 330, 402, 350 };
  // Ultra High toggle: center from faceplate@2x (1340, 680) -> (670, 340), same size as Ultra Low.
  RECT ultraHighRect{ 660, 330, 680, 350 };
};

static RECT getKnobRectForParam(const EditorWin32State* st, Steinberg::Vst::ParamID paramId)
{
  if (!st)
    return RECT{};

  if (paramId == kParamBass) return st->bassKnobRect;
  if (paramId == kParamMid) return st->midKnobRect;
  if (paramId == kParamTreble) return st->trebleKnobRect;
  if (paramId == kParamDrive) return st->driveKnobRect;
  if (paramId == kParamOutput) return st->outputKnobRect;
  return st->inputKnobRect;
}

static void initAnimation(EditorWin32State* st)
{
  if (!st || !st->controller)
    return;

  for (int i = 0; i < kKnobCount; ++i)
  {
    const float v = (float)st->controller->getParamNormalized(kKnobParams[i]);
    st->animValues[i] = v;
    st->animTargets[i] = v;
  }
  st->animInitialized = true;
}

static float getAnimatedValue(const EditorWin32State* st, Steinberg::Vst::ParamID paramId)
{
  if (!st || !st->controller)
    return 0.0f;

  const int idx = knobIndexFromParam(paramId);
  if (idx < 0 || !st->animInitialized)
    return (float)st->controller->getParamNormalized(paramId);

  return st->animValues[idx];
}

static LRESULT CALLBACK EditorWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  auto* st = reinterpret_cast<EditorWin32State*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

  switch (msg)
  {
    case WM_NCCREATE:
    {
      auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
      SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
      return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    case WM_LBUTTONDOWN:
    {
      if (!st || !st->controller)
        break;

      POINT p{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
      float sx = 1.0f;
      float sy = 1.0f;
      getClientScale(hWnd, sx, sy);
      const RECT ultraLowRc = scaleRect(st->ultraLowRect, sx, sy);
      const RECT inputKnobRc = scaleRect(st->inputKnobRect, sx, sy);
      const RECT inputTextRc = getTextRectForKnob(inputKnobRc, kParamInputGain);
      const RECT bassKnobRc = scaleRect(st->bassKnobRect, sx, sy);
      const RECT bassTextRc = getTextRectForKnob(bassKnobRc, kParamBass);
      const RECT midKnobRc = scaleRect(st->midKnobRect, sx, sy);
      const RECT midTextRc = getTextRectForKnob(midKnobRc, kParamMid);
      const RECT trebleKnobRc = scaleRect(st->trebleKnobRect, sx, sy);
      const RECT trebleTextRc = getTextRectForKnob(trebleKnobRc, kParamTreble);
      const RECT driveKnobRc = scaleRect(st->driveKnobRect, sx, sy);
      const RECT driveTextRc = getTextRectForKnob(driveKnobRc, kParamDrive);
      const RECT outputKnobRc = scaleRect(st->outputKnobRect, sx, sy);
      const RECT outputTextRc = getTextRectForKnob(outputKnobRc, kParamOutput);
      const RECT ultraHighRc = scaleRect(st->ultraHighRect, sx, sy);

      if (pointInRect(p, ultraLowRc))
      {
        const ParamValue cur = st->controller->getParamNormalized(kParamUltraLow);
        const ParamValue next = (cur >= 0.5 ? 0.0 : 1.0);
        st->controller->beginEdit(kParamUltraLow);
        st->controller->setParamNormalized(kParamUltraLow, next);
        st->controller->performEdit(kParamUltraLow, next);
        st->controller->endEdit(kParamUltraLow);
        InvalidateRect(hWnd, &ultraLowRc, FALSE);
        return 0;
      }
      if (pointInRect(p, ultraHighRc))
      {
        const ParamValue cur = st->controller->getParamNormalized(kParamUltraHigh);
        const ParamValue next = (cur >= 0.5 ? 0.0 : 1.0);
        st->controller->beginEdit(kParamUltraHigh);
        st->controller->setParamNormalized(kParamUltraHigh, next);
        st->controller->performEdit(kParamUltraHigh, next);
        st->controller->endEdit(kParamUltraHigh);
        InvalidateRect(hWnd, &ultraHighRc, FALSE);
        return 0;
      }

      if (pointInRect(p, inputKnobRc) || pointInRect(p, inputTextRc))
        st->activeParam = kParamInputGain;
      else if (pointInRect(p, bassKnobRc) || pointInRect(p, bassTextRc))
        st->activeParam = kParamBass;
      else if (pointInRect(p, midKnobRc) || pointInRect(p, midTextRc))
        st->activeParam = kParamMid;
      else if (pointInRect(p, trebleKnobRc) || pointInRect(p, trebleTextRc))
        st->activeParam = kParamTreble;
      else if (pointInRect(p, driveKnobRc) || pointInRect(p, driveTextRc))
        st->activeParam = kParamDrive;
      else if (pointInRect(p, outputKnobRc) || pointInRect(p, outputTextRc))
        st->activeParam = kParamOutput;
      else
        break;

      st->draggingParam = true;
      st->lastY = p.y;
      SetCapture(hWnd);

      st->controller->beginEdit(st->activeParam);
      return 0;
      break;
    }

    case WM_MOUSEMOVE:
    {
      if (!st || !st->controller || !st->draggingParam)
        break;

      POINT p{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

      const int dy = p.y - st->lastY;
      st->lastY = p.y;

      const ParamValue cur = st->controller->getParamNormalized(st->activeParam);
      float next = (float)cur - (float)dy / 150.0f;
      next = clamp01(next);

      // performEdit is the important call for automation + notifying the host.
      st->controller->setParamNormalized(st->activeParam, next);
      st->controller->performEdit(st->activeParam, next);

      const int activeIdx = knobIndexFromParam(st->activeParam);
      if (activeIdx >= 0)
        st->animTargets[activeIdx] = next;

      float sx = 1.0f;
      float sy = 1.0f;
      getClientScale(hWnd, sx, sy);
      const RECT knobRc = scaleRect(
        st->activeParam == kParamBass ? st->bassKnobRect :
        st->activeParam == kParamMid ? st->midKnobRect :
        st->activeParam == kParamTreble ? st->trebleKnobRect :
        st->activeParam == kParamDrive ? st->driveKnobRect :
        st->activeParam == kParamOutput ? st->outputKnobRect : st->inputKnobRect, sx, sy);
      RECT inv = getTextRectForKnob(knobRc, st->activeParam);
      InvalidateRect(hWnd, &knobRc, FALSE);
      InvalidateRect(hWnd, &inv, FALSE);
      return 0;
    }

    case WM_LBUTTONUP:
    {
      if (!st || !st->controller)
        break;

      if (st->draggingParam)
      {
        st->draggingParam = false;
        ReleaseCapture();

        st->controller->endEdit(st->activeParam);
        st->activeParam = 0;
        return 0;
      }
      break;
    }

    case WM_TIMER:
    {
      if (!st || !st->controller || wParam != kAnimTimerId)
        break;

      bool anyChange = false;
      for (int i = 0; i < kKnobCount; ++i)
      {
        const float target = (float)st->controller->getParamNormalized(kKnobParams[i]);
        st->animTargets[i] = target;

        const float cur = st->animValues[i];
        const float next = cur + (target - cur) * 0.25f;
        const float delta = target - next;
        st->animValues[i] = (std::fabs(delta) < 0.0005f) ? target : next;

        if (std::fabs(target - st->animValues[i]) > 0.0005f)
          anyChange = true;
      }

      if (anyChange)
      {
        float sx = 1.0f;
        float sy = 1.0f;
        getClientScale(hWnd, sx, sy);

        for (int i = 0; i < kKnobCount; ++i)
        {
          const RECT knobRc = scaleRect(getKnobRectForParam(st, kKnobParams[i]), sx, sy);
          const RECT textRc = getTextRectForKnob(knobRc, kKnobParams[i]);
          InvalidateRect(hWnd, &knobRc, FALSE);
          InvalidateRect(hWnd, &textRc, FALSE);
        }
      }
      return 0;
    }

    case WM_PAINT:
    {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hWnd, &ps);

      if (st)
      {
        RECT rcPaint = ps.rcPaint;
        const int paintW = rcPaint.right - rcPaint.left;
        const int paintH = rcPaint.bottom - rcPaint.top;

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, paintW, paintH);
        HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

        HBRUSH br = CreateSolidBrush(RGB(0, 0, 0));
        RECT localRc{ 0, 0, paintW, paintH };
        FillRect(memDC, &localRc, br);
        DeleteObject(br);

        Gdiplus::Graphics g(memDC);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetClip(Gdiplus::Rect(0, 0, paintW, paintH));

        RECT rcClient{};
        GetClientRect(hWnd, &rcClient);
        const int w = rcClient.right - rcClient.left;
        const int h = rcClient.bottom - rcClient.top;

        if (st->faceplate && st->faceplate->GetLastStatus() == Gdiplus::Ok)
          g.DrawImage(st->faceplate.get(), -rcPaint.left, -rcPaint.top, w, h);

        if (st->controller)
        {
          float sx = 1.0f;
          float sy = 1.0f;
          getClientScale(hWnd, sx, sy);
          const RECT inputKnobRc = scaleRect(st->inputKnobRect, sx, sy);
          const RECT bassKnobRc = scaleRect(st->bassKnobRect, sx, sy);
          const RECT midKnobRc = scaleRect(st->midKnobRect, sx, sy);
          const RECT trebleKnobRc = scaleRect(st->trebleKnobRect, sx, sy);
          const RECT driveKnobRc = scaleRect(st->driveKnobRect, sx, sy);
          const RECT outputKnobRc = scaleRect(st->outputKnobRect, sx, sy);
          const RECT ultraLowRc = scaleRect(st->ultraLowRect, sx, sy);
          const RECT ultraHighRc = scaleRect(st->ultraHighRect, sx, sy);

          const RECT sampleTextRc = getTextRectForKnob(inputKnobRc, kParamInputGain);
          const float textH = (float)(sampleTextRc.bottom - sampleTextRc.top);
          const float fontSize = (textH > 0.0f) ? textH * 0.65f : 10.0f;

          Gdiplus::FontFamily fontFamily(L"Segoe UI");
          Gdiplus::Font font(&fontFamily, fontSize, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
          Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, 0, 0, 0));
          Gdiplus::SolidBrush shadowBrush(Gdiplus::Color(160, 0, 0, 0));
          Gdiplus::SolidBrush highlightBrush(Gdiplus::Color(180, 255, 255, 255));
          Gdiplus::StringFormat fmt;
          fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
          fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
          g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

          g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
          Gdiplus::Pen indicatorPen(Gdiplus::Color(255, 0, 0, 0),
                                    (Gdiplus::REAL)(textH * 0.10f));
          const auto drawIndicator = [&](Steinberg::Vst::ParamID id, const RECT& knobRc)
          {
            const float norm = getAnimatedValue(st, id);
            float angle = normalizedToAngleRad(norm);
            float cx = (float)(knobRc.left + knobRc.right) * 0.5f - (float)rcPaint.left;
            float cy = (float)(knobRc.top + knobRc.bottom) * 0.5f - (float)rcPaint.top;
            const float w = (float)(knobRc.right - knobRc.left);
            const float h = (float)(knobRc.bottom - knobRc.top);
            const float minDim = (w < h) ? w : h;
            float x1 = 0.0f;
            float y1 = 0.0f;
            float x2 = 0.0f;
            float y2 = 0.0f;

            if (id == kParamInputGain || id == kParamBass || id == kParamMid || id == kParamTreble ||
                id == kParamDrive || id == kParamOutput)
            {
              const float startDeg = -130.0f;
              const float endDeg = 120.0f;
              const float deg = startDeg + (endDeg - startDeg) * clamp01(norm);
              const float mathDeg = deg - 90.0f;
              const float pi = 3.14159265358979323846f;
              angle = mathDeg * (pi / 180.0f);

              if (id == kParamBass)
              {
                const float baseCx = (float)(inputKnobRc.left + inputKnobRc.right) * 0.5f - (float)rcPaint.left;
                const float baseCy = (float)(inputKnobRc.top + inputKnobRc.bottom) * 0.5f - (float)rcPaint.top;
                cx = baseCx + (214.0f * sx);
                cy = baseCy + (46.0f * sy);
              }
              else if (id == kParamMid)
              {
                const float baseCx = (float)(inputKnobRc.left + inputKnobRc.right) * 0.5f - (float)rcPaint.left;
                const float baseCy = (float)(inputKnobRc.top + inputKnobRc.bottom) * 0.5f - (float)rcPaint.top;
                cx = baseCx + (349.0f * sx);
                cy = baseCy + (46.0f * sy);
              }
              else if (id == kParamTreble)
              {
                const float trebleCx = (float)(trebleKnobRc.left + trebleKnobRc.right) * 0.5f - (float)rcPaint.left;
                const float trebleCy = (float)(trebleKnobRc.top + trebleKnobRc.bottom) * 0.5f - (float)rcPaint.top;
                cx = trebleCx + (19.0f * sx);
                cy = trebleCy + (46.0f * sy);
              }
              else if (id == kParamDrive)
              {
                const float driveCx = (float)(driveKnobRc.left + driveKnobRc.right) * 0.5f - (float)rcPaint.left;
                const float driveCy = (float)(driveKnobRc.top + driveKnobRc.bottom) * 0.5f - (float)rcPaint.top;
                cx = driveCx + (24.0f * sx);
                cy = driveCy + (46.0f * sy);
              }
              else if (id == kParamOutput)
              {
                const float outputCx = (float)(outputKnobRc.left + outputKnobRc.right) * 0.5f - (float)rcPaint.left;
                const float outputCy = (float)(outputKnobRc.top + outputKnobRc.bottom) * 0.5f - (float)rcPaint.top;
                cx = outputCx + (21.0f * sx);
                cy = outputCy + (46.0f * sy);
              }
              else
              {
                cx += 19.0f * sx;
                cy += 46.0f * sy;
              }
              const float length = (minDim * 0.20f) - (6.0f * sx);
              const float start = length * 0.5f;
              x1 = cx + start * std::cos(angle);
              y1 = cy + start * std::sin(angle);
              x2 = cx + length * std::cos(angle);
              y2 = cy + length * std::sin(angle);
            }
            else
            {
              const float inner = minDim * 0.10f;
              const float outer = minDim * 0.42f;
              x1 = cx + inner * std::cos(angle);
              y1 = cy + inner * std::sin(angle);
              x2 = cx + outer * std::cos(angle);
              y2 = cy + outer * std::sin(angle);
            }

            g.DrawLine(&indicatorPen, x1, y1, x2, y2);
          };

          /*
          const auto drawValue = [&](Steinberg::Vst::ParamID id, const RECT& knobRc)
          {
            if (id == kParamInputGain && !(st->draggingParam && st->activeParam == kParamInputGain))
              return;

            const RECT textRc = getTextRectForKnob(knobRc, id);
            const float norm = getAnimatedValue(st, id);
            std::wstring valueText = formatDisplayValueOneDecimal(norm);

            Gdiplus::RectF layout(
              (float)(textRc.left - rcPaint.left),
              (float)(textRc.top - rcPaint.top),
              (float)(textRc.right - textRc.left),
              (float)(textRc.bottom - textRc.top)
            );
            Gdiplus::RectF shadowLayout = layout;
            shadowLayout.X -= 1.0f;
            shadowLayout.Y -= 1.0f;
            g.DrawString(valueText.c_str(), -1, &font, shadowLayout, &fmt, &shadowBrush);

            Gdiplus::RectF highlightLayout = layout;
            highlightLayout.X += 1.0f;
            highlightLayout.Y += 1.0f;
            g.DrawString(valueText.c_str(), -1, &font, highlightLayout, &fmt, &highlightBrush);

            g.DrawString(valueText.c_str(), -1, &font, layout, &fmt, &textBrush);
          };
          */

          drawIndicator(kParamInputGain, inputKnobRc);
          drawIndicator(kParamBass, bassKnobRc);
          drawIndicator(kParamMid, midKnobRc);
          drawIndicator(kParamTreble, trebleKnobRc);
          drawIndicator(kParamDrive, driveKnobRc);
          drawIndicator(kParamOutput, outputKnobRc);

          /*
          drawValue(kParamInputGain, inputKnobRc);
          drawValue(kParamBass, bassKnobRc);
          drawValue(kParamMid, midKnobRc);
          drawValue(kParamTreble, trebleKnobRc);
          drawValue(kParamDrive, driveKnobRc);
          drawValue(kParamOutput, outputKnobRc);
          */

          const ParamValue ultraLow = st->controller->getParamNormalized(kParamUltraLow);
          if (ultraLow >= 0.5)
          {
            const Gdiplus::REAL ulW = (Gdiplus::REAL)(ultraLowRc.right - ultraLowRc.left) - 2.0f;
            const Gdiplus::REAL ulH = (Gdiplus::REAL)(ultraLowRc.bottom - ultraLowRc.top) - 2.0f;
            Gdiplus::SolidBrush redBrush(Gdiplus::Color(220, 200, 20, 20));
            g.FillEllipse(&redBrush,
                          (Gdiplus::REAL)(ultraLowRc.left - rcPaint.left) + 1.0f,
                          (Gdiplus::REAL)(ultraLowRc.top - rcPaint.top) + 1.0f,
                          ulW, ulH);
          }
          const ParamValue ultraHigh = st->controller->getParamNormalized(kParamUltraHigh);
          if (ultraHigh >= 0.5)
          {
            const Gdiplus::REAL uhW = (Gdiplus::REAL)(ultraHighRc.right - ultraHighRc.left) - 2.0f;
            const Gdiplus::REAL uhH = (Gdiplus::REAL)(ultraHighRc.bottom - ultraHighRc.top) - 2.0f;
            Gdiplus::SolidBrush redBrush(Gdiplus::Color(220, 200, 20, 20));
            g.FillEllipse(&redBrush,
                          (Gdiplus::REAL)(ultraHighRc.left - rcPaint.left) + 1.0f,
                          (Gdiplus::REAL)(ultraHighRc.top - rcPaint.top) + 1.0f,
                          uhW, uhH);
          }
        }

        BitBlt(hdc, rcPaint.left, rcPaint.top, paintW, paintH, memDC, 0, 0, SRCCOPY);

        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);
      }

      EndPaint(hWnd, &ps);
      return 0;
    }

    case WM_NCDESTROY:
    {
      if (st)
      {
        if (st->animTimer)
          KillTimer(hWnd, kAnimTimerId);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        delete st;
      }
      return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
  }

  return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static ATOM registerEditorClass()
{
  static ATOM atom = 0;
  if (atom)
    return atom;

  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = EditorWndProc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = nullptr;
  wc.lpszClassName = L"SvenderBass_EditorView";

  atom = RegisterClassExW(&wc);
  return atom;
}

#endif // _WIN32

Editor::Editor(EditController* controller)
: EditorView(controller)
{
  rect_.left   = 0;
  rect_.top    = 0;
  rect_.right  = 1200;
  rect_.bottom = 450;

#ifdef _WIN32
  ensureGdiPlusStarted();
#endif
}

// NOTE: No Editor::~Editor() definition here because your editor.h has ~Editor() = default;

tresult PLUGIN_API Editor::attached(void* parent, FIDString type)
{
#ifdef _WIN32
  if (!parent || !type)
    return kInvalidArgument;

  if (std::strcmp(type, kPlatformTypeHWND) != 0)
    return kNotImplemented;

  if (!registerEditorClass())
    return kInternalError;

  auto* st = new EditorWin32State();

  // EditorView provides getController() (non-static). We store the interface pointer.
  st->controller = this->getController();

  st->faceplatePath = buildResourcePathW(L"faceplate.png");
  st->faceplate.reset(new Gdiplus::Image(st->faceplatePath.c_str()));

  HWND parentHwnd = (HWND)parent;

  const int w = rect_.right - rect_.left;
  const int h = rect_.bottom - rect_.top;

  st->hwnd = CreateWindowExW(
    0,
    L"SvenderBass_EditorView",
    L"",
    WS_CHILD | WS_VISIBLE,
    0, 0, w, h,
    parentHwnd,
    nullptr,
    GetModuleHandleW(nullptr),
    st
  );

  if (!st->hwnd)
  {
    delete st;
    return kInternalError;
  }

  initAnimation(st);
  st->animTimer = SetTimer(st->hwnd, kAnimTimerId, kAnimTimerMs, nullptr);

  tresult r = EditorView::attached(parent, type);
  InvalidateRect(st->hwnd, nullptr, TRUE);
  return r;
#else
  return EditorView::attached(parent, type);
#endif
}

tresult PLUGIN_API Editor::removed()
{
  return EditorView::removed();
}

tresult PLUGIN_API Editor::getSize(ViewRect* size)
{
  if (!size)
    return kInvalidArgument;

  *size = rect_;
  return kResultOk;
}

tresult PLUGIN_API Editor::onSize(ViewRect* newSize)
{
  if (!newSize)
    return kInvalidArgument;

  rect_ = *newSize;
  return kResultOk;
}

} // namespace SvenderBass
