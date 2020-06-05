/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include <GridMate_Traits_Platform.h>
#include <GridMate/Carrier/Utils.h>
#include <GridMate/Carrier/Driver.h>

#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

namespace GridMate
{
    string Utils::GetMachineAddress(int familyType)
    {
        string machineName;

        struct ifaddrs* ifAddrStruct = nullptr;
        struct ifaddrs* ifa = nullptr;
        void* tmpAddrPtr = nullptr;
        int systemFamilyType = (familyType == Driver::BSD_AF_INET6) ? AF_INET6 : AF_INET;
        getifaddrs(&ifAddrStruct);

        for (ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next)
        {
            if (ifa->ifa_addr->sa_family == systemFamilyType)
            {
                if (systemFamilyType == AF_INET)
                {
                    tmpAddrPtr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
                    char addressBuffer[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
                    machineName = addressBuffer;
                }
                else if (systemFamilyType == AF_INET6)
                {
                    tmpAddrPtr = &((struct sockaddr_in6*)ifa->ifa_addr)->sin6_addr;
                    char addressBuffer[INET6_ADDRSTRLEN];
                    inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
                    machineName = addressBuffer;
                }
                break;
            }
        }
        if (ifAddrStruct != nullptr)
        {
            freeifaddrs(ifAddrStruct);
        }
        return machineName;
    }

    const char* GridMate::Utils::GetBroadcastAddress(int familyType)
    {
        if (familyType == Driver::BSD_AF_INET6)
        {
            return "FF02::1";
        }
        else if (familyType == Driver::BSD_AF_INET)
        {
            return "255.255.255.255";
        }
        else
        {
            return "";
        }
    }
} // namespace GridMate

