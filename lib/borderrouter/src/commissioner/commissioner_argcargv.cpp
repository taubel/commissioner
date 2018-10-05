/*
 *    Copyright (c) 2017-2018, The OpenThread Authors.
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
 *   The file implements the command line params for the commissioner test app.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "commissioner_argcargv.hpp"
#include "common/logging.hpp"
#include "utils/hex.hpp"

namespace ot {
namespace BorderRouter {

/** see: commissioner_argcargv.hpp, processes 1 command line parameter */
int ArgcArgv::ParseArgs(void)
{
    const char *              arg;
    const struct ArgcArgvOpt *opt;

    /* Done? */
    if (mARGx >= mARGC)
    {
        return -1;
    }

    arg = mARGV[mARGx];
    /* consume this one */
    mARGx += 1;

    /* automatically support forms of "-help" */
    if ((0 == strcmp("-h", arg)) || (0 == strcmp("-?", arg)) || (0 == strcmp("-help", arg)) ||
        (0 == strcmp("--help", arg)))
    {
        usage("Help...\n");
        return 1;
    }

    for (opt = &mOpts[0]; opt->name; opt++)
    {
        if (0 == strcmp(arg, opt->name))
        {
            /* found */
            break;
        }
    }

    if (opt->name == NULL)
    {
        /* not found */
        usage("Unknown option: %s", arg);
        return 1;
    }
	if((*(opt->handler))(this, &args))
		return 1;

    return 0;
}

/** see: commissioner_argcargv.hpp, fetch an --ARG STRING pair, storing the result */
const char *ArgcArgv::StrParam(char *puthere, size_t bufsiz)
{
    const char *cp;
    size_t      len;

    mARGx += 1; /* skip parameter name */
    if (mARGx > mARGC)
    {
        usage("Missing: %s VALUE\n", mARGV[mARGx - 2]);
        return NULL;
    }

    cp = mARGV[mARGx - 1];

    /* check length */
    len = strlen(cp);
    len++; /* room for null */
    if (len > bufsiz)
    {
        usage("Too long: %s %s\n", mARGV[mARGx - 2], cp);
        return NULL;
    }

    /* save if requested */
    if (puthere)
    {
        strcpy(puthere, cp);
    }
    return cp;
}

/** see: commissioner_argcargv.hpp, fetch an --ARG HEXSTRING pair, and decode the hex string storing the result */
int ArgcArgv::HexParam(char *ascii_puthere, uint8_t *bin_puthere, int bin_len)
{
    int n;

    if(!StrParam(ascii_puthere, (bin_len * 2) + 1))
    	return 1;

    n = Utils::Hex2Bytes(ascii_puthere, bin_puthere, bin_len);

    /* hex strings must be *complete* */
    if (n != bin_len)
    {
        usage("Param: %s, invalid hex value %s\n", mARGV[mARGx - 2], mARGV[mARGx - 1]);
        return 1;
    }
    return 0;
}

/** see: commissioner_argcargv.hpp, fetch an --ARG NUMBER pair, returns the numeric value */
int ArgcArgv::NumParam(void)
{
    const char *s;
    char *      ep;
    int         v;

    /* fetch as string */
    if((s = StrParam(NULL, 100)) == NULL)
    	return -1;

    /* then convert */
    v = strtol(s, &ep, 0);
    if ((ep == s) || (*ep != 0))
    {
        usage("Not a number: %s %s\n", mARGV[mARGx - 2], mARGV[mARGx - 1]);
        return -1;
    }
    else
    {
    	return v;
    }
}

/** see: commissioner_argcargv.hpp, constructor for the commissioner argc/argv parser */
ArgcArgv::ArgcArgv(int argc, char **argv)
{
    mARGC = argc;
    mARGV = argv;
    mARGx = 1; /* skip the app name */

    memset(&(mOpts[0]), 0, sizeof(mOpts));
}

/** see: commissioner_argcargv.hpp, add an option to be decoded and its handler */
void ArgcArgv::AddOption(const char *name,
                         int (*handler)(ArgcArgv *pThis, CommissionerArgs *args),
                         const char *valuehelp,
                         const char *helptext)
{
    struct ArgcArgvOpt *opt;
    int                 x;

    for (x = 0; x < max_opts; x++)
    {
        opt = &mOpts[x];
        if (opt->name == NULL)
        {
            break;
        }
    }
    if (x >= max_opts)
    {
        otbrLog(OTBR_LOG_ERR, "internal error: Too many cmdline opts!\n");
    }

    opt->name     = name;
    opt->handler  = handler;
    opt->helptext = helptext;
    if (valuehelp == NULL)
    {
        valuehelp = "";
    }
    opt->valuehelp = valuehelp;
}

