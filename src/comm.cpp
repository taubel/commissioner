/*
 * comm.cpp
 *
 *  Created on: Oct 1, 2018
 *      Author: tautvydas
 */
//	TODO autorius, licenzija...

#include "comm.hpp"

using namespace ot;
using namespace BorderRouter;

void CommissionerPlugin::ThreadManager(std::future<void> futureObj)
{
	std::condition_variable cv;
	std::mutex cv_m;
	std::unique_lock<std::mutex> lk(cv_m);
	std::cv_status ret;

	std::unique_ptr<Thread> Init;
	std::unique_ptr<Thread> Run;
	std::unique_ptr<Commissioner> Comm;

//	TODO pabaigti statusus, vietoje StatusCode reiketu grazinti kazkoki teksta kuris butu labiau descriptive

	while(1)
	{
		if(arguments.mParametersChanged)
		{
			Run.reset(nullptr);
			Comm.reset(nullptr);

			Comm.reset(new Commissioner(arguments.mPSKcBin, arguments.mSendCommKATxRate));
			commissioner = Comm.get();
			Init.reset(new Thread(&CommissionerPlugin::InitCommissioner, this, &cv));

//			TODO spurious wakeup
//			TODO InitCommissioner gali iskviesti notify_all pries ThreadManager pasiekiant wait_for. Galima pagalba aprasyta std::condition_variable reference puslapy
			ret = cv.wait_for(lk, std::chrono::seconds(2));
			cv_m.unlock(); //	TODO:	issiaiskinti kas cia vyksta
			Init.reset(nullptr);
			if(ret == std::cv_status::timeout)
			{
				status.Set(StatusCode::server_error_internal_server_error);
				arguments.mParametersChanged = false;
				goto cont;
			}
			Run.reset(new Thread(&CommissionerPlugin::RunCommissioner, this));
			status.Set(StatusCode::success_ok);
			arguments.mParametersChanged = false;
		}
cont:
		if(futureObj.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
		{
			Init.reset(nullptr);
			Run.reset(nullptr);
			Comm.reset(nullptr);
			return;
		}
	}
}

void CommissionerPlugin::InitCommissioner(std::future<void> futureObj, std::condition_variable* cv)
{
	std::cout << "Initializing commissioner" << std::endl;
    int ret;

    srand(time(0));
    commissioner->InitDtls(arguments.mAgentAddress_ascii, arguments.mAgentPort_ascii);
    do
    {
    	if(futureObj.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
    	{
    		std::cout << __func__ << " timeout" << std::endl;
    		goto exit;
    	}
    	ret = commissioner->TryDtlsHandshake();
    }
    while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);
    if (commissioner->IsValid())
    {
        commissioner->CommissionerPetition();
    }
    else
    {
    	status.Set(StatusCode::server_error_internal_server_error);
    }
exit:
	cv->notify_all();
	std::cout << "~Initializing commissioner" << std::endl;
	return;
}

void CommissionerPlugin::RunCommissioner(std::future<void> futureObj)
{
	std::cout << "Running commissioner thread" << std::endl;
    while (commissioner->IsValid())
    {
        int            maxFd   = -1;
        struct timeval timeout = {1, 0};
        int            rval;

        fd_set readFdSet;
        fd_set writeFdSet;
        fd_set errorFdSet;

        FD_ZERO(&readFdSet);
        FD_ZERO(&writeFdSet);
        FD_ZERO(&errorFdSet);
        commissioner->UpdateFdSet(readFdSet, writeFdSet, errorFdSet, maxFd, timeout);
        rval = select(maxFd + 1, &readFdSet, &writeFdSet, &errorFdSet, &timeout);
        if (rval < 0)
        {
            otbrLog(OTBR_LOG_ERR, "select() failed", strerror(errno));
            status.Set(StatusCode::server_error_internal_server_error);
            break;
        }
        commissioner->Process(readFdSet, writeFdSet, errorFdSet);

        if(joiner_list.WasListUpdated())
        {
        	commissioner->CommissionerSet(joiner_list.GetSteering());
        	const char* last_pskd = joiner_list.GetLastPskd();
        	if(last_pskd)
        		commissioner->SetJoiner(last_pskd);
        	joiner_list.ListUpdateFinished();
        }

    	if(futureObj.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
    		goto exit;
    }
exit:
	std::cout << __func__ << " return" << std::endl;
	return;
}

static Plugin * CommCreate(PluginManagerCore *core)
{
	CommissionerPlugin* plugin = new CommissionerPlugin;
	if (plugin == NULL) return NULL;

	HttpFramework* u_framework = core->getHttpFramework();
	u_framework->addHandler("POST", "/network", 10, NetworkWrite, reinterpret_cast<void*>(&plugin->arguments));
	u_framework->addHandler("GET", "/network", 10, NetworkRead, reinterpret_cast<void*>(&plugin->arguments));
	u_framework->addHandler("GET", "/status", 10, StatusRead, reinterpret_cast<void*>(&plugin->status));
	u_framework->addHandler("POST", "/restart", 10, Restart, reinterpret_cast<void*>(plugin));

	u_framework->addHandler("POST", "/joiners", 9, JoinersWrite, reinterpret_cast<void*>(&plugin->joiner_list));
	u_framework->addHandler("PUT", "/joiners/*", 10, JoinersEdit, reinterpret_cast<void*>(&plugin->joiner_list));
	u_framework->addHandler("DELETE", "/joiners", 9, JoinersClear, reinterpret_cast<void*>(&plugin->joiner_list));
	u_framework->addHandler("DELETE", "/joiners/*", 10, JoinersRemove, reinterpret_cast<void*>(&plugin->joiner_list));
	u_framework->addHandler("GET", "/joiners", 10, JoinersRead, reinterpret_cast<void*>(&plugin->joiner_list));

    std::cout << __func__ << std::endl;
    return plugin;
}

static void CommDestroy(Plugin* plugin)
{
	CommissionerPlugin* comm_plugin = reinterpret_cast<CommissionerPlugin*>(plugin);
	delete comm_plugin;
//	TODO sunaikinti handlerius
}

extern "C" const plugin_api_t PLUGIN_API =
{
    .version = { 3, 2, 1},
    .create = CommCreate,
	.destroy = CommDestroy,
};

