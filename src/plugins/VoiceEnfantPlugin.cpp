// "VoiceEnfantPlugin.cpp" : voix d'enfant
#include "VoiceType.h"

class VoiceEnfantDSP : public VoiceTypeDSP {
public:
    VoiceEnfantDSP() : VoiceTypeDSP(290.0f, 1.35f) {}
};

VC_DEFINE_PLUGIN(VoiceEnfantDSP, "Voix enfant")
