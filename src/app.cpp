#include <format>
#include <stddef.h>
#include <stdint.h>
#include <chrono>
#include <expected>
#include <filesystem>
#include <iterator>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <inja.hpp>
#include <mw/http_server.hpp>
#include <mw/url.hpp>
#include <mw/utils.hpp>
#include <mw/crypto.hpp>
#include <mw/error.hpp>
#include <mw/auth.hpp>

#include "app.hpp"
#include "data.hpp"
#include "mw/error.hpp"

namespace
{

std::unordered_map<std::string, std::string> parseCookies(std::string_view value)
{
    std::unordered_map<std::string, std::string> cookies;
    size_t begin = 0;
    while(true)
    {
        if(begin >= value.size())
        {
            break;
        }

        size_t semicolon = value.find(';', begin);
        if(semicolon == std::string::npos)
        {
            semicolon = value.size();
        }

        std::string_view section = value.substr(begin, semicolon - begin);

        begin = semicolon + 1;
        // Skip spaces
        while(begin < value.size() && value[begin] == ' ')
        {
            begin++;
        }

        size_t equal = section.find('=');
        if(equal == std::string::npos) continue;
        cookies.emplace(section.substr(0, equal),
                        section.substr(equal+1, semicolon - equal - 1));
        if(semicolon >= value.size())
        {
            continue;
        }
    }
    return cookies;
}

void setTokenCookies(const mw::Tokens& tokens, App::Response& res)
{
    int64_t expire_sec = 300;
    if(tokens.expiration.has_value())
    {
        auto expire = std::chrono::duration_cast<std::chrono::seconds>(
            *tokens.expiration - mw::Clock::now());
        expire_sec = expire.count();
    }
    res.set_header("Set-Cookie", std::format(
                       "shrt-access-token={}; Max-Age={}",
                       mw::urlEncode(tokens.access_token), expire_sec));
    // Add refresh token to cookie, with one month expiration.
    if(tokens.refresh_token.has_value())
    {
        expire_sec = 1800;
        if(tokens.refresh_expiration.has_value())
        {
            auto expire = std::chrono::duration_cast<std::chrono::seconds>(
                *tokens.refresh_expiration - mw::Clock::now());
            expire_sec = expire.count();
        }

        res.set_header("Set-Cookie", std::format(
                           "shrt-refresh-token={}; Max-Age={}",
                           mw::urlEncode(*tokens.refresh_token), expire_sec));
    }
}

nlohmann::json link2JSON(const ShortLink& link)
{
    std::string type_is_regexp;
    switch(link.type)
    {
    case ShortLink::NORMAL:
        type_is_regexp = "-";
        break;
    case ShortLink::REGEXP:
        type_is_regexp = "✅";
        break;
    }

    return {{"shortcut", link.shortcut},
            {"original_url", link.original_url},
            {"id", link.id},
            {"id_str", std::to_string(link.id)},
            {"type", link.type},
            {"type_is_regexp_str", type_is_regexp},
            {"visits", link.visits},
            {"time", mw::timeToSeconds(link.time_creation)},
            {"time_str", mw::timeToStr(link.time_creation)},
            {"time_iso8601", mw::timeToISO8601(link.time_creation)}};
}

bool validateShortcut([[maybe_unused]] std::string_view shortcut)
{
    // This needs a unicode library to really work. For now I’ll just
    // not validate.
    return true;
}

} // namespace

App::App(const Configuration& conf,
         std::unique_ptr<DataSourceInterface> data_source,
         std::unique_ptr<mw::AuthInterface> openid_auth)
        : mw::HTTPServer(conf.listen_address, conf.listen_port),
          config(conf),
          templates((std::filesystem::path(config.data_dir) / "templates" / "")
                    .string()),
          data(std::move(data_source)),
          auth(std::move(openid_auth))
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
}