/** see: commissioner_argcargv.hpp, print error message & application usage */
void ArgcArgv::usage(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fprintf(stderr, "\n");
    fprintf(stderr, "Usage: %s OPTIONS....\n", mARGV[0]);
    fprintf(stderr, "\n");
    fprintf(stderr, "Where OPTIONS are:\n");
    fprintf(stderr, "\n");

    struct ArgcArgvOpt *opt;

    for (opt = &mOpts[0]; opt->name; opt++)
    {
        int n;

        n = fprintf(stderr, "    %s %s", opt->name, opt->valuehelp);
        fprintf(stderr, "%*s%s\n", (30 - n), "", opt->helptext);
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "Note the order of options is important\n");
    fprintf(stderr, "Example, the option --compute-pskc, has prerequistes of\n");
    fprintf(stderr, "   --network-name NAME\n");
    fprintf(stderr, "   --xpanid VALUE\n");
    fprintf(stderr, "   --agent-passphrase VALUE\n");
    fprintf(stderr, "\n");
}

/** Handle commissioner steering data length command line parameter */
static int handle_steering_length(ArgcArgv *pThis, CommissionerArgs *args)
{
    int v;

    if((v = pThis->NumParam()) == -1)
    	return 1;
    if ((v < 1) || (v > 16))
    {
        pThis->usage("invalid steering length: %d", v);
        return 1;
    }

    args->mSteeringLength = v;
    return 0;
}

/** Handle border router ip address on command line */
static int handle_ip_addr(ArgcArgv *pThis, CommissionerArgs *args)
{
    if((pThis->StrParam(args->mAgentAddress_ascii, sizeof(args->mAgentAddress_ascii))) == NULL)
    	return 1;

    return 0;
}

/** Handle border router ip port on command line */
static int handle_ip_port(ArgcArgv *pThis, CommissionerArgs *args)
{
    if((pThis->StrParam(args->mAgentPort_ascii, sizeof(args->mAgentPort_ascii))) == NULL)
    	return 1;

    return 0;
}

/** Handle hex encoded HASHMAC on command line */
static int handle_hashmac(ArgcArgv *pThis, CommissionerArgs *args)
{
    if(pThis->HexParam(args->mJoinerHashmacAscii, args->mJoinerHashmacBin, sizeof(args->mJoinerHashmacBin)))
    	return 1;

    return 0;
}

/** Handle joining device EUI64 on the command line */
static int handle_eui64(ArgcArgv *pThis, CommissionerArgs *args)
{
    if(pThis->HexParam(args->mJoinerEui64Ascii, args->mJoinerEui64Bin, sizeof(args->mJoinerEui64Bin)))
    	return 1;

    return 0;
}

/** Handle the preshared joining credential for the joining device on the command line */
static int handle_pskd(ArgcArgv *pThis, CommissionerArgs *args)
{
    const char *whybad;
    int         ch;
    int         len, x;
    char        pskdAscii[kPSKdLength + 1];

    /* assume not bad */
    whybad = NULL;

    /* get the parameter */
    if((pThis->StrParam(pskdAscii, sizeof(pskdAscii))) == NULL)
    	return 1;

    /*
     * Problem: Should we "base32" decode this per the specification?
     * Answer: No - because this needs to be identical to the CLI application
     * The CLI appication does *NOT* decode preshared key
     * thus we do not decode the base32 value here
     * We do however enforce the data..
     */

    /*
     * Joining Device Credential
     * Specification 1.1.1, Section 8.2 Table 8-1
     * Min Length 6, Max Length 32.
     *
     * Digits 0-9, Upper case only Letters A-Z
     * excluding: I,O,Q,Z
     *
     * Note: 26 letters - 4 illegals = 22 letters.
     * Thus 10 digits + 22 letters = 32 symbols.
     * Thus, "base32" encoding using the above.
     */
    len = strlen(pskdAscii);
    if ((len < 6) || (len > 32))
    {
        whybad = "invalid length (range: 6..32)";
    }
    else
    {
        for (x = 0; x < len; x++)
        {
            ch = pskdAscii[x];

            switch (ch)
            {
            case 'Z':
            case 'I':
            case 'O':
            case 'Q':
                whybad = "Letters I, O, Q and Z are not allowed";
                break;
            default:
                if (isupper(ch) || isdigit(ch))
                {
                    /* all is well */
                }
                else
                {
                    whybad = "contains non-uppercase or non-digit";
                }
                break;
            }
            if (whybad)
            {
                break;
            }
        }
    }

    if (whybad)
    {
        pThis->usage("Illegal PSKd: \"%s\", %s\n", pskdAscii, whybad);
        return 1;
    }

	memcpy(args->mJoinerPSKdAscii, pskdAscii, sizeof(pskdAscii));
	return 0;
}

