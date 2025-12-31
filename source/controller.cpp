#include "controller.h"
#include "editor.h"

#include "public.sdk/source/vst/vstparameters.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace SvenderBass {

tresult PLUGIN_API Controller::initialize(FUnknown* context)
{
    tresult res = EditController::initialize(context);
    if (res != kResultOk)
        return res;

    parameters.addParameter(STR16("Input"),  STR16(""), 0, 0.50, ParameterInfo::kCanAutomate, kParamInputGain);
    parameters.addParameter(STR16("Bass"),   STR16(""), 0, 0.50, ParameterInfo::kCanAutomate, kParamBass);
    parameters.addParameter(STR16("Mid"),    STR16(""), 0, 0.50, ParameterInfo::kCanAutomate, kParamMid);
    parameters.addParameter(STR16("Treble"), STR16(""), 0, 0.50, ParameterInfo::kCanAutomate, kParamTreble);

    parameters.addParameter(STR16("Mid Freq"), STR16(""), 4, 0.50,
                            ParameterInfo::kCanAutomate, kParamMidFreq);

    parameters.addParameter(STR16("Drive"),  STR16(""), 0, 0.30,
                            ParameterInfo::kCanAutomate, kParamDrive);
    parameters.addParameter(STR16("Output"), STR16(""), 0, 0.70,
                            ParameterInfo::kCanAutomate, kParamOutput);

    parameters.addParameter(STR16("Ultra Low"),  STR16(""), 1, 0.0,
                            ParameterInfo::kCanAutomate, kParamUltraLow);
    parameters.addParameter(STR16("Ultra High"), STR16(""), 1, 0.0,
                            ParameterInfo::kCanAutomate, kParamUltraHigh);

    return kResultOk;
}

IPlugView* PLUGIN_API Controller::createView(FIDString name)
{
    if (name && strcmp(name, ViewType::kEditor) == 0)
    {
        return new Editor(this);
    }
    return nullptr;
}

} // namespace SvenderBass
