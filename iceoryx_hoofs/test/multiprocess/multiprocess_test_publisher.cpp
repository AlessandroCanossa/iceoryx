// Copyright (c) 2021 by Apex.AI Inc. All rights reserved.
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

#include "iceoryx_posh/popo/publisher.hpp"
#include "iceoryx_posh/runtime/posh_runtime.hpp"
#include "iox/signal_watcher.hpp"
#include "topic_data.hpp"

#include <chrono>
#include <csignal>
#include <iostream>

constexpr char APP_NAME[] = "multiprocess-test-publisher";

int sending()
{
    iox::runtime::PoshRuntime::initRuntime(APP_NAME);

    iox::popo::Publisher<PointerTopic> myPublisher({"Multiprocess", "Message", "Pointer"});

    auto loanResult = myPublisher.loan();

    if (!loanResult.has_value())
    {
        std::cerr << "Could not loan result: " << loanResult.error() << std::endl;
        return (EXIT_FAILURE);
    }

    auto& sample = loanResult.value();
    void* pointer = sample.get();

    sample->pointer = pointer;
    std::cout << "Multiprocess.Message.Pointer sending: " << pointer << std::endl;
    sample.publish();

    return (EXIT_SUCCESS);
}

int main()
{
    return sending();
}
