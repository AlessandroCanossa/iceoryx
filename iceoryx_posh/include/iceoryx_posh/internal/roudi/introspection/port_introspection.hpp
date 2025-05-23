// Copyright (c) 2019 - 2020 by Robert Bosch GmbH. All rights reserved.
// Copyright (c) 2020 - 2022 by Apex.AI Inc. All rights reserved.
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
#ifndef IOX_POSH_ROUDI_INTROSPECTION_PORT_INTROSPECTION_HPP
#define IOX_POSH_ROUDI_INTROSPECTION_PORT_INTROSPECTION_HPP

#include "iceoryx_posh/iceoryx_posh_types.hpp"
#include "iceoryx_posh/internal/popo/ports/publisher_port_data.hpp"
#include "iceoryx_posh/popo/publisher_options.hpp"
#include "iceoryx_posh/popo/subscriber_options.hpp"
#include "iceoryx_posh/roudi/introspection_types.hpp"
#include "iox/assertions.hpp"
#include "iox/atomic.hpp"
#include "iox/detail/periodic_task.hpp"
#include "iox/fixed_position_container.hpp"
#include "iox/function.hpp"

#include <mutex>

#include <map>

namespace iox
{
namespace roudi
{
/// @brief This class handles the port introspection for RouDi.
///        It is recommended to use the PortIntrospectionType alias which sets
///        the intended template parameter required for the actual introspection.
///        The class manages a thread that periodically updates a field with port
///        introspection data to which clients may subscribe.
template <typename PublisherPort, typename SubscriberPort>
class PortIntrospection
{
  private:
    enum class ConnectionState
    {
        DEFAULT,
        SUB_REQUESTED,
        CONNECTED
    };

    using PortIntrospectionTopic = PortIntrospectionFieldTopic;
    using PortThroughputIntrospectionTopic = PortThroughputIntrospectionFieldTopic;

    class PortData
    {
      private:
        struct Dummy
        {
        };
        using PublisherContainerIndexType = typename FixedPositionContainer<Dummy, MAX_PUBLISHERS>::IndexType;
        using ConnectionContainerIndexType = typename FixedPositionContainer<Dummy, MAX_SUBSCRIBERS>::IndexType;

        struct ConnectionInfo;

        /// internal helper classes

        struct PublisherInfo
        {
            PublisherInfo() noexcept
            {
            }

            PublisherInfo(typename PublisherPort::MemberType_t& portData) noexcept
                : portData(&portData)
                , process(portData.m_runtimeName)
                , service(portData.m_serviceDescription)
                , options(portData.m_options)
            {
            }

            typename PublisherPort::MemberType_t* portData{nullptr};
            RuntimeName_t process;
            capro::ServiceDescription service;
            popo::PublisherOptions options;

            /// map from indices to ConnectionContainer indices
            std::map<int, ConnectionContainerIndexType> connectionMap;
            int index{-1};
        };

        struct SubscriberInfo
        {
            SubscriberInfo() noexcept = default;

            SubscriberInfo(typename SubscriberPort::MemberType_t& portData) noexcept
                : portData(&portData)
                , process(portData.m_runtimeName)
                , service(portData.m_serviceDescription)
                , options(portData.m_options)
            {
            }

            typename SubscriberPort::MemberType_t* portData{nullptr};
            RuntimeName_t process;
            capro::ServiceDescription service;
            popo::SubscriberOptions options;
        };

        struct ConnectionInfo
        {
            ConnectionInfo() noexcept = default;

            ConnectionInfo(typename SubscriberPort::MemberType_t& portData) noexcept
                : subscriberInfo(portData)
                , state(ConnectionState::DEFAULT)
            {
            }

            ConnectionInfo(SubscriberInfo& subscriberInfo) noexcept
                : subscriberInfo(subscriberInfo)
                , state(ConnectionState::DEFAULT)
            {
            }

            SubscriberInfo subscriberInfo;
            iox::optional<PublisherContainerIndexType> publisherInfoIndex;
            ConnectionState state{ConnectionState::DEFAULT};

            bool isConnected() const noexcept
            {
                return publisherInfoIndex.has_value() && state == ConnectionState::CONNECTED;
            }
        };

      public:
        PortData() noexcept;

