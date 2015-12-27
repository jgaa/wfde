#include "wfded.h"

#include <boost/program_options.hpp>

#include <log/WarLog.h>
#include "war_error_handling.h"

#ifdef WIN32
#   include "win/minidump.h"
#endif
#include <tasks/WarThreadpool.h>

#include "AuthManager.h"

using namespace std;
using namespace std::string_literals;
using namespace war;
using namespace wfde;
using namespace wfde::test;

/* Default configuration data
 *
 * At this stage in the development, the command-line options are detached
 * from the hard-coded test configuration data. This will change before
 * beta release.
 */

struct CmdLineOptions
{
    int num_io_threads {0};
    int max_io_thread_queue_capacity {1024 * 64};
#ifndef WIN32
    bool daemon {false};
#endif
    std::string conf_file {"wfded.conf"};
};

bool ParseCommandLine(int argc, char *argv[], log::LogEngine& logger,
                      CmdLineOptions& conf)
{
    namespace po = boost::program_options;

    po::options_description general("General Options");

    general.add_options()
        ("help,h", "Print help and exit")
        ("config-file,c",
            po::value<string>(&conf.conf_file)->default_value(conf.conf_file),
            "Configuration file")
        ("console-log,C", po::value<string>()->default_value("NOTICE"),
            "Log-level for the console-log")
        ("log-level,L", po::value<string>()->default_value("NOTICE"),
            "Log-level for the log-file")
        ("log-file", po::value<string>()->default_value("wfded.log"),
            "Name of the log-file")
        ("truncate-log", po::value<bool>()->default_value(true),
            "Truncate the log-file if it already exists")
#ifndef WIN32
        ("daemon", po::value<bool>(&conf.daemon), "Run as a system daemon")
#endif
        ;

    po::options_description performance("Performance Options");
    performance.add_options()
        ("io-threads", po::value<int>(&conf.num_io_threads)->default_value(
            conf.num_io_threads),
            "Number of IO threads. If 0, a reasonable value will be used")
        ("io-queue-size", po::value<int>(&conf.max_io_thread_queue_capacity)->default_value(
            conf.max_io_thread_queue_capacity),
            "Capacity of the IO thread queues (max number of pending tasks per thread)")
        ;

    po::options_description cmdline_options;
    cmdline_options.add(general).add(performance);

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
    po::notify(vm);

    if (vm.count("help")) {
        cout << cmdline_options << endl
            << "Log-levels are:" << endl
            << "   FATAL ERROR WARNING INFO NOTICE DEBUG " << endl
            << "   TRACE1 TRACE2 TRACE3 TRACE4" << endl;

        return false;
    }

    if (
#ifndef WIN32
        !conf.daemon && 
#endif
        vm.count("console-log")
    ) {
        logger.AddHandler(make_shared<log::LogToStream>(cout, "console",
            log::LogEngine::GetLevelFromName(vm["console-log"].as<string>())));
    }

    if (vm.count("log-level")) {
        logger.AddHandler(make_shared<log::LogToFile>(
            vm["log-file"].as<string>(),
            vm["truncate-log"].as<bool>(),
            "file",
            log::LogEngine::GetLevelFromName(vm["log-level"].as<string>())));
    }

    return true;
}

using ip_list_t = vector<pair<string /* host */, string /* port */>>;


void SleepUntilDoomdsay()
{
    io_service_t main_thread_service;

    boost::asio::signal_set signals(main_thread_service, SIGINT, SIGTERM
#ifdef SIGQUIT
        ,SIGQUIT
#endif
        );
    signals.async_wait([](boost::system::error_code /*ec*/, int signo) {

        LOG_INFO << "Reiceived signal " << signo << ". Shutting down";
    });

    LOG_DEBUG_FN << "Main thread going to sleep - waiting for shtudown signal";
    main_thread_service.run();
    LOG_DEBUG_FN << "Main thread is awake";
}

