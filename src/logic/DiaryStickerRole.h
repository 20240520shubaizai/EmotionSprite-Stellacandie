#pragma once
#include "../data/Records.h"

class DiaryStickerRole
{
public:
    static QList<DiaryStickerRecord> generate(const QDate &date, const PetStateRecord &state,
                                               const DreamRecord *dream = nullptr);
};
