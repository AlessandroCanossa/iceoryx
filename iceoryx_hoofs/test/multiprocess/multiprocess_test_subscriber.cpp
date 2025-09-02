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

#include "iceoryx_posh/internal/popo/building_blocks/chunk_receiver.hpp"
#include "iceoryx_posh/popo/listener.hpp"
#include "iceoryx_posh/popo/sample.hpp"
#include "iceoryx_posh/popo/subscriber.hpp"
#include "iceoryx_posh/popo/user_trigger.hpp"
#include "iceoryx_posh/runtime/posh_runtime.hpp"
#include "iox/optional.hpp"
#include "iox/signal_watcher.hpp"
#include "topic_data.hpp"

#include <chrono>
#include <csignal>
#include <iostream>

constexpr char APP_NAME[] = "multiprocess-test-subscriber";

void onPointer(iox::popo::Subscriber<PointerTopic>* subscriber)
{
    subscriber->take()
        .and_then([subscriber](const iox::popo::Sample<const PointerTopic>& sample) {
            auto instanceString = subscriber->getServiceDescription().getInstanceIDString();

            if (instanceString == iox::capro::IdString_t("Message"))
            {
                std::cout << "Multiprocess.Message.Pointer received: " << sample->pointer << std::endl;
            }
            if (sample->pointer != sample.get())
            {
                std::cerr << "Virtual pointers are different: " << sample->pointer << " vs " << sample.get()
                          << std::endl;
            }
        })
        .or_else([](const iox::popo::ChunkReceiveResult& error) { std::cerr << "Error: " << error << std::endl; });

    if (0 != std::raise(2))
    {
        std::cerr << "Could not raise SIGINT" << std::endl;
    }
}

int main()
{
    iox::runtime::PoshRuntime::initRuntime(APP_NAME);

    iox::popo::Listener listener;

    iox::popo::Subscriber<PointerTopic> subscriber({"Multiprocess", "Message", "Pointer"});

    listener
        .attachEvent(
            subscriber, iox::popo::SubscriberEvent::DATA_RECEIVED, iox::popo::createNotificationCallback(onPointer))
        .or_else([](auto) {
            std::cerr << "unable to attach subscriber" << std::endl;
            std::exit(EXIT_FAILURE);
        });

    iox::waitForTerminationRequest();

    listener.detachEvent(subscriber, iox::popo::SubscriberEvent::DATA_RECEIVED);

    return (EXIT_SUCCESS);
}