/** Handle a pre-computed border agent preshared key, the PSKc
 * This is derived from the Networkname, Xpanid & passphrase
 */
static int handle_pskc_bin(ArgcArgv *pThis, CommissionerArgs *args)
{
    if(pThis->HexParam(args->mPSKcAscii, args->mPSKcBin, sizeof(args->mPSKcBin)))
    	return 1;

    args->mHasPSKc = true;
    return 0;
}

/** handle the xpanid command line parameter */
static int handle_xpanid(ArgcArgv *pThis, CommissionerArgs *args)
{
    if(pThis->HexParam(args->mXpanidAscii, args->mXpanidBin, sizeof(args->mXpanidBin)))
    	return 1;

    return 0;
}

/* handle the networkname command line parameter */
static int handle_netname(ArgcArgv *pThis, CommissionerArgs *args)
{
    if((pThis->StrParam(args->mNetworkName, sizeof(args->mNetworkName))) == NULL)
    	return 1;

    return 0;
}

/** handle the border router pass phrase command line parameter */
static int handle_agent_passphrase(ArgcArgv *pThis, CommissionerArgs *args)
{
    if((pThis->StrParam(args->mPassPhrase, sizeof(args->mPassPhrase))) == NULL)
    	return 1;

    return 0;
}

/** Handle log fileanme on the command line */
static int handle_log_filename(ArgcArgv *pThis, CommissionerArgs *args)
{
    (void)args;
    char filename[PATH_MAX];

    if((pThis->StrParam(filename, sizeof(filename))) == NULL)
    	return 1;

    otbrLogSetFilename(filename);
    return 0;
}

/** compute the pskc from command line params */
static int handle_compute_pskc(ArgcArgv *pThis, CommissionerArgs *args)
{
    (void)pThis;
    args->mNeedComputePSKc = true;
    return 0;
}

/** commandline handling for flag that says we commission (not test) */
static int handle_commission_device(ArgcArgv *pThis, CommissionerArgs *args)
{
    (void)pThis;
    args->mNeedCommissionDevice = true;
    return 0;
}

/** compute hashmac of EUI64 on command line */
static int handle_compute_hashmac(ArgcArgv *pThis, CommissionerArgs *args)
{
    (void)pThis;
    args->mNeedComputeJoinerHashMac = true;
    return 0;
}

/** compute steering based on command line */
static int handle_compute_steering(ArgcArgv *pThis, CommissionerArgs *args)
{
    (void)pThis;
    args->mNeedComputeJoinerSteering = true;
    return 0;
}

/** handle debug level on command line */
static int handle_debug_level(ArgcArgv *pThis, CommissionerArgs *args)
{
    (void)args;
    int n;

    if((n = pThis->NumParam()) == -1)
    	return 1;
    if (n < OTBR_LOG_EMERG)
    {
        pThis->usage("invalid log level, must be >= %d\n", OTBR_LOG_EMERG);
        return 1;
    }
	if (n > OTBR_LOG_DEBUG)
	{
		n = OTBR_LOG_DEBUG;
	}
	otbrLogSetLevel(n);
	return 0;
}

/** handle steering allow any on command line */
static int handle_allow_all_joiners(ArgcArgv *pThis, CommissionerArgs *args)
{
    (void)(pThis);
    args->mAllowAllJoiners = true;
    return 0;
}

/** user wants to disable COMM_KA transmissions for test purposes */
static int handle_comm_ka_disabled(ArgcArgv *pThis, CommissionerArgs *args)
{
    (void)(pThis);
    args->mNeedSendCommKA = false;
    return 0;
}

/** user wants to adjust COMM_KA transmission rate */
static int handle_comm_ka_rate(ArgcArgv *pThis, CommissionerArgs *args)
{
    int n;

    if((n = pThis->NumParam()) == -1)
    	return 1;
    /* sanity... */

    /* note: 86400 = 1 day in seconds */
    if ((n < 3) || (n > 86400))
    {
        pThis->usage("comm-ka rate must be (n>3) && (n < 86400), not: %d\n", n);
        return 1;
    }

	args->mSendCommKATxRate = n;
	return 0;
}

