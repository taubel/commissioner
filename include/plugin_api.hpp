#ifndef PLUGIN_API_HPP
#define PLUGIN_API_HPP

#define PLUGIN_HANDLE_NAME  "LOREM_IPSUM"

#include "http_framework.hpp"

class Config
{
public:
    virtual ~Config() = 0;
};

class LwM2MFramework
{
public:
    virtual ~LwM2MFramework() = 0;
    virtual Config & getConfig(void) = 0; 
};

class PluginCore
{
public:
    virtual ~PluginCore() = 0;
    virtual LwM2MFramework & getLwM2M(void) = 0;
    virtual HttpFramework & getHttp(void) = 0;
};

class Plugin
{
public:
//    Plugin(PluginCore &aCore, Config &aConfig);
//    Plugin() = delete;
    virtual ~Plugin(void) = 0;
};

Config::~Config()
{
    std::cout << __func__ << " " << this << std::endl;
}

LwM2MFramework::~LwM2MFramework()
{
    std::cout << __func__ << " " << this << std::endl;
}

PluginCore::~PluginCore()
{
    std::cout << __func__ << " " << this << std::endl;
}

Plugin::~Plugin()
{
    std::cout << __func__ << " " << this << std::endl;
}

typedef Plugin * (*plugin_create_t)(PluginCore &core, Config &config);
typedef void (*plugin_destroy_t)(Plugin *plugin);

typedef struct
{
    uint32_t pv_major:8,
             pv_minor:8,
             pv_revision:16;
} plugin_version_t;

typedef struct
{
    plugin_version_t papi_version;
    plugin_create_t  papi_create;
    plugin_destroy_t papi_destroy;
} plugin_api_t;


#endif // PLUGIN_API_HPP
