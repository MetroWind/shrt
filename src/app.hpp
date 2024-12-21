#pragma once

#include <atomic>
#include <memory>
#include <string>

#include <inja.hpp>
#include <mw/url.hpp>
#include <mw/http_server.hpp>

#include "data.hpp"
#include "config.hpp"

class App : public mw::HTTPServer
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

private:
    void setup() override;

    Configuration config;
    mw::URL base_url;
    inja::Environment templates;
    std::unique_ptr<DataSourceInterface> data;
};
