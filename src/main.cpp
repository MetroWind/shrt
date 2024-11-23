#include <memory>

#include <spdlog/spdlog.h>

#include "config.hpp"
#include "data.hpp"
#include "app.hpp"

int main()
{
    spdlog::set_level(spdlog::level::debug);
    Configuration config;
    auto data_source = DataSourceSQLite::fromFile(
        (std::filesystem::path(config.data_dir) / "saves.db").string());
    if(!data_source.has_value())
    {
        spdlog::error("Failed to create data source: {}",
                      errorMsg(data_source.error()));
        return 2;
    }
    App app(config, *(std::move(data_source)));
    app.start();
    app.wait();
    return 0;
}
