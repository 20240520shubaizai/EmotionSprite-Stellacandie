#pragma once
#include "../Records.h"
#include <QList>
class DiaryRepository { public: virtual ~DiaryRepository()=default; virtual bool saveDiaryEntry(const QDate&,const QString&)=0; virtual QList<DiaryEntryRecord> loadDiaryEntries()const=0; virtual bool addDiarySticker(const DiaryStickerRecord&)=0; virtual QList<DiaryStickerRecord> loadDiaryStickers(const QDate&)const=0; };
