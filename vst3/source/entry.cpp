#include "cids.h"
#include "version.h"
#include "CarrierFieldController.h"
#include "CarrierFieldProcessor.h"

#include "public.sdk/source/main/pluginfactory.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

bool InitModule()   { return true; }
bool DeinitModule() { return true; }

BEGIN_FACTORY_DEF (stringCompanyName, stringCompanyWeb, stringCompanyEmail)

    DEF_CLASS2 (INLINE_UID_FROM_FUID (kCarrierFieldProcessorUID),
                PClassInfo::kManyInstances,
                kVstAudioEffectClass,
                stringPluginName,
                Vst::kDistributable,
                Vst::PlugType::kInstrumentSynth,
                FULL_VERSION_STR,
                kVstVersionString,
                cosmic::vst3::CarrierFieldProcessor::createInstance)

    DEF_CLASS2 (INLINE_UID_FROM_FUID (kCarrierFieldControllerUID),
                PClassInfo::kManyInstances,
                kVstComponentControllerClass,
                stringPluginName " Controller",
                0,
                "",
                FULL_VERSION_STR,
                kVstVersionString,
                cosmic::vst3::CarrierFieldController::createInstance)

END_FACTORY
