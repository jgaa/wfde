#include "war_tests.h"
#include <wfde/wfde.h>
#include "../src/wfde/ftp/WfdeFtpSession.h"

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
        if (index_ < static_cast<decltype(index_)>(data_.size())) {

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

const lest::test specification[] = {

STARTCASE(Test_WfdeFtpSessionInput)
{
    TestWfdeFtpSessionInput sesi;

    EXPECT(string(sesi.FetchNextCommand()) == "SYST");
    EXPECT(string(sesi.FetchNextCommand()) == "RETR path/path/path");
    EXPECT(string(sesi.FetchNextCommand()) == "NLST test/me");
    EXPECT(string(sesi.FetchNextCommand()) == "TEST IT");
    EXPECT(string(sesi.FetchNextCommand()) == "TEST1");
    EXPECT(string(sesi.FetchNextCommand()) == "TEST2 ");
    EXPECT(string(sesi.FetchNextCommand()) == " TEST3 ");
    EXPECT(string(sesi.FetchNextCommand()) == "test1");
    EXPECT(string(sesi.FetchNextCommand()) == "test2");
    EXPECT(string(sesi.FetchNextCommand()) == "test3");
    EXPECT_THROWS_AS(sesi.FetchNextCommand(),
                      war::wfde::impl::WfdeFtpSessionInput::NoInputException);
} ENDCASE
}; //lest

int main( int argc, char * argv[] )
{
    log::LogEngine log;
    log.AddHandler(log::LogToFile::Create("Test_ServerObject.log", true, "file",
        log::LL_TRACE4, log::LA_DEFAULT_ENABLE | log::LA_THREADS) );

    return lest::run( specification, argc, argv );
}
