#pragma once

#include "repositories/CollectionRepository.h"
#include "repositories/ConversationRepository.h"
#include "repositories/DiaryRepository.h"
#include "repositories/DreamRepository.h"
#include "repositories/MemoryRepository.h"
#include "repositories/MorningLollipopRepository.h"
#include "repositories/PetStateRepository.h"
#include "repositories/ReminderRepository.h"
#include "repositories/SyncRepository.h"

// Storage implementations may aggregate all domains, while consumers must depend
// on the smallest repository interface that satisfies their use case.
class DataRepository : public ConversationRepository,
                       public PetStateRepository,
                       public DiaryRepository,
                       public MemoryRepository,
                       public ReminderRepository,
                       public DreamRepository,
                       public CollectionRepository,
                       public MorningLollipopRepository,
                       public SyncRepository
{
public:
    ~DataRepository() override = default;
};
