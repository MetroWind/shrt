#include <memory>
#include <string>
#include <optional>
#include <expected>
#include <regex>
#include <utility>

#include <mw/database.hpp>
#include <mw/error.hpp>
#include <mw/utils.hpp>

#include "data.hpp"

namespace
{

mw::E<ShortLink> rowToLink(
    std::tuple<int64_t, int64_t, std::string, std::string, std::string,
    int, int64_t>& row)
{
    ShortLink link;
    link.id = std::get<0>(row);
    link.time_creation = mw::secondsToTime(std::get<1>(row));
    link.user_id = std::move(std::get<2>(row));
    link.shortcut = std::move(std::get<3>(row));
    link.original_url = std::move(std::get<4>(row));
    auto type = ShortLink::typeFromInt(std::get<5>(row));
    if(!type.has_value())
    {
        return std::unexpected(mw::runtimeError("Invalid link type"));
    }
    link.type = *type;
    link.visits = std::get<6>(row);
    return link;
}

} // namespace

std::optional<ShortLink::Type> ShortLink::typeFromInt(int t)
{
    switch(t)
    {
    case NORMAL:
        return NORMAL;
    case REGEXP:
        return REGEXP;
    default:
        std::unreachable();
    }
}

mw::E<std::unique_ptr<DataSourceSQLite>>
DataSourceSQLite::fromFile(const std::string& db_file)
{
    auto data_source = std::make_unique<DataSourceSQLite>();
    ASSIGN_OR_RETURN(data_source->db, mw::SQLite::connectFile(db_file));

    // Perform schema upgrade here.
    //
    // data_source->upgradeSchema1To2();

    // Update this line when schema updates.
    DO_OR_RETURN(data_source->setSchemaVersion(1));
    DO_OR_RETURN(data_source->db->execute(
        "CREATE TABLE IF NOT EXISTS Links "
        "(id INTEGER PRIMARY KEY, time_creation INTEGER, user_id TEXT,"
        " shortcut TEXT UNIQUE, original_url TEXT, type INTEGER,"
        " visits INTEGER);"));
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

mw::E<void> DataSourceSQLite::addLink(ShortLink&& link) const
{
    ASSIGN_OR_RETURN(auto statement, db->statementFromStr(
        "INSERT INTO Links (time_creation, user_id, shortcut, original_url,"
        " type, visits) VALUES (?, ?, ?, ?, ?, 0);"));
    DO_OR_RETURN((statement.bind<int64_t, std::string, std::string,
                  std::string&, int>(
        mw::timeToSeconds(mw::Clock::now()), link.user_id, link.shortcut,
        link.original_url, link.type)));
    return db->execute(std::move(statement));
}

mw::E<std::optional<ShortLink>> DataSourceSQLite::findLinkByShortcut(
    const std::string& shortcut) const
{
    ASSIGN_OR_RETURN(auto statement, db->statementFromStr(
        "SELECT id, time_creation, user_id, shortcut, original_url, type,"
        " visits FROM Links WHERE shortcut = ?;"));
    DO_OR_RETURN(statement.bind<std::string>(shortcut));
    ASSIGN_OR_RETURN(
        auto rows, (db->eval<int64_t, int64_t, std::string, std::string,
                    std::string, int, int64_t>(std::move(statement))));
    if(rows.empty())
    {
        return std::nullopt;
    }
    return rowToLink(rows[0]);
}

mw::E<std::optional<ShortLink>> DataSourceSQLite::findLinkFromRegexpLinks(
    const std::string& shortcut) const
{
    ASSIGN_OR_RETURN(auto statement, db->statementFromStr(
        "SELECT id, time_creation, user_id, shortcut, original_url, type,"
        " visits FROM Links WHERE type = ?;"));
    DO_OR_RETURN(statement.bind<int>(ShortLink::REGEXP));
    ASSIGN_OR_RETURN(
        auto rows, (db->eval<int64_t, int64_t, std::string, std::string,
                    std::string, int, int64_t>(std::move(statement))));
    for(auto& row: std::move(rows))
    {
        ASSIGN_OR_RETURN(ShortLink link, rowToLink(row));
        // Ensure it’s a full match
        if(std::regex_match(shortcut, std::regex(link.shortcut)))
        {
            return link;
        }
    }
    return std::nullopt;
}

mw::E<std::vector<ShortLink>> DataSourceSQLite::getAllLinks(
    const std::string& user_id) const
{
    ASSIGN_OR_RETURN(auto statement, db->statementFromStr(
        "SELECT id, time_creation, user_id, shortcut, original_url, type,"
        " visits FROM Links WHERE user_id = ?;"));
    DO_OR_RETURN(statement.bind<std::string>(user_id));
    ASSIGN_OR_RETURN(auto rows, (db->eval<int64_t, int64_t, std::string, std::string,
                            std::string, int, int64_t>(std::move(statement))));
    std::vector<ShortLink> links;
    links.reserve(rows.size());
    for(auto& row: std::move(rows))
    {
        ASSIGN_OR_RETURN(links.emplace_back(), rowToLink(row));
    }
    return links;
}

mw::E<std::optional<ShortLink>> DataSourceSQLite::getLink(int64_t id) const
{
    ASSIGN_OR_RETURN(auto statement, db->statementFromStr(
        "SELECT id, time_creation, user_id, shortcut, original_url, type,"
        " visits FROM Links WHERE id = ?;"));
    DO_OR_RETURN(statement.bind<int64_t>(id));
    ASSIGN_OR_RETURN(
        auto rows, (db->eval<int64_t, int64_t, std::string, std::string,
                    std::string, int, int64_t>(std::move(statement))));
    if(rows.empty())
    {
        return std::nullopt;
    }
    ASSIGN_OR_RETURN(ShortLink link, rowToLink(rows[0]));
    return link;
}

mw::E<void> DataSourceSQLite::removeLink(int64_t id) const
{
    ASSIGN_OR_RETURN(auto statement, db->statementFromStr(
        "DELETE FROM Links WHERE id = ?;"));
    DO_OR_RETURN(statement.bind<int>(id));
    return db->execute(std::move(statement));
}

mw::E<void> DataSourceSQLite::setSchemaVersion(int64_t v) const
{
    return db->execute(std::format("PRAGMA user_version = {};", v));
}
