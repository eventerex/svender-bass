#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"   // Steinberg::Vst::EditorView, EditController
#include "pluginterfaces/gui/iplugview.h"              // IPlugView
#include "pluginterfaces/base/fstrdefs.h"              // FIDString

namespace SvenderBass {

class Editor final : public Steinberg::Vst::EditorView {
public:
  explicit Editor(Steinberg::Vst::EditController* controller);
  ~Editor() = default;

  Steinberg::tresult PLUGIN_API attached(void* parent, Steinberg::FIDString type) override;
  Steinberg::tresult PLUGIN_API removed() override;
  Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect* newSize) override;
  Steinberg::tresult PLUGIN_API getSize(Steinberg::ViewRect* size) override;

private:
  Steinberg::ViewRect rect_ {0, 0, 1200, 450};
  Steinberg::Vst::EditController* controller_ = nullptr;
};

} // namespace SvenderBass

