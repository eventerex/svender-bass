#pragma once
#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace SvenderBass {

// NOTE: Generate your own GUIDs for real work.
static const Steinberg::FUID kProcessorUID (0x2F0B1C91, 0x2A8F4B7D, 0x9E6D58C2, 0x4C5E18A1);
static const Steinberg::FUID kControllerUID(0x8A1D2B7E, 0x9C3A4F10, 0xB1E2D3C4, 0x55667788);

enum ParamID : Steinberg::Vst::ParamID {
  kParamInputGain = 0,
  kParamBass      = 1,
  kParamMid       = 2,
  kParamTreble    = 3,
  kParamMidFreq   = 4, // 0..4 -> 220/450/800/1600/3000
  kParamDrive     = 5,
  kParamOutput    = 6,
  kParamUltraLow  = 7,
  kParamUltraHigh = 8,
};

} // namespace SvenderBass
