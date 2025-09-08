// Copyright (c) 2019 by Robert Bosch GmbH. All rights reserved.
// Copyright (c) 2021 - 2022 by Apex.AI Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "iox/detail/system_configuration.hpp"

#include "iox/assertions.hpp"
#include "iox/file_reader.hpp"
#include "iox/logging.hpp"
#include "iox/posix_call.hpp"

#include <cmath>
#include <cstdint>

namespace iox
{
namespace detail
{
uint64_t pageSize() noexcept
{
    // sysconf fails when one provides an invalid name parameter. _SC_PAGESIZE
    // is a valid name parameter therefore it should never fail.
    return static_cast<uint64_t>(IOX_POSIX_CALL(iox_sysconf)(IOX_SC_PAGESIZE)
                                     .failureReturnValue(-1)
                                     .evaluate()
                                     .or_else([](auto& r) {
                                         IOX_LOG(Fatal, "This should never happen: " << r.getHumanReadableErrnum());
                                         IOX_PANIC("Internal logic error");
                                     })
                                     .value()
                                     .value);
}

uint64_t maxVMAddress() noexcept
{
    const char* vmaddr = std::getenv("IOX_MAX_VIRTUAL_MEMORY_ADDRESS");
    if (vmaddr != nullptr)
    {
        IOX_LOG(Error, "exporting IOX_MAX_VIRTUAL_MEMORY_ADDRESS might break everything, do it at your own peril!");
        return std::stoul(vmaddr);
    }

    constexpr std::string_view stackName = "[stack]";

    iox::FileReader mapsFile("/proc/self/maps");

    std::string line;
    uint64_t maxBits = 0;
    while (mapsFile.readLine(line))
    {
        if (std::string::npos == line.find(stackName, line.size() - stackName.size()))
        {
            continue;
        }
        size_t start = line.find_first_of('-') + 1;
        size_t end = line.find_first_of(' ');
        auto endAddr = "0x" + line.substr(start, end - start);
        maxBits = static_cast<uint64_t>(std::ceil(std::log2(std::stoul(endAddr, nullptr, 16))));
        IOX_LOG(Debug, "Max addr: " << maxBits);
        break;
    }

    if (maxBits == 0)
    {
        IOX_PANIC("Max VM address detection failed");
    }
    return ((1ULL << maxBits) - (1ULL << (maxBits - 7)));
}
} // namespace detail
} // namespace iox
