#ifndef CONNECTION_H
#define CONNECTION_H

#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QDir>

/*
    This file defines a helper function to open a connection to an
    in-memory SQLITE database and to create a test table.
*/
static bool createConnection(QDir dir)
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dir.filePath("Exclude.sqlite"));
    if (!db.open()) {
        QMessageBox::critical(nullptr, QObject::tr("Cannot open database"),
            QObject::tr("Unable to establish a database connection.\n"
                        "This example needs SQLite support. Please read "
                        "the Qt SQL driver documentation for information how "
                        "to build it.\n\n"
                        "Click Cancel to exit."), QMessageBox::Cancel);
        return false;
    }

    QSqlQuery query;
    query.exec("create table if not exists trace (id integer primary key, "
                                                "filename text,"
                                                "datetime bigint,"
                                                "exclusion int)");
    return true;
}

#endif
