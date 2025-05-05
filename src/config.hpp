#pragma once

#include <filesystem>
#include <string>

#include <mw/error.hpp>

struct Configuration
{
    std::string listen_address = "localhost";
    // Set this to 0 to listen to socket file.
    int listen_port = 8123;
    std::string socket_user = "";
    std::string socket_group = "";
    int socket_permission = 0;
    std::string base_url = "http://localhost:8123/";
    std::string data_dir = ".";
    std::string openid_url_prefix;
    std::string client_id;
    std::string client_secret;

    static mw::E<Configuration> fromYaml(const std::filesystem::path& path);
};
