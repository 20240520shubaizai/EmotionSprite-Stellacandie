#pragma once
#include "../data/DataRepository.h"
#include <QAbstractListModel>
class MemoryListModel final:public QAbstractListModel{public: enum Role{CategoryRole=Qt::UserRole+1,SubjectRole,ContentRole,ImportanceRole,QuestionRole};
explicit MemoryListModel(DataRepository*r,QObject*p=nullptr):QAbstractListModel(p),repo(r){} int rowCount(const QModelIndex&p={})const override{return p.isValid()?0:items.size();}
QVariant data(const QModelIndex&i,int role)const override;QHash<int,QByteArray> roleNames()const override;void refresh();private:DataRepository*repo;QList<MemoryRecord>items;};
