#pragma once

#include <set>
#include <vector>
#include <set>

#ifndef BOOST_TEST_DYN_LINK
    #define BOOST_TEST_DYN_LINK
#endif

#include "lest/lest.hpp"

#include <warlib/basics.h>
#include <warlib/WarLog.h>
#include <warlib/asio.h>

namespace war {
namespace wfde {

#define STARTCASE(name) { CASE(#name) { \
    LOG_DEBUG << "================================"; \
    LOG_INFO << "Test case: " << #name; \
    LOG_DEBUG << "================================";

#define ENDCASE \
    LOG_DEBUG << "============== ENDCASE ============="; \
}},

} // wfde
} // war