std::string App::urlFor(const std::string& name, const std::string& arg) const
{
    if(name == "statics")
    {
        return mw::URL(base_url).appendPath("_/statics").appendPath(arg).str();
    }
    if(name == "index")
    {
        return base_url.str();
    }
    if(name == "shortcut")
    {
        return mw::URL(base_url).appendPath(arg).str();
    }
    if(name == "links")
    {
        return mw::URL(base_url).appendPath("_/links").str();
    }
    if(name == "login")
    {
        return mw::URL(base_url).appendPath("_/login").str();
    }
    if(name == "openid-redirect")
    {
        return mw::URL(base_url).appendPath("_/openid-redirect").str();
    }
    if(name == "new-link")
    {
        return mw::URL(base_url).appendPath("_/new-link").str();
    }
    if(name == "create-link")
    {
        return mw::URL(base_url).appendPath("_/create-link").str();
    }
    if(name == "delete-link-dialog")
    {
        return mw::URL(base_url).appendPath("_/delete-link").appendPath(arg)
            .str();
    }
    if(name == "delete-link")
    {
        return mw::URL(base_url).appendPath("_/delete-link").str();
    }

    return "";
}

void App::handleIndex(Response& res) const
{
    res.set_redirect(urlFor("links"), 301);
}

void App::handleLinks(const Request& req, Response& res)
{
    auto session = prepareSession(req, res, true);
    if(session->status == SessionValidation::INVALID)
    {
        res.set_redirect(urlFor("login"));
        return;
    }

    nlohmann::json render_data = {{"session_user", ""},
                                  {"title", "Links"},
                                  {"links", nlohmann::json::array_t()}};
    ASSIGN_OR_RESPOND_ERROR(std::vector<ShortLink> links,
                            data->getAllLinks(session->user.id), res);
    render_data["session_user"] = session->user.name;
    for(const ShortLink& link: links)
    {
        render_data["links"].push_back(link2JSON(link));
    }

    try
    {
        std::string result = templates.render_file(
            "links.html", std::move(render_data));
        res.status = 200;
        res.set_content(result, "text/html");
    }
    catch(const inja::InjaError& e)
    {
        spdlog::error("Failed to render page: {}", e.what());
        res.status = 500;
    }
}

void App::handleLogin(Response& res) const
{
    res.set_redirect(auth->initialURL(), 301);
}

void App::handleOpenIDRedirect(const Request& req, Response& res) const
{
    if(req.has_param("error"))
    {
        res.status = 500;
        if(req.has_param("error_description"))
        {
            res.set_content(
                std::format("{}: {}.", req.get_param_value("error"),
                            req.get_param_value("error_description")),
                "text/plain");
        }
        return;
    }
    else if(!req.has_param("code"))
    {
        res.status = 500;
        res.set_content("No error or code in auth response", "text/plain");
        return;
    }

    std::string code = req.get_param_value("code");
    spdlog::debug("OpenID server visited {} with code {}.", req.path, code);
    ASSIGN_OR_RESPOND_ERROR(mw::Tokens tokens, auth->authenticate(code), res);
    ASSIGN_OR_RESPOND_ERROR(mw::UserInfo user, auth->getUser(tokens), res);

    setTokenCookies(tokens, res);
    res.set_redirect(urlFor("index"), 301);
}

void App::handleNewLink(const Request& req, Response& res)
{
    auto session = prepareSession(req, res);
    if(!session.has_value())
    {
        return;
    }

    nlohmann::json render_data = {{"session_user", session->user.name},
                                  {"title", "Create New Link"}};
    try
    {
        std::string result = templates.render_file(
            "new-link.html", std::move(render_data));
        res.status = 200;
        res.set_content(result, "text/html");
    }
    catch(const inja::InjaError& e)
    {
        spdlog::error("Failed to render page: {}", e.what());
        res.status = 500;
    }
}

