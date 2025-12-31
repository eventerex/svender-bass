#pragma once
#include <cmath>
#include <algorithm>

namespace SvenderBass::DSP {

constexpr float kPi = 3.14159265358979323846f;

inline float dbToLin(float db) { return std::pow(10.0f, db / 20.0f); }
inline float clamp(float x, float lo, float hi) { return std::max(lo, std::min(x, hi)); }

struct Smoother {
  float a = 0.0f;
  float y = 0.0f;
  void setTimeMs(float sr, float ms) {
    const float t = std::max(0.001f, ms) * 0.001f;
    a = std::exp(-1.0f / (t * sr));
  }
  float process(float x) { y = a * y + (1.0f - a) * x; return y; }
  void reset(float v) { y = v; }
};

struct EnvelopeFollower {
  float a = 0.0f;
  float y = 0.0f;

  void setTimeMs(float sr, float ms) {
    float t = std::max(0.001f, ms) * 0.001f;
    a = std::exp(-1.0f / (t * sr));
  }

  float process(float x) {
    x = std::fabs(x);
    y = a * y + (1.0f - a) * x;
    return y;
  }

  void reset() { y = 0.0f; }
};

struct Biquad {
  float b0=1, b1=0, b2=0, a1=0, a2=0;
  float z1=0, z2=0;

  void reset() { z1 = z2 = 0.0f; }

  float process(float x) {
    float y = b0*x + z1;
    z1 = b1*x - a1*y + z2;
    z2 = b2*x - a2*y;
    return y;
  }

  void setLowShelf(float sr, float f0, float gainDb, float Q=0.707f) {
    float A = std::pow(10.0f, gainDb/40.0f);
    float w0 = 2.0f * float(kPi) * (f0 / sr);
    float cw = std::cos(w0), sw = std::sin(w0);
    float alpha = sw/(2.0f*Q);
    float sqrtA = std::sqrt(A);

    float b0n =    A*((A+1) - (A-1)*cw + 2*sqrtA*alpha);
    float b1n =  2*A*((A-1) - (A+1)*cw);
    float b2n =    A*((A+1) - (A-1)*cw - 2*sqrtA*alpha);
    float a0n =        (A+1) + (A-1)*cw + 2*sqrtA*alpha;
    float a1n =   -2*((A-1) + (A+1)*cw);
    float a2n =        (A+1) + (A-1)*cw - 2*sqrtA*alpha;

    b0 = b0n/a0n; b1 = b1n/a0n; b2 = b2n/a0n;
    a1 = a1n/a0n; a2 = a2n/a0n;
  }

  void setHighShelf(float sr, float f0, float gainDb, float Q=0.707f) {
    float A = std::pow(10.0f, gainDb/40.0f);
    float w0 = 2.0f * float(kPi) * (f0 / sr);
    float cw = std::cos(w0), sw = std::sin(w0);
    float alpha = sw/(2.0f*Q);
    float sqrtA = std::sqrt(A);

    float b0n =    A*((A+1) + (A-1)*cw + 2*sqrtA*alpha);
    float b1n = -2*A*((A-1) + (A+1)*cw);
    float b2n =    A*((A+1) + (A-1)*cw - 2*sqrtA*alpha);
    float a0n =        (A+1) - (A-1)*cw + 2*sqrtA*alpha;
    float a1n =    2*((A-1) - (A+1)*cw);
    float a2n =        (A+1) - (A-1)*cw - 2*sqrtA*alpha;

    b0 = b0n/a0n; b1 = b1n/a0n; b2 = b2n/a0n;
    a1 = a1n/a0n; a2 = a2n/a0n;
  }

  void setPeaking(float sr, float f0, float gainDb, float Q=1.0f) {
    float A = std::pow(10.0f, gainDb/40.0f);
    float w0 = 2.0f * float(kPi) * (f0 / sr);
    float cw = std::cos(w0), sw = std::sin(w0);
    float alpha = sw/(2.0f*Q);

    float b0n = 1 + alpha*A;
    float b1n = -2*cw;
    float b2n = 1 - alpha*A;
    float a0n = 1 + alpha/A;
    float a1n = -2*cw;
    float a2n = 1 - alpha/A;

    b0 = b0n/a0n; b1 = b1n/a0n; b2 = b2n/a0n;
    a1 = a1n/a0n; a2 = a2n/a0n;
  }

  void setHP(float sr, float f0, float Q=0.707f) {
    float w0 = 2.0f * float(kPi) * (f0 / sr);
    float cw = std::cos(w0), sw = std::sin(w0);
    float alpha = sw/(2.0f*Q);

    float b0n =  (1 + cw)/2;
    float b1n = -(1 + cw);
    float b2n =  (1 + cw)/2;
    float a0n =  1 + alpha;
    float a1n = -2*cw;
    float a2n =  1 - alpha;

    b0 = b0n/a0n; b1 = b1n/a0n; b2 = b2n/a0n;
    a1 = a1n/a0n; a2 = a2n/a0n;
  }

  void setLP(float sr, float f0, float Q=0.707f) {
    float w0 = 2.0f * float(kPi) * (f0 / sr);
    float cw = std::cos(w0), sw = std::sin(w0);
    float alpha = sw/(2.0f*Q);

    float b0n = (1 - cw)/2;
    float b1n = 1 - cw;
    float b2n = (1 - cw)/2;
    float a0n = 1 + alpha;
    float a1n = -2*cw;
    float a2n = 1 - alpha;

    b0 = b0n/a0n; b1 = b1n/a0n; b2 = b2n/a0n;
    a1 = a1n/a0n; a2 = a2n/a0n;
  }
};

inline float tubeSat(float x, float drive) {
  float y = x * drive;
  float asym = 0.15f;
  y = y + asym * y * y;
  y = std::tanh(y);
  y -= 0.02f * asym;
  return y;
}

} // namespace SvenderBass::DSP