// Go over the configuration and prepare servers, hosts and interfaces
auto PrepareServers(const Configuration::ptr_t& conf, Threadpool& threadPool)
{
    std::vector<Server::ptr_t> servers;

    // Enumerate servers
    auto svrdefs = conf->EnumNodes("/");
    for(auto svrdef : svrdefs) {
        const auto svr_path = "/"s + svrdef.name;
        auto server = CreateServer(conf->GetConfigForPath(svr_path), threadPool);

        // Enumerate hosts
        auto hosts_path = svr_path + "/Hosts"s;
        auto hostsdefs = conf->EnumNodes(hosts_path.c_str());
        for(auto hostdef : hostsdefs) {
            auto thishost_path = hosts_path + "/"s + hostdef.name;

            auto auth_mgr = make_shared<wfded::AuthManagerImpl>(
                *conf->GetConfigForPath(thishost_path + "/Users"s));

            auto host = CreateHost(*server, auth_mgr,
                conf->GetConfigForPath(thishost_path));
            auto perms = CreatePermissions(
                conf->GetConfigForPath(thishost_path + "/Paths"s));
            host->SetPermissions(move(perms));

            // enumerate protocols
            const auto prot_path = thishost_path + "/Protocols"s;
            auto prdefs = conf->EnumNodes(prot_path.c_str());
            for(auto prdef: prdefs) {
                const auto thispr_path = prot_path + "/"s + prdef.name;
                auto prot = CreateProtocol(host.get(),
                    conf->GetConfigForPath(thispr_path));

                /* Create the interface(s) that are configured for the protocol */
                prot->AddInterfaces();
            }
        }
        servers.push_back(server); // Keep the reference hot
    }

    return servers;
}

int main(int argc, char *argv[])
{
    log::LogEngine logger;
    CmdLineOptions options;

#ifdef WIN32
    /*
     * Enable minidump generation if the application crash under Windows
     */
    EnableMinidump("wfded");
#endif

    if (!ParseCommandLine(argc, argv, logger, options))
        return -1;

    LOG_INFO << "wfded " << wfde::Version() << " starting up";

    try {
        /* 
         * Create a configuration 
         */
        Configuration::ptr_t conf = war::wfde::Configuration::GetConfiguration(
            options.conf_file);
        WAR_ASSERT(conf != nullptr);

#ifndef WIN32
        if (options.daemon) {
            LOG_INFO << "Switching to system daemon mode";
            daemon(1, 0);
        }
#endif

        /*! Create a thread-pool */
        Threadpool thread_pool(options.num_io_threads,
                               options.max_io_thread_queue_capacity);

        /*! Prepare use of the FTP protocol */
        RegisterDefaultProtocols();

        auto servers = PrepareServers(conf, thread_pool);

        LOG_INFO << "Starting all services";

        /* Start the server(s) */
        for(auto& server : servers) {
            server->Start();
        }

        /* We now put the main-thread to sleep.
         *
         * It will remain sleeping until we receive one of common
         * shutdown/quit siglals.
         */
        SleepUntilDoomdsay();

        LOG_INFO << "Stopping all services";

        /* Stop the server(s) - terminate all connections and transfers */
        for(auto& server : servers) {
            server->Stop();
        }

        LOG_NOTICE << "Shutting down the thread-pool";

        /* Shut down the the thread-pool and wait for the
         * threads in the thread-pool to finish.
         */
        thread_pool.Close();
        thread_pool.WaitUntilClosed();

    } catch(const war::ExceptionBase& ex) {
        LOG_ERROR_FN << "Caught exception: " << ex;
        return -1;
    } catch(const boost::exception& ex) {
        LOG_ERROR_FN << "Caught boost exception: " << ex;
        return -1;
    } catch(const std::exception& ex) {
        LOG_ERROR_FN << "Caught standad exception: " << ex;
        return -1;
    } catch(...) {
        LOG_ERROR_FN << "Caught UNKNOWN exception!";
        return -1;
    };

    LOG_INFO << "So Long, and Thanks for All the Fish!";

    return 0;
}
