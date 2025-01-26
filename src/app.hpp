#pragma once

#include <memory>
#include <optional>
#include <string>

#include <inja.hpp>
#include <mw/url.hpp>
#include <mw/http_server.hpp>
#include <mw/error.hpp>
#include <mw/auth.hpp>

#include "data.hpp"
#include "config.hpp"

class App : public mw::HTTPServer
{
public:
    using Request = mw::HTTPServer::Request;
    using Response = mw::HTTPServer::Response;

    App() = delete;
    App(const Configuration& conf,
        std::unique_ptr<DataSourceInterface> data_source,
        std::unique_ptr<mw::AuthInterface> openid_auth);

    std::string urlFor(const std::string& name, const std::string& arg="") const;

    void handleIndex(Response& res) const;
    void handleLogin(Response& res) const;
    void handleOpenIDRedirect(const Request& req, Response& res) const;
    // List the links of the current user.
    void handleLinks(const Request& req, Response& res);
    void handleNewLink(const Request& req, Response& res);
    void handleCreateLink(const Request& req, Response& res) const;
    void handleDeleteLinkDialog(const Request& req, Response& res);
    void handleDeleteLink(const Request& req, Response& res) const;
    void handleShortcut(const Request& req, Response& res) const;

private:
    void setup() override;

    struct SessionValidation
    {
        enum { VALID, REFRESHED, INVALID } status;
        mw::UserInfo user;
        mw::Tokens new_tokens;

        static SessionValidation valid(mw::UserInfo&& user_info)
        {
            return {VALID, user_info, {}};
        }

        static SessionValidation refreshed(mw::UserInfo&& user_info, mw::Tokens&& tokens)
        {
            return {REFRESHED, user_info, tokens};
        }

        static SessionValidation invalid()
        {
            return {INVALID, {}, {}};
        }
    };
    mw::E<SessionValidation> validateSession(const Request& req) const;

    // Query the auth module for the status of the session. If there
    // is no session or it fails to query the auth module, set the
    // status and body in “res” accordingly, and return nullopt. In
    // this case if this function does return a value, it would never
    // be an invalid session.
    //
    // If “allow_error_and_invalid” is true, failure to query and
    // invalid session are considered ok, and no status and body would
    // be set in “res”. In this case this function just returns an
    // invalid session.
    std::optional<SessionValidation> prepareSession(
        const Request& req, Response& res,
        bool allow_error_and_invalid=false) const;

    // This gives a path, optionally with the name of an argument,
    // that is suitable to bind to a URL handler. For example,
    // supposed the URL of the blog post with ID 1 is
    // “http://some.domain/blog/p/1”. Calling “getPath("post", "id")”
    // would give “/blog/p/:id”. This uses urlFor(), and therefore
    // requires that the URL is mapped correctly in that function.
    std::string getPath(const std::string& name, const std::string& arg_name="")
        const;

    Configuration config;
    mw::URL base_url;
    inja::Environment templates;
    std::unique_ptr<DataSourceInterface> data;
    std::unique_ptr<mw::AuthInterface> auth;
};
