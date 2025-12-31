#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/base/fstrdefs.h"

#include "ids.h"

namespace SvenderBass {

class Controller final : public Steinberg::Vst::EditController {
public:
  Controller() = default;

  static Steinberg::FUnknown* createInstance(void*)
  {
    return (Steinberg::Vst::IEditController*)new Controller();
  }

  Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) override;

  // VST3 UI factory hook ("editor" view)
  Steinberg::IPlugView* PLUGIN_API createView(const char* name) override;
};

} // namespace SvenderBass
