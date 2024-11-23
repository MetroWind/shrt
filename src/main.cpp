#include <memory>

#include <cxxopts.hpp>
#include <spdlog/spdlog.h>

#include "config.hpp"
#include "data.hpp"
#include "app.hpp"

int main(int argc, char** argv)
{
    cxxopts::Options cmd_options(
        "DoL server", "A DoL server");
    cmd_options.add_options()
        ("c,config", "Config file",
         cxxopts::value<std::string>()->default_value("/etc/dol-server.yaml"))
        ("h,help", "Print this message.");
    auto opts = cmd_options.parse(argc, argv);

    if(opts.count("help"))
    {
        std::cout << cmd_options.help() << std::endl;
        return 0;
    }

    const std::string config_file = opts["config"].as<std::string>();
    auto config = Configuration::fromYaml(std::move(config_file));
    if(!config.has_value())
    {
        spdlog::error("Failed to load config, using default: {}",
                      mw::errorMsg(config.error()));
        config = mw::E<Configuration>(Configuration());
    }

    auto data_source = DataSourceSQLite::fromFile(
        (std::filesystem::path(config->data_dir) / "saves.db").string());
    if(!data_source.has_value())
    {
        spdlog::error("Failed to create data source: {}",
                      errorMsg(data_source.error()));
        return 2;
    }
    App app(*std::move(config), *std::move(data_source));
    app.start();
    app.wait();
    return 0;
}
