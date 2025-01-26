#pragma once

#include <vector>
#include <string>
#include <optional>

#include <gmock/gmock.h>
#include <mw/error.hpp>

#include "data.hpp"

class DataSourceMock : public DataSourceInterface
{
public:
    ~DataSourceMock() override = default;

    MOCK_METHOD(mw::E<int64_t>, getSchemaVersion, (), (const override));
    MOCK_METHOD(mw::E<void>, addLink, (ShortLink&& link), (const override));
    MOCK_METHOD(mw::E<std::optional<ShortLink>>, findLinkByShortcut,
                (const std::string& shortcut), (const override));
    MOCK_METHOD(mw::E<std::optional<ShortLink>>, findLinkFromRegexpLinks,
                (const std::string& shortcut), (const override));
    MOCK_METHOD(mw::E<std::vector<ShortLink>>, getAllLinks,
                (const std::string& user_id), (const override));
    MOCK_METHOD(mw::E<std::optional<ShortLink>>, getLink, (int64_t id),
                (const override));
    MOCK_METHOD(mw::E<void>, removeLink, (int64_t id), (const override));

protected:
    mw::E<void> setSchemaVersion([[maybe_unused]] int64_t v) const override
    {
        return {};
    }
};
