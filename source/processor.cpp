#include "processor.h"
#include "controller.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace SvenderBass {

Processor::Processor() {
  setControllerClass(kControllerUID);
}

tresult PLUGIN_API Processor::initialize(FUnknown* context) {
  tresult res = AudioEffect::initialize(context);
  if (res != kResultOk) return res;

  addAudioInput(STR16("Stereo In"),  SpeakerArr::kStereo);
  addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);

  return kResultOk;
}

tresult PLUGIN_API Processor::setupProcessing(ProcessSetup& setup) {
  sampleRate_ = setup.sampleRate;

  inGainSm_.setTimeMs((float)sampleRate_, 15.0f);
  outGainSm_.setTimeMs((float)sampleRate_, 15.0f);
  driveSm_.setTimeMs((float)sampleRate_, 25.0f);

  inGainSm_.reset(1.0f);
  outGainSm_.reset(1.0f);
  driveSm_.reset(1.0f);

  envL_.setTimeMs((float)sampleRate_, 30.0f);
  envR_.setTimeMs((float)sampleRate_, 30.0f);
  envL_.reset();
  envR_.reset();
  lastEnv_ = 0.0f;

  updateFilters();
  return AudioEffect::setupProcessing(setup);
}

tresult PLUGIN_API Processor::setActive(TBool state) {
  if (state) {
    bassL_.reset(); bassR_.reset();
    midL_.reset();  midR_.reset();
    trebL_.reset(); trebR_.reset();
    postLowL_.reset();  postLowR_.reset();
    postHighL_.reset(); postHighR_.reset();
    cabHpL_.reset(); cabHpR_.reset();
    cabLpL_.reset(); cabLpR_.reset();
    ultraLowL_.reset(); ultraLowR_.reset();
    ultraLowCutL_.reset(); ultraLowCutR_.reset();
    ultraHighL_.reset(); ultraHighR_.reset();
    envL_.reset(); envR_.reset();
    lastEnv_ = 0.0f;
  }
  return AudioEffect::setActive(state);
}

static float midFreqFromSwitch(int pos) {
  switch (pos) {
    case 0: return 220.0f;
    case 1: return 450.0f;
    case 2: return 800.0f;
    case 3: return 1600.0f;
    default:return 3000.0f;
  }
}

void Processor::updateFilters() {
  const float sr = (float)sampleRate_;
  auto mapDb = [](float norm, float maxAbsDb) { return (norm * 2.0f - 1.0f) * maxAbsDb; };
  auto mapDbAsym = [](float norm, float maxPosDb, float maxNegDb) {
    if (norm >= 0.5f)
      return ((norm - 0.5f) / 0.5f) * maxPosDb;
    return ((norm - 0.5f) / 0.5f) * maxNegDb;
  };

  const float bassDb = mapDb(pBass_,   12.0f);
  const float midDb  = mapDbAsym(pMid_,    10.0f, 20.0f);
  const float treDb  = mapDbAsym(pTreble_, 15.0f, 20.0f);

  bassL_.setLowShelf(sr, 40.0f, bassDb, 0.707f);
  bassR_.setLowShelf(sr, 40.0f, bassDb, 0.707f);

  float mf = midFreqFromSwitch(pMidFreq_);
  midL_.setPeaking(sr, mf, midDb, 0.9f);
  midR_.setPeaking(sr, mf, midDb, 0.9f);

  trebL_.setHighShelf(sr, 4000.0f, treDb, 0.707f);
  trebR_.setHighShelf(sr, 4000.0f, treDb, 0.707f);

  cabHpL_.setHP(sr, 55.0f, 0.707f);
  cabHpR_.setHP(sr, 55.0f, 0.707f);
  cabLpL_.setLP(sr, 5200.0f, 0.707f);
  cabLpR_.setLP(sr, 5200.0f, 0.707f);

  const float ulDb = pUltraLow_  ? +2.0f : 0.0f;
  const float ulCutDb = pUltraLow_ ? -10.0f : 0.0f;
  const float uhDb = pUltraHigh_ ? +9.0f : 0.0f;

  ultraLowL_.setLowShelf(sr, 40.0f, ulDb, 0.707f);
  ultraLowR_.setLowShelf(sr, 40.0f, ulDb, 0.707f);
  ultraLowCutL_.setPeaking(sr, 500.0f, ulCutDb, 0.9f);
  ultraLowCutR_.setPeaking(sr, 500.0f, ulCutDb, 0.9f);

  ultraHighL_.setHighShelf(sr, 8000.0f, uhDb, 0.707f);
  ultraHighR_.setHighShelf(sr, 8000.0f, uhDb, 0.707f);

}

