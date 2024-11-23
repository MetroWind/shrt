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
    virtual mw::E<void> storeSave(const std::string& save) const = 0;
    virtual mw::E<std::optional<std::string>> loadLatestSave() const = 0;
};

class DataSourceSQLite : public DataSourceInterface
{
public:
    explicit DataSourceSQLite(std::unique_ptr<mw::SQLite> conn)
            : db(std::move(conn)) {}

    static mw::E<std::unique_ptr<DataSourceSQLite>>
    fromFile(const std::string& db_file);
    static mw::E<std::unique_ptr<DataSourceSQLite>> newFromMemory();

    ~DataSourceSQLite() override = default;
    mw::E<void> storeSave(const std::string& save) const override;
    mw::E<std::optional<std::string>> loadLatestSave() const override;

    // Do not use.
    DataSourceSQLite() = default;
private:
    std::unique_ptr<mw::SQLite> db;
};
