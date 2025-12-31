#include "public.sdk/source/main/pluginfactory.h"
#include "processor.h"
#include "controller.h"
#include "ids.h"
#include "version.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

BEGIN_FACTORY_DEF(PLUGIN_VENDOR, "https://example.invalid", "mailto:dev@example.invalid")

DEF_CLASS2(INLINE_UID_FROM_FUID(SvenderBass::kProcessorUID),
           PClassInfo::kManyInstances,
           kVstAudioEffectClass,
           PLUGIN_NAME,
           Vst::kDistributable,
           "Fx",
           PLUGIN_VERSION_STR,
           kVstVersionString,
           SvenderBass::Processor::createInstance)

DEF_CLASS2(INLINE_UID_FROM_FUID(SvenderBass::kControllerUID),
           PClassInfo::kManyInstances,
           kVstComponentControllerClass,
           PLUGIN_NAME " Controller",
           0,
           "",
           PLUGIN_VERSION_STR,
           kVstVersionString,
           SvenderBass::Controller::createInstance)

END_FACTORY