        /// @brief add a publisher port to be tracked by introspection
        /// @param[in] port to be added
        /// @return returns false if the port could not be added and true otherwise
        bool addPublisher(typename PublisherPort::MemberType_t& port) noexcept;

        /// @brief add a subscriber port to be tracked by introspection
        /// @param[in] portData to be added
        /// @return returns false if the port could not be added and true otherwise
        bool addSubscriber(typename SubscriberPort::MemberType_t& portData) noexcept;

        /// @brief remove a publisher port from introspection
        /// @param[in] port publisher port to be removed
        /// @return returns false if the port could not be removed (since it did not exist)
        ///              and true otherwise
        bool removePublisher(const PublisherPort& port) noexcept;

        /// @brief remove a subscriber port from introspection
        /// @param[in] port subscriber port to be removed
        /// @return returns false if the port could not be removed (since it did not exist)
        ///              and true otherwise
        bool removeSubscriber(const SubscriberPort& port) noexcept;

        /// @brief update the state of any connection identified by the capro id of a given message
        ///        according to the message type (e.g. capro::SUB for a subscription request)
        /// @param[in] message capro message to be processed
        /// @return returns false there is no corresponding capro service and true otherwise
        bool updateConnectionState(const capro::CaproMessage& message) noexcept;

        /// @brief update the subscriber connection state identified by the unique port id and the capro id of a given
        /// message according to the message type (e.g. capro::SUB for a subscription request)
        /// @param[in] message capro message to be processed
        /// @param[in] id unique port id
        /// @return false if there is no corresponding capro service or unique port id, otherwise true
        /// @note introduced for identifying the subscriber port whose connection state has to be updated, e.g. if a
        /// subscriber unsubscribes only its connection state should be updated - not the states of all subscribers
        /// which are subscribed to the same topic
        bool updateSubscriberConnectionState(const capro::CaproMessage& message, const popo::UniquePortId& id) noexcept;

        /// @brief prepare the topic to be send based on the internal connection state of all tracked ports
        /// @param[out] topic data structure to be prepared for sending
        void prepareTopic(PortIntrospectionTopic& topic) noexcept;

        void prepareTopic(PortThroughputIntrospectionTopic& topic) noexcept;

        void prepareTopic(SubscriberPortChangingIntrospectionFieldTopic& topic) noexcept;

        /// @brief compute the next connection state based on the current connection state and a capro message type when
        /// the communication policy is OneToMany
        /// @param[in] currentState current connection state (e.g. CONNECTED)
        /// @param[in] messageType capro message type
        /// @return returns the new connection state
        template <typename T, std::enable_if_t<std::is_same<T, iox::build::OneToManyPolicy>::value>* = nullptr>
        PortIntrospection::ConnectionState getNextState(ConnectionState currentState,
                                                        capro::CaproMessageType messageType) noexcept;

        /// @brief compute the next connection state based on the current connection state and a capro message type when
        /// the communication policy is ManyToMany
        /// @param[in] currentState current connection state (e.g. CONNECTED)
        /// @param[in] messageType capro message type
        /// @return returns the new connection state
        template <typename T, std::enable_if_t<std::is_same<T, iox::build::ManyToManyPolicy>::value>* = nullptr>
        PortIntrospection::ConnectionState getNextState(ConnectionState currentState,
                                                        capro::CaproMessageType messageType) noexcept;

        /// @brief indicates whether the logical object state has changed (i.e. the data is new)
        /// @return returns true if the data is new(e.g. new connections were established), false otherwise
        bool isNew() const noexcept;

        /// @brief sets the internal flag indicating new data
        /// @param[in] value value to be set
        void setNew(bool value) noexcept;

      private:
        using PublisherContainer = FixedPositionContainer<PublisherInfo, MAX_PUBLISHERS>;
        using ConnectionContainer = FixedPositionContainer<ConnectionInfo, MAX_SUBSCRIBERS>;

        /// @brief inner map maps from unique port IDs to indices in the PublisherContainer
        std::map<capro::ServiceDescription, std::map<popo::UniquePortId, PublisherContainerIndexType>> m_publisherMap;

        /// inner map maps from unique port IDs to indices in the ConnectionContainer
        std::map<capro::ServiceDescription, std::map<popo::UniquePortId, ConnectionContainerIndexType>> m_connectionMap;

