#pragma once

#include <memory>
#include <string>
#include <optional>
#include <vector>

#include <mw/database.hpp>
#include <mw/error.hpp>

struct ShortLink
{
    enum Type { NORMAL = 1, REGEXP };
    int64_t id;
    // This is the name of the shortened link; the 1st level path
    // after the domain in the URL, without any parameter or fragment.
    std::string shortcut;
    // The original URL that is shortened.
    std::string original_url;
    Type type;
    std::string user_id;
    std::string user_name;
    uint64_t visits;
    mw::Time time_creation;

    static std::optional<Type> typeFromInt(int t);
};

class DataSourceInterface
{
public:
    virtual ~DataSourceInterface() = default;

    // The schema version is the version of the database. It starts
    // from 1. Every time the schema change, someone should increase
    // this number by 1, manually, by hand. The intended use is to
    // help with database migration.
    virtual mw::E<int64_t> getSchemaVersion() const = 0;

    // Add a link to the database. This requires the user_id,
    // shortcut, original_url and type to be filled in “link”.
    virtual mw::E<void> addLink(ShortLink&& link) const = 0;
    virtual mw::E<std::optional<ShortLink>>
    findLinkByShortcut(const std::string& shortcut) const = 0;
    virtual mw::E<std::optional<ShortLink>>
    findLinkFromRegexpLinks(const std::string& shortcut) const = 0;
    virtual mw::E<std::vector<ShortLink>>
    getAllLinks(const std::string& user_id) const = 0;
    virtual mw::E<std::optional<ShortLink>> getLink(int64_t id) const = 0;
    virtual mw::E<void> removeLink(int64_t id) const = 0;

protected:
    virtual mw::E<void> setSchemaVersion(int64_t v) const = 0;
};

class DataSourceSQLite : public DataSourceInterface
{
public:
    explicit DataSourceSQLite(std::unique_ptr<mw::SQLite> conn)
            : db(std::move(conn)) {}
    ~DataSourceSQLite() override = default;

    static mw::E<std::unique_ptr<DataSourceSQLite>>
    fromFile(const std::string& db_file);
    static mw::E<std::unique_ptr<DataSourceSQLite>> newFromMemory();

    mw::E<int64_t> getSchemaVersion() const override;

    mw::E<void> addLink(ShortLink&& link) const override;
    mw::E<std::optional<ShortLink>>
    findLinkByShortcut(const std::string& shortcut) const override;
    mw::E<std::optional<ShortLink>>
    findLinkFromRegexpLinks(const std::string& shortcut) const override;
    mw::E<std::vector<ShortLink>> getAllLinks(const std::string& user_id) const
        override;
    mw::E<std::optional<ShortLink>> getLink(int64_t id) const override;
    mw::E<void> removeLink(int64_t id) const override;

    // Do not use.
    DataSourceSQLite() = default;

protected:
    mw::E<void> setSchemaVersion(int64_t v) const override;

private:
    std::unique_ptr<mw::SQLite> db;
};
