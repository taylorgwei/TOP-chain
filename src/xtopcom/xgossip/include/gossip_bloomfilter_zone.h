// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#if 0
#pragma once
#include <map>
#include <chrono>
#include <mutex>
#include "xtransport/transport.h"
#include "xgossip/gossip_interface.h"
#include "xpbase/base/top_timer.h"

namespace top {

namespace gossip {

// notice: no dtor!!
class GossipBloomfilterZone : public GossipInterface {
public:
    explicit GossipBloomfilterZone(transport::TransportPtr transport_ptr);
    virtual ~GossipBloomfilterZone();
    virtual void Broadcast(
            uint64_t local_hash64,
            transport::protobuf::RoutingMessage& message,
            std::shared_ptr<std::vector<kadmlia::NodeInfoPtr>> neighbors);

    // just for performance test
    virtual void BroadcastWithNoFilter(
            const std::string& local_id,
            transport::protobuf::RoutingMessage& message,
            const std::vector<kadmlia::NodeInfoPtr>& neighbors);

    void Start();

private:
    void CleanTimerProc();
    int GetGossipNodes(const std::shared_ptr<base::Uint64BloomFilter>& bloomfilter,
        const std::vector<kadmlia::NodeInfoPtr>& neighbors,
        std::vector<kadmlia::NodeInfoPtr>& tmp_neighbors);
    int GetZoneNodes(const std::shared_ptr<base::Uint64BloomFilter>& bloomfilter,
        const std::vector<kadmlia::NodeInfoPtr>& neighbors,
        std::vector<kadmlia::NodeInfoPtr>& tmp_neighbors,
        const transport::protobuf::RoutingMessage& message);
    int SendMessage(const std::shared_ptr<base::Uint64BloomFilter>& bloomfilter,
            transport::protobuf::RoutingMessage& message,
            std::vector<kadmlia::NodeInfoPtr>& rest_random_neighbors);
    int SendZoneMessage(const std::shared_ptr<base::Uint64BloomFilter>& bloomfilter,
            transport::protobuf::RoutingMessage& message,
            std::vector<kadmlia::NodeInfoPtr>& rest_random_neighbors);
    int SendLessZoneMessage(const std::shared_ptr<base::Uint64BloomFilter>& bloomfilter,
            transport::protobuf::RoutingMessage& message,
            std::vector<kadmlia::NodeInfoPtr>& rest_random_neighbors,
            int zone);
    int SendFourthZone(transport::protobuf::RoutingMessage& message,
            std::vector<kadmlia::NodeInfoPtr>& neighbors);

private:
    std::mutex m_mutex;
    struct GossipInfo {
        std::chrono::steady_clock::time_point timepoint;
        std::string bloomfilter;
    };
    std::map<uint32_t, GossipInfo> m_gossip_map;  // record gossip msg_id
    base::TimerRepeated m_clean_timer{base::TimerManager::Instance(), "GossipBloomfilterZone_cleaner"};
    DISALLOW_COPY_AND_ASSIGN(GossipBloomfilterZone);
};

}  // namespace gossip

}  // namespace top
#endif