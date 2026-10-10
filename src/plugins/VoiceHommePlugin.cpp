// "VoiceHommePlugin.cpp" : voix grave d'homme adulte
#include "VoiceType.h"

class VoiceHommeDSP : public VoiceTypeDSP {
public:
    VoiceHommeDSP() : VoiceTypeDSP(100.0f, 0.95f) {}
};

VC_DEFINE_PLUGIN(VoiceHommeDSP, "Voix homme")
