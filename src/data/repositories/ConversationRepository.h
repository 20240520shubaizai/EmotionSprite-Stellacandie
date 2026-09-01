#pragma once
#include "../Records.h"
#include <QList>
class ConversationRepository { public: virtual ~ConversationRepository()=default; virtual QList<ChatMessageRecord> loadRecentMessages(int limit=100)const=0; virtual ChatMessageRecord addMessage(const QString&,const QString&)=0; };
