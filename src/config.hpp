#pragma once

#include <filesystem>
#include <string>

#include <mw/error.hpp>

struct Configuration
{
    std::string listen_address = "localhost";
    int listen_port = 8123;
    std::string base_url = "http://localhost:8123/";
    std::string data_dir = ".";
    std::string openid_url_prefix;
    std::string client_id;
    std::string client_secret;

    static mw::E<Configuration> fromYaml(const std::filesystem::path& path);
};
