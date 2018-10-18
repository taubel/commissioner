
#include <iostream>
#include <list>
#include <dlfcn.h>

#include "plugin_api.hpp"
#include "ulfius_http_framework.hpp"

class BasicConfig: public Config
{
public:
    BasicConfig(void) { };
};

class BasicLwM2MFramework: public LwM2MFramework
{
public:
    Config & getConfig(void) { return mConfig; };

protected:
    BasicConfig mConfig;
};

class BasicPluginCore: public PluginCore
{
public:
    BasicPluginCore(void): mHttp(12345) { };
    
    LwM2MFramework & getLwM2M(void) { return mLwM2M; };
    HttpFramework & getHttp(void) { return mHttp; };

protected:
    BasicLwM2MFramework mLwM2M;
    UlfiusHttpFramework mHttp;
};

class PluginManager
{
public:
    PluginManager(PluginCore &aCore);

    bool load(const char *aFile, Config &aConfig);
    void unloadAll(void);
    
protected:
    PluginCore &mCore;
    std::list<Plugin *> mPlugins;
};

PluginManager::PluginManager(PluginCore &aCore):
    mCore(aCore),
    mPlugins()
{
    std::cout << "Initializing PluginManager" << std::endl;
};

bool PluginManager::load(const char *aFile, Config &aConfig)
{
    std::cout << "Loading '" << aFile << "'..." << std::endl;
    void *dll = dlopen(aFile, RTLD_NOW | RTLD_LOCAL);
    //std::cout << "dll = " << dll << std::endl; 

    if (dll == NULL)
    {
        std::cout << "Error opening dll!" << std::endl << dlerror() << std::endl;
        return false;
    }

    // XXX: maybe use dlvsym???
    plugin_api_t *func = static_cast<plugin_api_t *>(dlsym(dll, PLUGIN_HANDLE_NAME));
    //std::cout << "func = " << dll << std::endl;

    if (func == NULL)
    {
        return false;
    }

    plugin_version_t ver = func->papi_version;
    // TODO: check version
    std::cout << "version: v" << ver.pv_major << "." << ver.pv_minor << "." << ver.pv_revision << std::endl;

    Plugin *plugin = func->papi_create(mCore, aConfig);

    if (plugin == NULL)
    {
        std::cout << "Failed to create plugin instance!" << std::endl;
        return false;
    }
    
    std::cout << "Created plugin " << plugin << std::endl;

    mPlugins.push_back(plugin);

    return true;
}

void PluginManager::unloadAll(void)
{
    std::cout << "PluginManager::unloadAll" << std::endl;

    while (!mPlugins.empty())
    {
        Plugin *p = mPlugins.back();
        mPlugins.pop_back();
        std::cout << "Deleting plugin:" << p << std::endl;
        delete p;
    }
}

/* Application */
int main(int argc, const char *argv[])
{
    if (argc < 2)
    {
        std::cout << "Please provide plugin file names" << std::endl;
        return 1;
    }

    BasicPluginCore core;
    PluginManager pm(core);

    for (int i = 1; i < argc; i++)
    {
        const char *name = argv[i];
        BasicConfig config;
        bool status = pm.load(name, config);
        if (status)
        {
            std::cout << "Loaded '" << name << "' plugin'" << std::endl;
        }
        else
        {
            std::cout << "Failed to load '" << name << "'! " << std::endl;
        }
    }

    UlfiusHttpFramework& u_framework = (UlfiusHttpFramework&)core.getHttp();
    u_framework.startFramework();

    while(1);

    std::cout << "Unloading all plugins..." << std::endl;

    pm.unloadAll();

    std::cout << "Finished cleanup" << std::endl;

    return 0;
}
