#include <memory>
#include <string>
#include <optional>

#include <mw/database.hpp>
#include <mw/error.hpp>
#include <mw/utils.hpp>

#include "data.hpp"

mw::E<std::unique_ptr<DataSourceSQLite>>
DataSourceSQLite::fromFile(const std::string& db_file)
{
    auto data_source = std::make_unique<DataSourceSQLite>();

    // Perform schema upgrade here.
    //
    // data_source->upgradeSchema1To2();

    // Update this line when schema updates.
    DO_OR_RETURN(data_source->setSchemaVersion(1));
    ASSIGN_OR_RETURN(data_source->db, mw::SQLite::connectFile(db_file));
    DO_OR_RETURN(data_source->db->execute(
        "CREATE TABLE IF NOT EXISTS Saves "
        "(id INTEGER PRIMARY KEY AUTOINCREMENT, time INTEGER, save TEXT);"));
    return data_source;
}

mw::E<std::unique_ptr<DataSourceSQLite>> DataSourceSQLite::newFromMemory()
{
    return fromFile(":memory:");
}

mw::E<int64_t> DataSourceSQLite::getSchemaVersion() const
{
    return db->evalToValue<int64_t>("PRAGMA user_version;");
}

mw::E<void> DataSourceSQLite::storeSave(const std::string& save) const
{
    ASSIGN_OR_RETURN(auto sql, db->statementFromStr(
        "INSERT INTO Saves (time, save) VALUES (?, ?);"));
    DO_OR_RETURN(sql.bind(mw::timeToSeconds(mw::Clock::now()), save));
    DO_OR_RETURN(db->execute(std::move(sql)));
    return {};
}

mw::E<std::optional<std::string>> DataSourceSQLite::loadLatestSave() const
{
    ASSIGN_OR_RETURN(auto rows, (db->eval<std::string>(
        "SELECT save FROM Saves ORDER BY id DESC LIMIT 1;")));
    if(rows.empty())
    {
        return std::nullopt;
    }
    return std::get<0>(rows[0]);
}

mw::E<void> DataSourceSQLite::setSchemaVersion(int64_t v) const
{
    return db->execute(std::format("PRAGMA user_version = {};", v));
}
