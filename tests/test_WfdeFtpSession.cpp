#include "war_tests.h"
#include "wfde/wfde.h"
#include "../src/wfde/ftp/WfdeFtpSession.h"

#include <boost/test/unit_test.hpp>

using namespace std;
using namespace war;
using namespace war::wfde;
using namespace war::wfde::impl;

// Simulate valid input
class TestWfdeFtpSessionInput : public WfdeFtpSessionInput
{
public:
    TestWfdeFtpSessionInput() : WfdeFtpSessionInput(nullptr) {}

protected:
    virtual size_t ReadSome(char* p, const size_t bytes) override {
        if (index_ < data_.size()) {

            const auto& curr_data = data_[index_++];
            assert(bytes >= curr_data.size());
            memcpy(p, curr_data.c_str(), curr_data.size());
            return curr_data.size();
        }

        return 0;
    }

private:
    std::vector<std::string> data_ = {
        "SYST\r\n",
        "RETR path/path/path\r\n",
        "NLST test/me\r\n",
        "TEST ", // no crlf
        "IT\r\n",
        "TEST1\r\nTEST2 \r\n TEST3 \r\n"
        "test1"
        "\r\n"
        "test2"
        "\r"
        "\ntest3"
        "\r"
        "\n"
    };

    int index_ = 0;
};

// Simulate buffer overflow
class TestWfdeFtpSessionInputOf : public WfdeFtpSessionInput
{
public:
    TestWfdeFtpSessionInputOf() : WfdeFtpSessionInput(nullptr) {}

protected:
    virtual size_t ReadSome(char* p, const size_t bytes) override {
        static const auto data = "abcdefghijklmnop"s;
        const auto len = min(bytes, data.size());
        memcpy(p, data.c_str(), len);
        return len;
    }
};

BOOST_AUTO_TEST_SUITE(Wfde_Unit_Tests)

BOOST_AUTO_TEST_CASE(Test_WfdeFtpSessionInput)
{
    TestWfdeFtpSessionInput sesi;

    BOOST_CHECK(sesi.FetchNextCommand() == "SYST");
    BOOST_CHECK(sesi.FetchNextCommand() == "RETR path/path/path");
    BOOST_CHECK(sesi.FetchNextCommand() == "NLST test/me");
    BOOST_CHECK(sesi.FetchNextCommand() == "TEST IT");
    BOOST_CHECK(sesi.FetchNextCommand() == "TEST1");
    BOOST_CHECK(sesi.FetchNextCommand() == "TEST2 ");
    BOOST_CHECK(sesi.FetchNextCommand() == " TEST3 ");
    BOOST_CHECK(sesi.FetchNextCommand() == "test1");
    BOOST_CHECK(sesi.FetchNextCommand() == "test2");
    BOOST_CHECK(sesi.FetchNextCommand() == "test3");
    BOOST_CHECK_THROW(sesi.FetchNextCommand(),
                      war::wfde::impl::WfdeFtpSessionInput::NoInputException);
}

BOOST_AUTO_TEST_CASE(Test_WfdeFtpSessionInput_Overflow)
{
    TestWfdeFtpSessionInputOf sesi;

    BOOST_CHECK_THROW(sesi.FetchNextCommand(),
                      war::wfde::impl::WfdeFtpSessionInput::InputTooBigException);
}

BOOST_AUTO_TEST_SUITE_END()

