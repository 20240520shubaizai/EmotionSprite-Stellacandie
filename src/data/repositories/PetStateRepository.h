#pragma once
#include "../Records.h"
class PetStateRepository { public: virtual ~PetStateRepository()=default; virtual PetStateRecord loadPetState()const=0; virtual bool savePetState(const PetStateRecord&)=0; };
