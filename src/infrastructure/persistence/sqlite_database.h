#pragma once

#include <QSqlDatabase>
#include <QString>

class QSqlQuery;

namespace infrastructure {

class SqliteDatabase final
{
public:
    explicit SqliteDatabase(QString filePath);
    ~SqliteDatabase();

    SqliteDatabase(const SqliteDatabase&) = delete;
    SqliteDatabase& operator=(const SqliteDatabase&) = delete;

    void open();
    const QSqlDatabase& connection() const noexcept;
    void execute(QSqlQuery& query) const;

private:
    void migrate();
    void close();

    QString filePath_;
    QString connectionName_;
    QSqlDatabase database_;
    bool opened_{};
};

} // namespace infrastructure
