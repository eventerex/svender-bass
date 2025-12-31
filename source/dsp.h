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

struct AttackReleaseEnvelope {
  float aA = 0.0f;
  float aR = 0.0f;
  float y = 0.0f;

  void setTimesMs(float sr, float attackMs, float releaseMs) {
    const float a = std::max(0.001f, attackMs) * 0.001f;
    const float r = std::max(0.001f, releaseMs) * 0.001f;
    aA = std::exp(-1.0f / (a * sr));
    aR = std::exp(-1.0f / (r * sr));
  }

  float process(float x) {
    x = std::fabs(x);
    if (x > y)
      y = aA * y + (1.0f - aA) * x;
    else
      y = aR * y + (1.0f - aR) * x;
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

struct Oversampler2x {
  float prev = 0.0f;
  Biquad lpUp;
  Biquad lpDown;

  void setSampleRate(float sr) {
    const float osSr = sr * 2.0f;
    const float cutoff = sr * 0.45f;
    lpUp.setLP(osSr, cutoff, 0.707f);
    lpDown.setLP(osSr, cutoff, 0.707f);
  }

  void reset(float v = 0.0f) {
    prev = v;
    lpUp.reset();
    lpDown.reset();
  }

  void upsample(float x, float& y0, float& y1) {
    y0 = x;
    y1 = 0.5f * (x + prev);
    prev = x;
    y0 = lpUp.process(y0);
    y1 = lpUp.process(y1);
  }

  float downsample(float y0, float y1) {
    float f0 = lpDown.process(y0);
    float f1 = lpDown.process(y1);
    (void)f1;
    return f0;
  }
};

struct Oversampler4x {
  Oversampler2x s1;
  Oversampler2x s2;

  void setSampleRate(float sr) {
    s1.setSampleRate(sr);
    s2.setSampleRate(sr * 2.0f);
  }

  void reset(float v = 0.0f) {
    s1.reset(v);
    s2.reset(v);
  }

  void upsample(float x, float* y4) {
    float t0 = 0.0f;
    float t1 = 0.0f;
    s1.upsample(x, t0, t1);
    s2.upsample(t0, y4[0], y4[1]);
    s2.upsample(t1, y4[2], y4[3]);
  }

  float downsample(const float* y4) {
    float t0 = s2.downsample(y4[0], y4[1]);
    float t1 = s2.downsample(y4[2], y4[3]);
    return s1.downsample(t0, t1);
  }
};

inline float tubeStage(float x, float drive, float bias) {
  float y = x * drive + bias;
  y = std::tanh(y);
  y -= std::tanh(bias);
  return y;
}

inline float tubeSatMulti(float x, float drive) {
  float d1 = drive;
  float d2 = drive * 0.7f + 0.3f;
  float d3 = drive * 0.5f + 0.5f;

  float y = x;
  y = tubeStage(y, d1, 0.08f);
  y = tubeStage(y, d2, -0.04f);
  y = tubeStage(y, d3, 0.02f);
  return y;
}

inline float tubeSat(float x, float drive) {
  return tubeSatMulti(x, drive);
}

} // namespace SvenderBass::DSP
