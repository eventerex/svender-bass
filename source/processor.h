#pragma once
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "ids.h"
#include "dsp.h"

namespace SvenderBass {

class Processor final : public Steinberg::Vst::AudioEffect {
public:
  Processor();
  static Steinberg::FUnknown* createInstance(void*) { return (Steinberg::Vst::IAudioProcessor*)new Processor(); }

  Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) override;
  Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool state) override;
  Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup& setup) override;
  Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data) override;

private:
  void applyParameterChanges(Steinberg::Vst::IParameterChanges* changes);
  void updateFilters();

  double sampleRate_ = 44100.0;

  float pInputGain_ = 0.5f;
  float pBass_      = 0.5f;
  float pMid_       = 0.5f;
  float pTreble_    = 0.5f;
  int   pMidFreq_   = 2;
  float pDrive_     = 0.3f;
  float pOutput_    = 0.7f;
  bool pUltraLow_ = false;
  bool pUltraHigh_ = false;


  DSP::Smoother inGainSm_, outGainSm_, driveSm_;
  DSP::EnvelopeFollower envL_, envR_;
  float lastEnv_ = 0.0f;

  DSP::Biquad bassL_, bassR_;
  DSP::Biquad midL_, midR_;
  DSP::Biquad trebL_, trebR_;

  DSP::Biquad postLowL_, postLowR_;
  DSP::Biquad postHighL_, postHighR_;

  DSP::Biquad cabHpL_, cabHpR_;
  DSP::Biquad cabLpL_, cabLpR_;

  DSP::Biquad ultraLowL_, ultraLowR_;
  DSP::Biquad ultraLowCutL_, ultraLowCutR_;
  DSP::Biquad ultraHighL_, ultraHighR_;

};

} // namespace SvenderBass
