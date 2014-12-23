
#define BOOST_TEST_MODULE WfdeTests

#include "war_tests.h"
#include <chrono>
#include "wfde/wfde.h"
#include "TestConfig.h"
#include <tasks/WarThreadpool.h>


using namespace std;
using namespace war;
using namespace wfde::test;


BOOST_AUTO_TEST_SUITE(Wfde_Unit_Tests)

BOOST_AUTO_TEST_CASE(Test_ServerObject)
{
    log::LogEngine log;
    log.AddHandler(log::LogToFile::Create("test_wfde_ServerObject.log", true, "file",
        log::LL_TRACE4, log::LA_DEFAULT_ENABLE | log::LA_THREADS) );

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
        BOOST_CHECK_MESSAGE(v.size() == 1, "One config node");

        v = conf->EnumNodes("/Server/Hosts/FanClub");
        BOOST_CHECK_MESSAGE(v.size() == 2, "Five config nodes - o to two");
        BOOST_CHECK_MESSAGE(v[1].name == "Protocols", "Name");

        v = conf->EnumNodes("/Server/Hosts/FanClub/Protocols/ftp/Interfaces/tcp");
        BOOST_CHECK_MESSAGE(v.size() == 3, "Three config nodes");
    }

    // Verify the default server name
    {
        auto conf = make_shared<TestConfig>();
        auto svr = wfde::CreateServer(conf, tp); // empty conf
        BOOST_CHECK_MESSAGE((svr->GetName() == "Default"), "svr->GetName() == Default");
    }

    // Verify that we can set our own name in the config and that it applies for the server.
    {
        const std::string name { "My Test Server" };

        // Root config
        auto conf = make_shared<TestConfig>();
        conf->SetValue("/Server/Name", name);
        auto svr_conf = conf->GetConfigForPath("/Server");
        auto svr = wfde::CreateServer(svr_conf, tp);

        BOOST_CHECK_MESSAGE((svr->GetName() == name), std::string("svr->GetName() == ") + name);
    }
}


BOOST_AUTO_TEST_SUITE_END()
