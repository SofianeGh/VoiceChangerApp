// "VoiceFemmePlugin.cpp" : voix de femme adulte
#include "VoiceType.h"

class VoiceFemmeDSP : public VoiceTypeDSP {
public:
    VoiceFemmeDSP() : VoiceTypeDSP(210.0f, 1.17f) {}
};

VC_DEFINE_PLUGIN(VoiceFemmeDSP, "Voix femme")
