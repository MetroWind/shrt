#include <httplib.h>
#include <memory>
#include <iostream>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mw/test_utils.hpp>
#include <mw/http_client.hpp>
#include <mw/auth_mock.hpp>

#include "app.hpp"
#include "config.hpp"
#include "data.hpp"
#include "data_mock.hpp"

using ::testing::_;
using ::testing::Return;
using ::testing::HasSubstr;
using ::testing::FieldsAre;
using ::testing::ContainsRegex;

void PrintTo(const ShortLink& link, std::ostream* os)
{
    *os << "ShortLink(id: " << link.id
        << ", shortcut: " << link.shortcut
        << ", original_url: " << link.original_url
        << ", type: " << link.type
        << ", user_id: " << link.user_id
        << ", user_name: " << link.user_name
        << ", visits: " << link.visits
        << ", time_creation: " << link.time_creation << ")";
}

class UserAppTest : public testing::Test
{
protected:
    UserAppTest()
    {
        config.base_url = "http://localhost:8080/";
        config.listen_address = "localhost";
        config.listen_port = 8080;
        config.data_dir = ".";

        auto auth = std::make_unique<mw::AuthMock>();

        mw::UserInfo expected_user;
        expected_user.name = "mw";
        expected_user.id = "mw";
        mw::Tokens token;
        token.access_token = "aaa";
        EXPECT_CALL(*auth, getUser(std::move(token)))
            .Times(::testing::AtLeast(0))
            .WillRepeatedly(Return(expected_user));
        auto data = std::make_unique<DataSourceMock>();
        data_source = data.get();

        app = std::make_unique<App>(config, std::move(data), std::move(auth));
    }

    Configuration config;
    std::unique_ptr<App> app;
    const DataSourceMock* data_source;
};

TEST_F(UserAppTest, CanDenyAccessToLinkList)
{
    EXPECT_TRUE(mw::isExpected(app->start()));
    {
        mw::HTTPSession client;
        ASSIGN_OR_FAIL(const mw::HTTPResponse* res, client.get(
            mw::HTTPRequest("http://localhost:8080/_/links")));
        EXPECT_EQ(res->status, 401);
    }
    app->stop();
    app->wait();
}

TEST_F(UserAppTest, CanHandleLinkList)
{
    std::vector<ShortLink> links;
    ShortLink link0;
    link0.shortcut = "link0";
    link0.original_url = "a";
    link0.id = 1;
    link0.user_id = "mw";
    link0.user_name = "mw";
    link0.type = ShortLink::NORMAL;
    ShortLink link1;
    link1.shortcut = "link1";
    link1.original_url = "b";
    link1.id = 2;
    link1.user_id = "mw";
    link1.user_name = "mw";
    link1.type = ShortLink::REGEXP;
    links.push_back(std::move(link0));
    links.push_back(std::move(link1));

    EXPECT_CALL(*data_source, getAllLinks("mw"))
        .WillOnce(Return(std::move(links)));

    EXPECT_TRUE(mw::isExpected(app->start()));
    {
        mw::HTTPSession client;
        ASSIGN_OR_FAIL(const mw::HTTPResponse* res, client.get(
            mw::HTTPRequest("http://localhost:8080/_/links")
            .addHeader("Cookie", "shrt-access-token=aaa")));
        EXPECT_EQ(res->status, 200) << "Response body: " << res->payloadAsStr();
        EXPECT_THAT(res->payloadAsStr(), ContainsRegex("<td>a</td>[[:space:]]*<td>-</td>"));
        EXPECT_THAT(res->payloadAsStr(), ContainsRegex("<td>b</td>[[:space:]]*<td>✅</td>"));
    }
    app->stop();
    app->wait();
}

TEST_F(UserAppTest, CanDenyAccessToNewLink)
{
    EXPECT_TRUE(mw::isExpected(app->start()));
    {
        mw::HTTPSession client;
        ASSIGN_OR_FAIL(const mw::HTTPResponse* res, client.get(
            mw::HTTPRequest("http://localhost:8080/_/new-link")));
        EXPECT_EQ(res->status, 401);
    }
    app->stop();
    app->wait();
}

TEST_F(UserAppTest, CanHandleNewLink)
{
    EXPECT_TRUE(mw::isExpected(app->start()));
    {
        mw::HTTPSession client;
        ASSIGN_OR_FAIL(const mw::HTTPResponse* res, client.get(
            mw::HTTPRequest("http://localhost:8080/_/new-link")
            .addHeader("Cookie", "shrt-access-token=aaa")));
        EXPECT_EQ(res->status, 200);
        EXPECT_THAT(res->payloadAsStr(), HasSubstr("Create a New Link"));
    }
    app->stop();
    app->wait();
}

TEST_F(UserAppTest, CanDenyAccessToCreateLink)
{
    EXPECT_TRUE(mw::isExpected(app->start()));
    {
        mw::HTTPSession client;
        ASSIGN_OR_FAIL(const mw::HTTPResponse* res, client.post(
            mw::HTTPRequest("http://localhost:8080/_/create-link")));
        EXPECT_EQ(res->status, 401);
    }
    app->stop();
    app->wait();
}

TEST_F(UserAppTest, CanDenyHandleCreateLink)
{
    EXPECT_CALL(*data_source, addLink(
        FieldsAre(
            _,                  // id
            "abc",              // shortcut
            "http://darksair.org", // original_url
            ShortLink::NORMAL,     // type
            "mw",                  // user_id
            "",                  // user_name
            _,                     // visits
            _)))                   // time_creation
        .WillOnce(Return(mw::E<void>()));

    EXPECT_CALL(*data_source, addLink(
        FieldsAre(
            _,                  // id
            "xyz",              // shortcut
            "http://mws.rocks", // original_url
            ShortLink::REGEXP,     // type
            "mw",                  // user_id
            "",                  // user_name
            _,                     // visits
            _)))                   // time_creation
        .WillOnce(Return(mw::E<void>()));

    EXPECT_TRUE(mw::isExpected(app->start()));
    {
        mw::HTTPSession client;
        ASSIGN_OR_FAIL(const mw::HTTPResponse* res1, client.post(
            mw::HTTPRequest("http://localhost:8080/_/create-link")
            .setPayload("shortcut=abc&original_url=http%3A%2F%2Fdarksair%2Eorg"
                        "&regexp=off")
            .addHeader("Cookie", "shrt-access-token=aaa")
            .setContentType("application/x-www-form-urlencoded")));
        EXPECT_EQ(res1->status, 302);
        EXPECT_EQ(res1->header.at("Location"), "http://localhost:8080/");

        ASSIGN_OR_FAIL(const mw::HTTPResponse* res2, client.post(
            mw::HTTPRequest("http://localhost:8080/_/create-link")
            .setPayload("shortcut=xyz&original_url=http%3A%2F%2Fmws%2Erocks"
                        "&regexp=on")
            .addHeader("Cookie", "shrt-access-token=aaa")
            .setContentType("application/x-www-form-urlencoded")));
        EXPECT_EQ(res2->status, 302);
        EXPECT_EQ(res2->header.at("Location"), "http://localhost:8080/");
    }
    app->stop();
    app->wait();
}