void Processor::applyParameterChanges(IParameterChanges* changes) {
  if (!changes) return;

  int32 count = changes->getParameterCount();
  bool needFilterUpdate = false;

  for (int32 i = 0; i < count; ++i) {
    IParamValueQueue* q = changes->getParameterData(i);
    if (!q) continue;

    Steinberg::Vst::ParamID pid = q->getParameterId();
    int32 points = q->getPointCount();
    if (points <= 0) continue;

    int32 sampleOffset = 0;
    ParamValue value = 0.0;
    if (q->getPoint(points - 1, sampleOffset, value) != kResultOk) continue;

    float v = (float)value;

    switch (pid) {
      case kParamInputGain: pInputGain_ = v; break;
      case kParamBass:      pBass_      = v; needFilterUpdate = true; break;
      case kParamMid:       pMid_       = v; needFilterUpdate = true; break;
      case kParamTreble:    pTreble_    = v; needFilterUpdate = true; break;
      case kParamMidFreq:   pMidFreq_   = (int)std::lround(v * 4.0f); needFilterUpdate = true; break;
      case kParamDrive:     pDrive_     = v; break;
      case kParamOutput:    pOutput_    = v; break;
      case kParamUltraLow:  pUltraLow_  = (v >= 0.5f); needFilterUpdate = true; break;
      case kParamUltraHigh: pUltraHigh_ = (v >= 0.5f); needFilterUpdate = true; break;
      default: break;
    }
  }

  if (needFilterUpdate) updateFilters();
}

tresult PLUGIN_API Processor::process(ProcessData& data) {
  applyParameterChanges(data.inputParameterChanges);

  if (data.numInputs == 0 || data.numOutputs == 0) return kResultOk;
  if (data.inputs[0].numChannels < 2 || data.outputs[0].numChannels < 2) return kResultOk;
  if (data.numSamples <= 0) return kResultOk;

  float** in  = data.inputs[0].channelBuffers32;
  float** out = data.outputs[0].channelBuffers32;
  if (!in || !out) return kResultOk;

  float inDb  = (pInputGain_ * 2.0f - 1.0f) * 24.0f;
  float outDb = (pOutput_    * 2.0f - 1.0f) * 24.0f;
  float inLinTarget  = DSP::dbToLin(inDb);
  float outLinTarget = DSP::dbToLin(outDb);

  float driveTarget = 1.0f + pDrive_ * 19.0f;

  float envForDrive = DSP::clamp(lastEnv_ * 3.0f, 0.0f, 1.0f);
  float dynamicDrive = 1.0f + 8.0f * envForDrive;
  float driveEffectiveTarget = driveTarget * dynamicDrive;

  float driveNorm = DSP::clamp((driveEffectiveTarget - 1.0f) / 12.0f, 0.0f, 1.0f);
  float lowTightenDb = -3.0f * driveNorm;
  float highSoftenDb = -4.0f * driveNorm;

  postLowL_.setLowShelf((float)sampleRate_, 40.0f, lowTightenDb, 0.707f);
  postLowR_.setLowShelf((float)sampleRate_, 40.0f, lowTightenDb, 0.707f);
  postHighL_.setHighShelf((float)sampleRate_, 4000.0f, highSoftenDb, 0.707f);
  postHighR_.setHighShelf((float)sampleRate_, 4000.0f, highSoftenDb, 0.707f);

  float envSum = 0.0f;

  for (int32 n = 0; n < data.numSamples; ++n) {
    float inG  = inGainSm_.process(inLinTarget);
    float outG = outGainSm_.process(outLinTarget);
    float drv  = driveSm_.process(driveEffectiveTarget);

    float xL = in[0][n] * inG;
    float xR = in[1][n] * inG;

    xL = ultraLowL_.process(xL);
    xL = ultraLowCutL_.process(xL);
    xL = ultraHighL_.process(xL);

    xL = bassL_.process(xL);
    xL = midL_.process(xL);
    xL = trebL_.process(xL);

    xR = ultraLowR_.process(xR);
    xR = ultraLowCutR_.process(xR);
    xR = ultraHighR_.process(xR);

    xR = bassR_.process(xR);
    xR = midR_.process(xR);
    xR = trebR_.process(xR);

    float eL = envL_.process(xL);
    float eR = envR_.process(xR);
    envSum += 0.5f * (eL + eR);

    xL = DSP::tubeSat(xL, drv);
    xR = DSP::tubeSat(xR, drv);

    xL = postLowL_.process(xL);
    xL = postHighL_.process(xL);
    xR = postLowR_.process(xR);
    xR = postHighR_.process(xR);

    xL = cabHpL_.process(xL);
    xL = cabLpL_.process(xL);
    xR = cabHpR_.process(xR);
    xR = cabLpR_.process(xR);

    out[0][n] = xL * outG;
    out[1][n] = xR * outG;
  }

  lastEnv_ = envSum / (float)std::max<int32>(1, data.numSamples);
  return kResultOk;
}

} // namespace SvenderBass
