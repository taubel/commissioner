/*
 *    Copyright (c) 2017, The OpenThread Authors.
 *    All rights reserved.
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *    3. Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *    POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   The file implements the Thread border agent.
 */

#include "border_agent.hpp"

#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "border_agent.hpp"
#include "dtls.hpp"
#include "ncp.hpp"
#include "uris.hpp"
#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/tlv.hpp"
#include "common/types.hpp"
#include "utils/hex.hpp"

namespace ot {

namespace BorderRouter {

static const char   kBorderAgentServiceType[] = "_meshcop._udp."; ///< Border agent service type of mDNS
static const size_t kMaxSizeOfPacket          = 1500;             ///< Max size of packet in bytes.

/**
 * Locators
 *
 */
enum
{
    kAloc16Leader   = 0xfc00, ///< leader anycast locator.
    kInvalidLocator = 0xffff, ///< invalid locator.
};

/**
 * UDP ports
 *
 */
enum
{
    kBorderAgentUdpPort = 49191, ///< Thread commissioning port.
};

BorderAgent::BorderAgent(Ncp::Controller *aNcp)
    : mPublisher(Mdns::Publisher::Create(AF_UNSPEC, NULL, NULL, HandleMdnsState, this))
    , mNcp(aNcp)
    , mThreadStarted(false)
{
}

otbrError BorderAgent::Start(void)
{
    otbrError           error = OTBR_ERROR_NONE;
    struct sockaddr_in6 sin6;

    memset(&sin6, 0, sizeof(sin6));
    sin6.sin6_family = AF_INET6;
    sin6.sin6_port   = htons(kBorderAgentUdpPort);

    mSocket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    VerifyOrExit(mSocket != -1, error = OTBR_ERROR_ERRNO);
    SuccessOrExit(bind(mSocket, reinterpret_cast<struct sockaddr *>(&sin6), sizeof(sin6)));

    mNetworkName[sizeof(mNetworkName) - 1] = '\0';

    mNcp->On(Ncp::kEventExtPanId, HandleExtPanId, this);
    mNcp->On(Ncp::kEventThreadState, HandleThreadState, this);
    mNcp->On(Ncp::kEventNetworkName, HandleNetworkName, this);

    mNcp->RequestEvent(Ncp::kEventNetworkName);
    mNcp->RequestEvent(Ncp::kEventExtPanId);
    mNcp->RequestEvent(Ncp::kEventThreadState);

    mNcp->On(Ncp::kEventUdpProxyStream, SendToCommissioner, this);

exit:
    if (error != OTBR_ERROR_NONE)
    {
        otbrLog(OTBR_LOG_ERR, "Failed to start border agent: %s!", otbrErrorString(error));
    }

    return error;
}

BorderAgent::~BorderAgent(void)
{
}

void BorderAgent::HandleMdnsState(Mdns::State aState)
{
    switch (aState)
    {
    case Mdns::kStateReady:
        PublishService();
        break;
    default:
        otbrLog(OTBR_LOG_WARNING, "Mdns service not available!");
        break;
    }
}

void BorderAgent::SendToCommissioner(void *aContext, int aEvent, va_list aArguments)
{
    struct sockaddr_in6 sin6;
    const uint8_t *     packet   = va_arg(aArguments, const uint8_t *);
    uint16_t            length   = static_cast<uint16_t>(va_arg(aArguments, unsigned int));
    uint16_t            peerPort = static_cast<uint16_t>(va_arg(aArguments, unsigned int));
    const in6_addr *    addr     = va_arg(aArguments, const in6_addr *);
    uint16_t            sockPort = static_cast<uint16_t>(va_arg(aArguments, unsigned int));

    (void)aEvent;
    assert(aEvent == Ncp::kEventUdpProxyStream);
    VerifyOrExit(sockPort == kBorderAgentUdpPort);

    memset(&sin6, 0, sizeof(sin6));
    sin6.sin6_family = AF_INET6;
    memcpy(sin6.sin6_addr.s6_addr, addr->s6_addr, sizeof(sin6.sin6_addr));
    sin6.sin6_port = htons(peerPort);

    {
        ssize_t sent = sendto(static_cast<BorderAgent *>(aContext)->mSocket, packet, length, 0,
                              reinterpret_cast<const sockaddr *>(&sin6), sizeof(sin6));
        VerifyOrExit(sent == static_cast<ssize_t>(length), perror("send to commissioner"));
    }

    otbrLog(OTBR_LOG_INFO, "Sent to commissioner");

exit:
    return;
}

void BorderAgent::UpdateFdSet(fd_set & aReadFdSet,
                              fd_set & aWriteFdSet,
                              fd_set & aErrorFdSet,
                              int &    aMaxFd,
                              timeval &aTimeout)
{
    (void)aWriteFdSet;
    (void)aErrorFdSet;
    (void)aTimeout;

    FD_SET(mSocket, &aReadFdSet);

    if (mSocket > aMaxFd)
    {
        aMaxFd = mSocket;
    }
}

void BorderAgent::Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet)
{
    uint8_t             packet[kMaxSizeOfPacket];
    struct sockaddr_in6 sin6;
    ssize_t             len     = sizeof(packet);
    socklen_t           socklen = sizeof(sin6);

    (void)aWriteFdSet;
    (void)aErrorFdSet;

    VerifyOrExit(FD_ISSET(mSocket, &aReadFdSet));

    len = recvfrom(mSocket, packet, sizeof(packet), 0, reinterpret_cast<struct sockaddr *>(&sin6), &socklen);
    VerifyOrExit(len > 0);

    mNcp->UdpProxySend(packet, static_cast<uint16_t>(len), ntohs(sin6.sin6_port), sin6.sin6_addr, kBorderAgentUdpPort);

exit:
    return;
}

void BorderAgent::PublishService(void)
{
    char xpanid[sizeof(mExtPanId) * 2 + 1];

    assert(mNetworkName[0] != '\0');
    Utils::Bytes2Hex(mExtPanId, sizeof(mExtPanId), xpanid);
    mPublisher->PublishService(kBorderAgentUdpPort, mNetworkName, kBorderAgentServiceType, "nn", mNetworkName, "xp",
                               xpanid, NULL);
}

void BorderAgent::StartPublishService(void)
{
    otbrLog(OTBR_LOG_INFO, "Start publishing service");

    if (mPublisher->IsStarted())
    {
        PublishService();
    }
    else
    {
        mPublisher->Start();
    }
}

void BorderAgent::StopPublishService(void)
{
    otbrLog(OTBR_LOG_INFO, "Stop publishing service");

    if (mPublisher->IsStarted())
    {
        mPublisher->Stop();
    }
}

void BorderAgent::HandleThreadChange(void)
{
    otbrLog(OTBR_LOG_INFO, "Handle Thread change");

    if (mThreadStarted && mNetworkName[0] != '\0')
    {
        StartPublishService();
    }
    else
    {
        StopPublishService();
    }
}

void BorderAgent::SetThreadStarted(bool aStarted)
{
    mThreadStarted = aStarted;
    HandleThreadChange();
}

void BorderAgent::SetNetworkName(const char *aNetworkName)
{
    strncpy(mNetworkName, aNetworkName, sizeof(mNetworkName) - 1);
    // have to restart publisher to publish new service name.
    mPublisher->Stop();
    HandleThreadChange();
}

void BorderAgent::SetExtPanId(const uint8_t *aExtPanId)
{
    memcpy(mExtPanId, aExtPanId, sizeof(mExtPanId));
    HandleThreadChange();
}

void BorderAgent::HandleThreadState(void *aContext, int aEvent, va_list aArguments)
{
    assert(aEvent == Ncp::kEventThreadState);

    int started = va_arg(aArguments, int);
    static_cast<BorderAgent *>(aContext)->SetThreadStarted(started);
}

void BorderAgent::HandleNetworkName(void *aContext, int aEvent, va_list aArguments)
{
    assert(aEvent == Ncp::kEventNetworkName);

    const char *networkName = va_arg(aArguments, const char *);
    static_cast<BorderAgent *>(aContext)->SetNetworkName(networkName);
}

void BorderAgent::HandleExtPanId(void *aContext, int aEvent, va_list aArguments)
{
    assert(aEvent == Ncp::kEventExtPanId);

    const uint8_t *xpanid = va_arg(aArguments, const uint8_t *);
    static_cast<BorderAgent *>(aContext)->SetExtPanId(xpanid);
}

} // namespace BorderRouter

} // namespace ot