/** Adjust total envelope timeout for test automation reasons */
static int handle_comm_envelope_timeout(ArgcArgv *pThis, CommissionerArgs *args)
{
    int n;

    /*
     * This exists because if the COMM_KA keeps going
     * nothing will stop... and test scripts run forever!
     */

    if((n = pThis->NumParam()) == -1)
    	return 1;
    /* between 1 second and 1 day.. */
    if ((n < 1) || (n > (86400)))
    {
        pThis->usage("Invalid envelope time, range: 1 <= n <= 86400, not %d\n", n);
        return 1;
    }
	args->mEnvelopeTimeout = n;
	return 0;
}

/* handle disabling syslog on command line */
static int handle_no_syslog(ArgcArgv *pThis, CommissionerArgs *args)
{
    (void)args;
    (void)pThis;
    otbrLogEnableSyslog(false);
    return 0;
}

/** Called by main(), to process commissioner command line arguments */
int ParseArgs(int aArgc, char **aArgv, CommissionerArgs* user_args)
{
    ArgcArgv args(aArgc, aArgv);

    // default value
    args.args.mEnvelopeTimeout           = 5 * 60; // 5 minutes;
    args.args.mAllowAllJoiners           = false;
    args.args.mNeedSendCommKA            = true;
    args.args.mSendCommKATxRate          = 15; // send keep alive every 15 seconds;
    args.args.mSteeringLength            = kSteeringDefaultLength;
    args.args.mNeedComputePSKc           = false;
    args.args.mNeedComputeJoinerSteering = false;
    args.args.mNeedComputeJoinerHashMac  = false;
    args.args.mNeedCommissionDevice      = true;
    args.args.mHasPSKc                   = false;

    // args.add_option("--selftest", CommissionerCmdLineSelfTest, "", "perform internal selftests");
    args.AddOption("--joiner-eui64", handle_eui64, "VALUE", "joiner EUI64 value");
    args.AddOption("--hashmac", handle_hashmac, "VALUE", "joiner HASHMAC value");
    args.AddOption("--agent-passphrase", handle_agent_passphrase, "VALUE", "Pass phrase for agent");
    args.AddOption("--network-name", handle_netname, "VALUE", "UTF8 encoded network name");
    args.AddOption("--xpanid", handle_xpanid, "VALUE", "xpanid in hex");
    args.AddOption("--pskc-bin", handle_pskc_bin, "VALUE", "Precomputed PSKc in hex notation");
    args.AddOption("--joiner-passphrase", handle_pskd, "VALUE", "PSKd for joiner");
    args.AddOption("--steering-length", handle_steering_length, "NUMBER", "Length of steering data 1..15");
    args.AddOption("--allow-all-joiners", handle_allow_all_joiners, "", "Allow any device to join");
    args.AddOption("--agent-addr", handle_ip_addr, "VALUE", "ip address of border router agent");
    args.AddOption("--agent-port", handle_ip_port, "VALUE", "ip port used by border router agent");
    args.AddOption("--log-filename", handle_log_filename, "FILENAME", "set logfilename");
    args.AddOption("--compute-pskc", handle_compute_pskc, "", "compute and print the pskc from parameters");
    args.AddOption("--compute-hashmac", handle_compute_hashmac, "", "compute and print the hashmac of the given eui64");
    args.AddOption("--compute-steering", handle_compute_steering, "", "compute and print steering data");
    args.AddOption("--comm-ka-disabled", handle_comm_ka_disabled, "", "Disable COMM_KA transmissions");
    args.AddOption("--comm-ka-rate", handle_comm_ka_rate, "", "Set COMM_KA transmission rate");
    args.AddOption("--disable-syslog", handle_no_syslog, "", "Disable log via syslog");
    args.AddOption("--comm-envelope-timeout", handle_comm_envelope_timeout, "VALUE",
                   "Set the total envelope timeout for commissioning");

    args.AddOption("--commission-device", handle_commission_device, "", "Enable device commissioning");

    args.AddOption("--debug-level", handle_debug_level, "NUMBER", "Enable debug output at level VALUE (higher=more)");
    if (aArgc == 1)
    {
        args.usage("No parameters!\n");
        return 1;
    }
    /* parse the args */
    int ret = 0;
    while((ret = args.ParseArgs()) != - 1)
    {
    	if(ret == 1)
    		return 1;
    }

    memcpy(user_args, &args.args, sizeof(CommissionerArgs));
    return 0;
}

} // namespace BorderRouter
} // namespace ot
