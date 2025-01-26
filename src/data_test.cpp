#include <vector>

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <mw/error.hpp>
#include <mw/utils.hpp>
#include <mw/test_utils.hpp>

#include "data.hpp"

using ::testing::IsEmpty;

TEST(DataSource, CanAddAndDeleteLink)
{
    ASSIGN_OR_FAIL(std::unique_ptr<DataSourceSQLite> data,
                   DataSourceSQLite::newFromMemory());
    ShortLink link0;
    link0.shortcut = "link0";
    link0.original_url = "https://darksair.org/";
    link0.type = ShortLink::NORMAL;
    link0.user_id = "aaa";
    EXPECT_TRUE(mw::isExpected(data->addLink(std::move(link0))));
    ASSIGN_OR_FAIL(std::vector<ShortLink> links0, data->getAllLinks("aaa"));
    ASSERT_EQ(links0.size(), 1);

    EXPECT_TRUE(mw::isExpected(data->removeLink(links0[0].id)));
    ASSIGN_OR_FAIL(std::vector<ShortLink> links1, data->getAllLinks("aaa"));
    EXPECT_THAT(links1, IsEmpty());
}
