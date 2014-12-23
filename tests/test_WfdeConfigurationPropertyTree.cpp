#include "war_tests.h"
#include "wfde/wfde.h"
#include "../src/wfde/WfdeConfigurationPropertyTree.h"

#include <boost/test/unit_test.hpp>

using namespace std;
using namespace war;
using namespace war::wfde;
using namespace war::wfde::impl;

BOOST_AUTO_TEST_SUITE(Wfde_Unit_Tests)

BOOST_AUTO_TEST_CASE(Test_WfdeConfigurationPropertyTree)
{

    const auto df_name = "Test_WfdeConfigurationPropertyTree.data001";
    static const auto testdata =
        "key1 \"blue cats\"\n"
        "key2 {\n"
        "  key2a key2a_value\n"
        "  key2b \"green cats with pink eyes\" {\n"
        "    key2b_1  green {\n"
        "      key2b_1_1 blue\n"
        "    }\n"
        "    key2ba red\n"
        "    }\n"
        "  key2c\n"
        "  }\n"
        "key3 yellow\n"
        "key4\n"
        "\n";

    {
        std::ofstream data(df_name);
        data << testdata;
    }

    auto conf = WfdeConfigurationPropertyTree::CreateInstance(df_name);
    BOOST_CHECK(conf->EnumNodes("/").size() == 4);
    BOOST_CHECK(conf->EnumNodes("/key2").size() == 3);
    BOOST_CHECK(conf->EnumNodes("/key2").at(1).name == "key2b");
    BOOST_CHECK(conf->EnumNodes("/key2").at(0).name == "key2a");
    BOOST_CHECK(conf->EnumNodes("/key2/key2b").size() == 2);
    BOOST_CHECK(conf->GetValue("/key3", "") == "yellow");
    BOOST_CHECK(conf->GetValue("key3", "") == "yellow");
    BOOST_CHECK(conf->GetValue("/key3", "blue") == "yellow");
    BOOST_CHECK(conf->GetValue("/nokey", "blue") == "blue");
    BOOST_CHECK(conf->GetValue("/key2/key2b/key2b_1/key2b_1_1") == "blue");

    auto subnode = conf->GetConfigForPath("/key2");
    BOOST_CHECK(subnode->EnumNodes("/key2b").size() == 2);
    BOOST_CHECK(subnode->EnumNodes("key2b").size() == 2);
    BOOST_CHECK(subnode->EnumNodes("/key2b").at(0).name == "key2b_1");
    BOOST_CHECK(subnode->EnumNodes("key2b").at(0).name == "key2b_1");
}

BOOST_AUTO_TEST_SUITE_END()
