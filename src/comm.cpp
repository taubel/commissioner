/*
 * MIT License
 *
 * Copyright (c) 2018 8devices
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "comm.hpp"

using namespace ot;
using namespace BorderRouter;

void CommissionerPlugin::ManagerThread(std::future<void> futureObj)
{
	std::condition_variable cv;
	std::mutex cv_m;
	std::unique_lock<std::mutex> lk(cv_m);
	std::cv_status ret;

	std::unique_ptr<Thread> Init;
	std::unique_ptr<Commissioner> Comm;

	while(1)
	{
		if(arguments.mParametersChanged)
		{
			Init.reset(nullptr);
			Comm.reset(new Commissioner(arguments.mPSKcBin, arguments.mSendCommKATxRate, &joiner_list));
			commissioner = Comm.get();
			Init.reset(new Thread(&CommissionerPlugin::CommissionerThread, this, &cv));
			status.Set("Petitioning");

			ret = cv.wait_for(lk, std::chrono::seconds(2));
			cv_m.unlock();
			if(ret == std::cv_status::timeout)
			{
				Init.reset(nullptr);
				status.Set("Timeout");
				arguments.mParametersChanged = false;
				goto cont;
			}
			arguments.mParametersChanged = false;
		}
cont:
		if(futureObj.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
		{
			status.Set("Disabled");
			Init.reset(nullptr);
			Comm.reset(nullptr);
			return;
		}
	}
}

void CommissionerPlugin::CommissionerThread(std::future<void> futureObj, std::condition_variable* cv)
{
	std::cout << "CommissionerThread" << std::endl;
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
    	status.Set("Petitioning error");
		goto exit;
    }
	cv->notify_all();

	status.Set("Active");
	std::cout << "Running commissioner" << std::endl;
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
        	status.Set("Receive error");
            break;
        }
        commissioner->Process(readFdSet, writeFdSet, errorFdSet);

        if(joiner_list.WasListUpdated())
        {
        	commissioner->CommissionerSet(joiner_list.GetSteering());
		commissioner->SetJoiner("DFLT");
        	joiner_list.ListUpdateFinished();
        }

    	if(futureObj.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
    		goto exit;
    }
exit:
	cv->notify_all();
	std::cout << "~CommissionerThread" << std::endl;
	return;
}

static Plugin * CommCreate(PluginManagerCore *core)
{
	CommissionerPlugin* plugin = new CommissionerPlugin;
	if (plugin == NULL) return NULL;

	HttpFramework* u_framework = core->getHttpFramework();
	u_framework->addHandler("POST", "/network", 10, NetworkWrite, reinterpret_cast<void*>(&plugin->arguments));
	u_framework->addHandler("GET", "/network", 10, NetworkRead, reinterpret_cast<void*>(plugin));
	u_framework->addHandler("DELETE", "/network", 10, NetworkDelete, reinterpret_cast<void*>(plugin));

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
	//	TODO destroy http handlers
	delete comm_plugin;
}

extern "C" const plugin_api_t PLUGIN_API =
{
    .version = { 3, 2, 1},
    .create = CommCreate,
	.destroy = CommDestroy,
};

