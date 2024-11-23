#pragma once

#include <atomic>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <inja.hpp>
#include <httplib.h>
#include <spdlog/spdlog.h>
#include <mw/url.hpp>

#include "data.hpp"
#include "config.hpp"

class App
{
public:
    App() = delete;
    App(const Configuration& conf,
        std::unique_ptr<DataSourceInterface> data_source);

    std::string urlFor(const std::string& name, const std::string& arg="") const;

    void handleIndex(httplib::Response& res);
    void handleStoreSave(const httplib::Request& req, httplib::Response& res)
        const;
    void handleRetrieveSave(httplib::Response& res) const;

    void start();
    void stop();
    void wait();

private:
    void setup();

    Configuration config;
    mw::URL base_url;
    inja::Environment templates;
    std::unique_ptr<DataSourceInterface> data;
    std::atomic<bool> should_stop;
    std::thread server_thread;
    httplib::Server server;
};
