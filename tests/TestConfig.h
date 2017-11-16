#pragma once

#include <boost/regex.hpp> // g++ don't properly support C++11 regex yet
//#include "../war_tests.h"
#include <wfde/wfde.h>
#include <warlib/WarLog.h>

namespace war {
namespace wfde {
namespace test {

class SubConfig : public Configuration
{
public:
    SubConfig(std::string root, Configuration::ptr_t conf)
    : root_path_(root), conf_(conf)
    {}

    Configuration::ptr_t GetConfigForPath(std::string path) override {
        return std::make_shared<SubConfig>(root_path_ + path, conf_);
    }

    std::string GetValue(const char* path, const char *defaultVal) const override {
        return conf_->GetValue((root_path_ + path).c_str(), defaultVal);
    }
    
    bool HaveValue(const char* path) const override {
        return conf_->HaveValue(path);
    }

    bool CanWrite(const char* path) override { return conf_->CanWrite(); }

    void SetValue(const char* path, std::string value) {
        conf_->SetValue((root_path_ + path).c_str(), value);
    }

    node_enum_t EnumNodes(const char *path) {
        return conf_->EnumNodes((root_path_ + path).c_str());
    }

private:
    const std::string root_path_;
    Configuration::ptr_t conf_;
};

class TestConfig : public Configuration,
public std::enable_shared_from_this<TestConfig>
{
public:
    using data_container_t = std::map<std::string, std::string>;
    TestConfig() = default;
    TestConfig(std::initializer_list<data_container_t::value_type> data)
    : config_{data}
    {
    }
    ~TestConfig() {}

    Configuration::ptr_t GetConfigForPath(std::string path) override
    {
        return std::make_shared<SubConfig>(path, shared_from_this());
    }

    std::string GetValue(const char* path, const char *defaultVal) const override {
        auto it = config_.find(path);
        if (it == config_.end()) {
            if (!defaultVal) {
                LOG_ERROR_FN << "No value for " << log::Esc(path);
                WAR_THROW_T(ExceptionNotFound, "Option");
            }
            return defaultVal;
        }
        return it->second;
    }
    
    virtual bool HaveValue(const char* path) const override {
        return config_.find(path) != config_.end();
    }

    bool CanWrite(const char* path) override { return true; }

    void SetValue(const char* path, std::string value) {
        config_[path] = value;
    }

    node_enum_t EnumNodes(const char *path) override {

        LOG_TRACE4_FN << "Enumerating from: " << log::Esc(path)
            << ". I have " << config_.size() << " nodes.";

        std::set<std::string> known_nodes; // Already added

        const std::string pattern{std::string("^") + path + "/([^/]+)(/(.+))?"};
        const boost::regex pat{ pattern , boost::regex_constants::extended };
        node_enum_t ret;
        for (auto v : config_) {
            boost::cmatch cm;

            LOG_TRACE4_FN << "Comparing "
                << log::Esc(v.first) << " with "
                << log::Esc(pattern);

            if (boost::regex_match(v.first.c_str(), cm, pat)) {
                LOG_TRACE4_FN << "Match!";

                if (known_nodes.insert(cm[1]).second) {
                    ret.push_back({cm[1]});
                }
            }
        }

        LOG_TRACE4_FN << "Returning " << ret.size() << " matches.";

        return ret;
    }

private:
    data_container_t config_;
};

}}} //namespaces
