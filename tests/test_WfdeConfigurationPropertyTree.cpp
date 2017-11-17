#include "war_tests.h"
#include <wfde/wfde.h>
#include "../src/wfde/WfdeConfigurationPropertyTree.h"

using namespace std;
using namespace war;
using namespace war::wfde;
using namespace war::wfde::impl;

const lest::test specification[] = {

STARTCASE(Test_WfdeConfigurationPropertyTree) {

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
    EXPECT(conf->EnumNodes("/").size() == 4);
    EXPECT(conf->EnumNodes("/key2").size() == 3);
    EXPECT(conf->EnumNodes("/key2").at(1).name == "key2b");
    EXPECT(conf->EnumNodes("/key2").at(0).name == "key2a");
    EXPECT(conf->EnumNodes("/key2/key2b").size() == 2);
    EXPECT(conf->GetValue("/key3", "") == "yellow");
    EXPECT(conf->GetValue("key3", "") == "yellow");
    EXPECT(conf->GetValue("/key3", "blue") == "yellow");
    EXPECT(conf->GetValue("/nokey", "blue") == "blue");
    EXPECT(conf->GetValue("/key2/key2b/key2b_1/key2b_1_1") == "blue");

    auto subnode = conf->GetConfigForPath("/key2");
    EXPECT(subnode->EnumNodes("/key2b").size() == 2);
    EXPECT(subnode->EnumNodes("key2b").size() == 2);
    EXPECT(subnode->EnumNodes("/key2b").at(0).name == "key2b_1");
    EXPECT(subnode->EnumNodes("key2b").at(0).name == "key2b_1");
} ENDCASE
}; //lest

int main( int argc, char * argv[] )
{
    log::LogEngine log;
    log.AddHandler(log::LogToFile::Create("Test_ServerObject.log", true, "file",
        log::LL_TRACE4, log::LA_DEFAULT_ENABLE | log::LA_THREADS) );

    return lest::run( specification, argc, argv );
}
