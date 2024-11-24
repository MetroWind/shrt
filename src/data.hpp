#pragma once

#include <memory>
#include <string>
#include <optional>

#include <mw/database.hpp>
#include <mw/error.hpp>

class DataSourceInterface
{
public:
    virtual ~DataSourceInterface() = default;

    // The schema version is the version of the database. It starts
    // from 1. Every time the schema change, someone should increase
    // this number by 1, manually, by hand. The intended use is to
    // help with database migration.
    virtual mw::E<int64_t> getSchemaVersion() const = 0;

    virtual mw::E<void> storeSave(const std::string& save) const = 0;
    virtual mw::E<std::optional<std::string>> loadLatestSave() const = 0;

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
    mw::E<void> storeSave(const std::string& save) const override;
    mw::E<std::optional<std::string>> loadLatestSave() const override;

    // Do not use.
    DataSourceSQLite() = default;

protected:
    mw::E<void> setSchemaVersion(int64_t v) const override;

private:
    std::unique_ptr<mw::SQLite> db;
};
