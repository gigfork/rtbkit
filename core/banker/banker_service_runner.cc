/* banker_service_runner.cc                                        -*- C++ -*-
   Jeremy Barnes, 20 November 2012
   Copyright (c) 2012 Datacratic.  All rights reserved.

   Runner for the banker service.
*/


#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/positional_options.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/thread/thread.hpp>
#include <boost/make_shared.hpp>

#include "rtbkit/core/banker/master_banker.h"
#include "soa/service/service_utils.h"
#include "jml/utils/pair_utils.h"
#include "jml/arch/timers.h"
#include "jml/arch/futex.h"


using namespace std;
using namespace ML;
using namespace Datacratic;
using namespace RTBKIT;


int main(int argc, char ** argv)
{
    using namespace boost::program_options;

    ServiceProxyArguments serviceArgs;

    options_description configuration_options("Configuration options");

    std::string redisUri;  ///< TODO: zookeeper

    std::vector<std::string> fixedHttpBindAddresses;

    configuration_options.add_options()
        ("redis-uri,r", value<string>(&redisUri)->required(),
         "URI of connection to redis")
        ("fixed-http-bind-address,a", value(&fixedHttpBindAddresses),
         "Fixed address (host:port or *:port) at which we will always listen");

    options_description all_opt;
    all_opt
        .add(serviceArgs.makeProgramOptions())
        .add(configuration_options);
    all_opt.add_options()
        ("help,h", "print this message");
    
    variables_map vm;
    store(command_line_parser(argc, argv)
          .options(all_opt)
          //.positional(p)
          .run(),
          vm);
    notify(vm);

    if (vm.count("help")) {
        cerr << all_opt << endl;
        exit(1);
    }

    auto proxies = serviceArgs.makeServiceProxies();

    MasterBanker banker(proxies, "masterBanker");
    std::shared_ptr<Redis::AsyncConnection> redis;

    if (redisUri != "nopersistence") {
        auto address = Redis::Address(redisUri);
        redis = std::make_shared<Redis::AsyncConnection>(redisUri);
        redis->test();

        banker.init(std::make_shared<RedisBankerPersistence>(redisUri));
    }
    else {
        cerr << "*** WARNING ***" << endl;
        cerr << "BANKER IS RUNNING WITH NO PERSISTENCE" << endl;
        cerr << "IF THIS IS NOT A TESTING ENVIRONEMNT, SPEND WILL BE LOST" << endl;
        banker.init(std::make_shared<NoBankerPersistence>());
    }

    // Bind to any fixed addresses
    for (auto a: fixedHttpBindAddresses)
        banker.bindFixedHttpAddress(a);

    auto addr = banker.bindTcp();
    cerr << "master banker is listening on " << addr.first << ","
         << addr.second << endl;

    banker.start();
    proxies->config->dump(cerr);

    for (;;) {
        ML::sleep(10);
    }
}