void App::handleCreateLink(const Request& req, Response& res) const
{
    auto session = prepareSession(req, res);
    if(!session.has_value()) return;

    ShortLink link;
    link.type = ShortLink::NORMAL;

    if(!req.has_param("original_url"))
    {
        res.status = 400;
        res.set_content("Link should have an original URL.", "text/plain");
        return;
    }
    link.original_url = mw::strip(req.get_param_value("original_url"));
    if(link.original_url.empty())
    {
        res.status = 400;
        res.set_content("Link should have an original URL that is not empty.",
                        "text/plain");
        return;
    }
    if(req.has_param("shortcut"))
    {
        link.shortcut = mw::strip(req.get_param_value("shortcut"));
    }
    if(!validateShortcut(link.shortcut))
    {
        res.status = 400;
        res.set_content("Invalid shortcut", "text/plain");
        return;
    }

    if(req.has_param("regexp"))
    {
        link.type = req.get_param_value("regexp") == "on" ?
            ShortLink::REGEXP : ShortLink::NORMAL;
    }
    if(link.shortcut.empty())
    {
        if(link.type == ShortLink::REGEXP)
        {
            res.status = 400;
            res.set_content("Regexp link should have a non-empty shortcut",
                            "text/plain");
            return;
        }
        // Generate a shortcut by taking the first 8 bytes of the
        // SHA256 hash of the original URL, and base64-encode.
        ASSIGN_OR_RESPOND_ERROR(
            auto hash, mw::SHA256Hasher().hashToBytes(link.original_url), res);

        hash.resize(8);
        link.shortcut = mw::base64Encode(hash);
    }
    link.user_id = session->user.id;
    mw::E<void> result = data->addLink(std::move(link));
    if(!result.has_value())
    {
        res.status = 500;
        res.set_content(mw::errorMsg(result.error()), "text/plain");
        return;
    }
    res.set_redirect(urlFor("index"));
}

void App::handleDeleteLinkDialog(const Request& req, Response& res)
{
    ASSIGN_OR_RESPOND_ERROR(
        int64_t id, mw::strToNumber<int64_t>(req.path_params.at("id")).or_else(
            []([[maybe_unused]] auto _) -> mw::E<int64_t>
            {
                return std::unexpected(mw::httpError(401, "Invalid link ID"));
            }), res);

    auto session = prepareSession(req, res);
    if(!session.has_value()) return;

    ASSIGN_OR_RESPOND_ERROR(std::optional<ShortLink> link, data->getLink(id),
                            res);
    if(!link.has_value())
    {
        res.status = 404;
        return;
    }
    nlohmann::json data = {{"title", "Delete link"},
                           {"session_user", session->user.name},
                           {"link", {{"shortcut", link->shortcut},
                                     {"original_url", link->original_url},
                                     {"id_str", std::to_string(link->id)}}}};
    try
    {
        std::string result = templates.render_file(
            "delete-link.html", std::move(data));
        res.status = 200;
        res.set_content(result, "text/html");
    }
    catch(const inja::InjaError& e)
    {
        spdlog::error("Failed to render page: {}", e.what());
        res.status = 500;
    }
}

void App::handleDeleteLink(const Request& req, Response& res) const
{
    auto session = prepareSession(req, res);
    if(!session.has_value()) return;

    if(!req.has_param("id"))
    {
        res.status = 400;
        res.set_content("Need link ID", "text/plain");
        return;
    }
    ASSIGN_OR_RESPOND_ERROR(
        int64_t id, mw::strToNumber<int64_t>(req.get_param_value("id")), res);
    ASSIGN_OR_RESPOND_ERROR(std::optional<ShortLink> link, data->getLink(id),
                            res);
    if(!link.has_value())
    {
        res.status = 404;
        return;
    }

    if(std::move(link)->user_id != session->user.id)
    {
        res.status = 401;
        return;
    }

    mw::E<void> result = data->removeLink(id);
    if(!result.has_value())
    {
        res.status = 500;
        res.set_content(mw::errorMsg(result.error()), "text/plain");
        return;
    }
    res.set_redirect(urlFor("index"));
}

void App::handleShortcut(const Request& req, Response& res) const
{
    std::string shortcut = req.path_params.at("shortcut");
    if(shortcut.empty())
    {
        res.status = 500;
        res.set_content("This shouldn't happen!", "text/plain");
        return;
    }

    ASSIGN_OR_RESPOND_ERROR(std::optional<ShortLink> link,
                            data->findLinkByShortcut(shortcut), res);
    if(!link.has_value())
    {
        res.status = 404;
        return;
    }
    res.set_redirect(link->original_url, 308);
}

