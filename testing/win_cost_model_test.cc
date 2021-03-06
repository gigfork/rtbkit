/* win_cost_model_test.cc
   Eric Robert, 16 May 2013
   Copyright (c) 2013 Datacratic.  All rights reserved.

   Test for the win cost model.
*/


#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>

#include "rtbkit/common/win_cost_model.h"
#include "rtbkit/plugins/exchange/openrtb_exchange_connector.h"
#include "rtbkit/testing/bid_stack.h"

using namespace Datacratic;
using namespace RTBKIT;

namespace {

Amount linearWinCostModel(WinCostModel const & model,
                          Bid const & bid,
                          Amount const & price)
{
    double m = model.data["m"].asDouble();
    Amount b = Amount::fromJson(model.data["b"]);
    return price * m + b;
}

struct TestExchangeConnector : public OpenRTBExchangeConnector {
    TestExchangeConnector(ServiceBase & owner,
                          const std::string & name)
        : OpenRTBExchangeConnector(owner, name) {
    }

    static std::string exchangeNameString() {
        return "test";
    }

    std::string exchangeName() const {
        return exchangeNameString();
    }

    WinCostModel
    getWinCostModel(Auction const & auction, AgentConfig const & agent) {
        Json::Value data;
        data["m"] = 0.5;
        data["b"] = MicroUSD(5.0).toJson();
        return WinCostModel("test", data);
    }
};

} // file scope

BOOST_AUTO_TEST_CASE( win_cost_model_test )
{
    // register the exchange
    ExchangeConnector::registerFactory<TestExchangeConnector>();

    // register the win cost model
    WinCostModel::registerModel("test", linearWinCostModel);

    std::string configuration = ML::format(
        "[{"
            "\"exchangeType\":\"test\""
        "}]");

    std::cout << configuration << std::endl;

    BidStack stack;
    stack.run(configuration, USD_CPM(1.0), 10);

    auto events = stack.proxies->events->get(std::cerr);

    BOOST_CHECK_EQUAL(events["router.cummulatedBidPrice"], 10000);
    BOOST_CHECK_EQUAL(events["router.cummulatedAuthorizedPrice"], 5050);
}

