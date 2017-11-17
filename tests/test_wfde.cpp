
//#define BOOST_TEST_MAIN WfdeTests
#define BOOST_TEST_MODULE WfdeTests

#include "war_tests.h"
#include <chrono>
#include <wfde/wfde.h>
#include "TestConfig.h"
#include <warlib/WarThreadpool.h>

using namespace std;
using namespace war;
using namespace war::wfde;
using namespace war::wfde::test;

const lest::test specification[] = {

STARTCASE(Test_ServerObject) {
    Threadpool tp;

    // Check configuration
    {
        /* Test configuration data */
        initializer_list<TestConfig::data_container_t::value_type> conf_data {
            {"/Server/Name", "Server"},
            {"/Server/Test/Name", "Server"},
            {"/Server/Hosts/FanClub/Name", "FanClub"},
            {"/Server/Hosts/FanClub/Protocols/ftp/Name", "ftp"},
            {"/Server/Hosts/FanClub/Protocols/ftp/Interfaces/tcp/Name", "Localhost"},
            {"/Server/Hosts/FanClub/Protocols/ftp/Interfaces/tcp/Ip", "127.0.0.1"},
            {"/Server/Hosts/FanClub/Protocols/ftp/Interfaces/tcp/Port", "2121"},
        };

        auto conf = make_shared<TestConfig>(conf_data);

        auto v = conf->EnumNodes("/Server/Test");
        EXPECT(v.size() == 1);

        v = conf->EnumNodes("/Server/Hosts/FanClub");
        EXPECT(v.size() == 2);
        EXPECT(v[1].name == "Protocols");

        v = conf->EnumNodes("/Server/Hosts/FanClub/Protocols/ftp/Interfaces/tcp");
        EXPECT(v.size() == 3);
    }

    // Verify the default server name
    {
        auto conf = make_shared<TestConfig>();
        auto svr = wfde::CreateServer(conf, tp); // empty conf
        EXPECT(svr->GetName() == "Default");
    }

    // Verify that we can set our own name in the config and that it applies for the server.
    {
        const std::string name { "My Test Server" };

        // Root config
        auto conf = make_shared<TestConfig>();
        conf->SetValue("/Server/Name", name);
        auto svr_conf = conf->GetConfigForPath("/Server");
        auto svr = wfde::CreateServer(svr_conf, tp);

        EXPECT(svr->GetName() == name);
    }
} ENDCASE
}; //lest

int main( int argc, char * argv[] )
{
    log::LogEngine log;
    log.AddHandler(log::LogToFile::Create("Test_ServerObject.log", true, "file",
        log::LL_TRACE4, log::LA_DEFAULT_ENABLE | log::LA_THREADS) );

    return lest::run( specification, argc, argv );
}