std::string App::getPath(const std::string& name,
                         const std::string& arg_name) const
{
    return mw::URL::fromStr(urlFor(name, std::string(":") + arg_name)).value()
        .path();
}

void App::setup()
{
    {
        std::string statics_dir = (std::filesystem::path(config.data_dir) /
                                   "statics").string();
        spdlog::info("Mounting static dir at {}...", statics_dir);
        if (!server.set_mount_point("/_/statics", statics_dir))
        {
            spdlog::error("Failed to mount statics");
            return;
        }
    }

    server.Get(getPath("index"), [&]([[maybe_unused]] const Request& req, Response& res)
    {
        handleIndex(res);
    });
    server.Get(getPath("login"), [&]([[maybe_unused]] const Request& req, Response& res)
    {
        handleLogin(res);
    });
    server.Get(getPath("openid-redirect"), [&](const Request& req, Response& res)
    {
        handleOpenIDRedirect(req, res);
    });
    server.Get(getPath("links"), [&](const Request& req, Response& res)
    {
        handleLinks(req, res);
    });
    server.Get(getPath("new-link"), [&](const Request& req, Response& res)
    {
        handleNewLink(req, res);
    });
    server.Post(getPath("create-link"), [&](const Request& req, Response& res)
    {
        handleCreateLink(req, res);
    });
    server.Get(getPath("delete-link-dialog", "id"), [&](const Request& req, Response& res)
    {
        handleDeleteLinkDialog(req, res);
    });
    server.Post(getPath("delete-link"), [&](const Request& req, Response& res)
    {
        handleDeleteLink(req, res);
    });
    server.Get(getPath("shortcut", "shortcut"),
                [&](const Request& req, Response& res)
    {
        handleShortcut(req, res);
    });

}

mw::E<App::SessionValidation> App::validateSession(const Request& req) const
{
    if(!req.has_header("Cookie"))
    {
        spdlog::debug("Request has no cookie.");
        return SessionValidation::invalid();
    }

    auto cookies = parseCookies(req.get_header_value("Cookie"));
    if(auto it = cookies.find("shrt-access-token");
       it != std::end(cookies))
    {
        spdlog::debug("Cookie has access token.");
        mw::Tokens tokens;
        tokens.access_token = it->second;
        mw::E<mw::UserInfo> user = auth->getUser(tokens);
        if(user.has_value())
        {
            return SessionValidation::valid(*std::move(user));
        }
    }
    // No access token or access token expired
    if(auto it = cookies.find("shrt-refresh-token");
       it != std::end(cookies))
    {
        spdlog::debug("Cookie has refresh token.");
        // Try to refresh the tokens.
        ASSIGN_OR_RETURN(mw::Tokens tokens, auth->refreshTokens(it->second));
        ASSIGN_OR_RETURN(mw::UserInfo user, auth->getUser(tokens));
        return SessionValidation::refreshed(std::move(user), std::move(tokens));
    }
    return SessionValidation::invalid();
}

std::optional<App::SessionValidation> App::prepareSession(
    const Request& req, Response& res, bool allow_error_and_invalid) const
{
    mw::E<SessionValidation> session = validateSession(req);
    if(!session.has_value())
    {
        if(allow_error_and_invalid)
        {
            return SessionValidation::invalid();
        }
        else
        {
            res.status = 500;
            res.set_content("Failed to validate session.", "text/plain");
            return std::nullopt;
        }
    }

    switch(session->status)
    {
    case SessionValidation::INVALID:
        if(allow_error_and_invalid)
        {
            return *session;
        }
        else
        {
            res.status = 401;
            res.set_content("Invalid session.", "text/plain");
            return std::nullopt;
        }
    case SessionValidation::VALID:
        break;
    case SessionValidation::REFRESHED:
        setTokenCookies(session->new_tokens, res);
        break;
    }
    return *session;
}