        /// @note we avoid allocating the objects on the heap but can still use a map
        /// to locate/remove them fast(er)
        /// max number needs to be a compile time constant
        PublisherContainer m_publisherContainer;
        ConnectionContainer m_connectionContainer;

        concurrent::Atomic<bool> m_newData;
        std::mutex m_mutex;
    };

    // end of helper classes

  public:
    PortIntrospection() noexcept;

    ~PortIntrospection() noexcept;

    // delete copy constructor and assignment operator
    PortIntrospection(PortIntrospection const&) = delete;
    PortIntrospection& operator=(PortIntrospection const&) = delete;
    // delete move constructor and assignment operator
    PortIntrospection(PortIntrospection&&) = delete;
    PortIntrospection& operator=(PortIntrospection&&) = delete;

    /// @brief add a publisher port to be tracked by introspection
    /// @param[in] port to be added
    /// @return returns false if the port could not be added and true otherwise
    bool addPublisher(typename PublisherPort::MemberType_t& port) noexcept;

    /// @brief add a subscriber port to be tracked by introspection
    /// @param[in] port to be added
    /// @return returns false if the port could not be added and true otherwise
    bool addSubscriber(typename SubscriberPort::MemberType_t& port) noexcept;

    /// @brief remove a publisher port from introspection
    /// @param[in] port publisher port to be removed
    /// @return returns false if the port could not be removed (since it did not exist)
    ///              and true otherwise
    bool removePublisher(const PublisherPort& port) noexcept;

    /// @brief remove a subscriber port from introspection
    /// @param[in] port subscriber port to be removed
    /// @return returns false if the port could not be removed (since it did not exist)
    ///              and true otherwise
    bool removeSubscriber(const SubscriberPort& port) noexcept;

    /// @brief report a capro message to introspection (since this could change the state of active connections)
    /// @param[in] message capro message to be processed
    void reportMessage(const capro::CaproMessage& message) noexcept;

    /// @brief report a capro message to introspection (since this could change the state of active connections)
    /// @param[in] message capro message to be processed
    /// @param[in] id unique port id
    /// @note introduced for identifying the subscriber port whose connection state has to be updated, e.g. if a
    /// subscriber unsubscribes only its connection state should be updated - not the states of all subscribers
    /// which are subscribed to the same topic
    void reportMessage(const capro::CaproMessage& message, const popo::UniquePortId& id) noexcept;

    /// @brief register publisher port used to send introspection
    /// @param[in] publisherPort publisher port to be registered
    /// @return true if registration was successful, false otherwise
    bool registerPublisherPort(PublisherPort&& publisherPortGeneric,
                               PublisherPort&& publisherPortThroughput,
                               PublisherPort&& publisherPortSubscriberPortsData) noexcept;

    /// @brief set the time interval used to send new introspection data
    /// @param[in] interval duration between two send invocations
    void setSendInterval(const units::Duration interval) noexcept;


    /// @brief start the internal send thread
    void run() noexcept;

    /// @brief stop the internal send thread
    void stop() noexcept;

  protected:
    /// @brief sends the port data; this is used from the unittests
    void sendPortData() noexcept;

    /// @brief sends the throughput data; this is used from the unittests
    void sendThroughputData() noexcept;

    /// @brief sends the subscriberport changing data, this is used from the unittests
    void sendSubscriberPortsData() noexcept;

    /// @brief calls the three specific send functions from above, this is used from the periodic task
    void send() noexcept;

  protected:
    optional<PublisherPort> m_publisherPort;
    optional<PublisherPort> m_publisherPortThroughput;
    optional<PublisherPort> m_publisherPortSubscriberPortsData;

  private:
    PortData m_portData;

    units::Duration m_sendInterval{units::Duration::fromSeconds(1U)};
    concurrent::detail::PeriodicTask<function<void()>> m_publishingTask{
        concurrent::detail::PeriodicTaskManualStart, "PortIntr", *this, &PortIntrospection::send};
};

/// @brief typedef for the templated port introspection class that is used by RouDi for the
/// actual port introspection functionality.
using PortIntrospectionType = PortIntrospection<PublisherPortUserType, SubscriberPortUserType>;

} // namespace roudi
} // namespace iox

#include "port_introspection.inl"

#endif // IOX_POSH_ROUDI_INTROSPECTION_PORT_INTROSPECTION_HPP
