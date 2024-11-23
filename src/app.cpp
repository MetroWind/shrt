#include "mw/error.hpp"
#define CPPHTTPLIB_FORM_URL_ENCODED_PAYLOAD_MAX_LENGTH 131072

#include <nlohmann/json.hpp>
#include <inja.hpp>
#include <httplib.h>
#include <spdlog/spdlog.h>
#include <mw/error.hpp>

#include "statics.hpp"
#include "app.hpp"

#define _ASSIGN_OR_RESPOND_ERROR(tmp, var, val, res)                    \
    auto tmp = val;                                                     \
    if(!tmp.has_value())                                                \
    {                                                                   \
        if(std::holds_alternative<mw::HTTPError>(tmp.error()))          \
        {                                                               \
            const mw::HTTPError& e = std::get<mw::HTTPError>(tmp.error()); \
            res.status = e.code;                                        \
            res.set_content(e.msg, "text/plain");                       \
            return;                                                     \
        }                                                               \
        else                                                            \
        {                                                               \
            res.status = 500;                                           \
            res.set_content(std::visit([](const auto& e) { return e.msg; }, \
                                       tmp.error()),                    \
                            "text/plain");                              \
            return;                                                     \
        }                                                               \
    }                                                                   \
    var = std::move(tmp).value()

// Val should be a rvalue.
#define ASSIGN_OR_RESPOND_ERROR(var, val, res)                          \
    _ASSIGN_OR_RESPOND_ERROR(_CONCAT_NAMES(assign_or_return_tmp, __COUNTER__), \
                            var, val, res)

App::App(const Configuration& conf,
         std::unique_ptr<DataSourceInterface> data_source)
: config(conf),
    base_url(),
    templates(),
    data(std::move(data_source)),
    should_stop(false)
{
    auto u = mw::URL::fromStr(conf.base_url);
    if(u.has_value())
    {
        base_url = *std::move(u);
    }

    templates.add_callback("url_for", [&](const inja::Arguments& args) ->
                           std::string
    {
        switch(args.size())
        {
        case 1:
            return urlFor(args.at(0)->get_ref<const std::string&>());
        case 2:
            return urlFor(args.at(0)->get_ref<const std::string&>(),
                          args.at(1)->get_ref<const std::string&>());
        default:
            return "Invalid number of url_for() arguments";
        }
    });

    setup();
}

std::string App::urlFor(const std::string& name, [[maybe_unused]] const std::string& arg) const
{
    if(name == "index")
    {
        return base_url.path();
    }
    if(name == "statics")
    {
        return mw::URL(base_url).appendPath("statics").appendPath(arg).path();
    }
    if(name == "store-save")
    {
        return mw::URL(base_url).appendPath("store-save").path();
    }
    if(name == "retrieve-save")
    {
        return mw::URL(base_url).appendPath("retrieve-save").path();
    }
    return "";
}

void App::handleIndex(httplib::Response& res)
{
    res.status = 200;
    res.set_content(templates.render(MAIN_HTML, {}), "text/html");
}

void App::handleStoreSave(const httplib::Request& req, httplib::Response& res)
    const
{
    auto r = data->storeSave(req.body);
    if(!r.has_value())
    {
        res.status = 500;
        res.set_content(mw::errorMsg(r.error()), "text/plain");
        return;
    }
    res.status = 200;
    res.set_content("ok", "text/plain");
    spdlog::info("Save stored.");
}

void App::handleRetrieveSave(httplib::Response& res) const
{
    ASSIGN_OR_RESPOND_ERROR(std::optional<std::string> save,
                            data->loadLatestSave(), res);
    if(save.has_value())
    {
        res.status = 200;
        res.set_content(*save, "application/json");
        spdlog::info("Save retrieved.");
        return;
    }
    else
    {
        res.status = 404;
        return;
    }
}

void App::start()
{
    spdlog::info("Listening at http://{}:{}/...", config.listen_address,
                 config.listen_port);
    server_thread = std::thread([&] {
        try
        {
            server.listen(config.listen_address, config.listen_port);
        }
        catch(...)
        {
            spdlog::error("Exception when listening.");
        }
    });
    while(!server.is_running());
    server.wait_until_ready();
}

void App::setup()
{
    {
        std::string statics_dir = (std::filesystem::path(config.data_dir) /
                                   "statics").string();
        spdlog::info("Mounting static dir at {}...", statics_dir);
        auto ret = server.set_mount_point(
            mw::URL(base_url).appendPath("statics").path(), statics_dir);
        if (!ret)
        {
            spdlog::error("Failed to mount statics");
            return;
        }
    }

    server.Get(urlFor("index"), [&]([[maybe_unused]] const httplib::Request& req,
                        httplib::Response& res)
    {
        handleIndex(res);
    });

    server.Post(urlFor("store-save"),
                [&](const httplib::Request& req, httplib::Response& res)
    {
        handleStoreSave(req, res);
    });

    server.Get(urlFor("retrieve-save"),
                [&]([[maybe_unused]] const httplib::Request& req,
                    httplib::Response& res)
    {
        handleRetrieveSave(res);
    });

}

void App::stop()
{
    should_stop = true;
    server.stop();
}

void App::wait()
{
    while(!should_stop && server.is_running());
    server_thread.join();
}
