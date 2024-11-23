#include <filesystem>
#include <vector>
#include <fstream>
#include <format>
#include <utility>

#include <ryml.hpp>
#include <ryml_std.hpp>
#include <mw/error.hpp>

#include "config.hpp"

namespace {

mw::E<std::vector<char>> readFile(const std::filesystem::path& path)
{
    std::ifstream f(path, std::ios::binary);
    std::vector<char> content;
    content.reserve(102400);
    content.assign(std::istreambuf_iterator<char>(f),
                   std::istreambuf_iterator<char>());
    if(f.bad() || f.fail())
    {
        return std::unexpected(mw::runtimeError(
            std::format("Failed to read file {}", path.string())));
    }

    return content;
}

} // namespace

mw::E<Configuration> Configuration::fromYaml(const std::filesystem::path& path)
{
    auto buffer = readFile(path);
    if(!buffer.has_value())
    {
        return std::unexpected(buffer.error());
    }

    ryml::Tree tree = ryml::parse_in_place(ryml::to_substr(*buffer));
    Configuration config;
    if(tree["listen-address"].readable())
    {
        tree["listen-address"] >> config.listen_address;
    }
    if(tree["listen-port"].readable())
    {
        tree["listen-port"] >> config.listen_port;
    }
    if(tree["base-url"].readable())
    {
        tree["base-url"] >> config.base_url;
    }
    if(tree["data-dir"].readable())
    {
        tree["data-dir"] >> config.data_dir;
    }

    return mw::E<Configuration>{std::in_place, std::move(config)};
}
