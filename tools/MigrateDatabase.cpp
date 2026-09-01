#include <QCoreApplication>
#include <QTextStream>
#include "src/StorageService.h"

int main(int argc,char **argv)
{
    QCoreApplication app(argc,argv);
    if(app.arguments().size()!=2){QTextStream(stderr)<<"usage: MigrateDatabase <sqlite-file>\n";return 2;}
    StorageService storage(app.arguments().at(1));
    if(!storage.initialize()){QTextStream(stderr)<<storage.lastError()<<'\n';return 1;}
    QTextStream(stdout)<<"schema_version="<<storage.schemaVersion()<<"\n";
    return 0;
}
