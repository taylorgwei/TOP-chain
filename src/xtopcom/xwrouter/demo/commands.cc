// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "commands.h"

#include <string.h>
#ifdef _WIN32
#include <winsock2.h>
#include <time.h>
#else
#include <sys/time.h>
#endif

#include <algorithm>
#include <iostream>
#include <fstream>
#include <chrono>
#include <utility>
#include <list>
#include <string>

#include "xbase/xutl.h"
#include "xpbase/base/line_parser.h"
#include "xpbase/base/top_log.h"
#include "xpbase/base/top_utils.h"
#include "xpbase/base/check_cast.h"
#include "xpbase/base/xip_parser.h"
#include "xpbase/base/top_string_util.h"
#include "xpbase/base/sem.h"
#include "xpbase/base/kad_key/platform_kadmlia_key.h"
#include "xkad/routing_table/routing_table.h"
#include "xkad/routing_table/callback_manager.h"
#include "xkad/routing_table/local_node_info.h"
#include "xwrouter/register_routing_table.h"
#include "xwrouter/root/root_routing.h"
#include "xwrouter/xwrouter.h"
#include "xpbase/base/xid/xid_def.h"
#include "xpbase/base/xid/xid_parser.h"
#include "xpbase/base/xid/xid_db_session.h"
#include "xpbase/base/kad_key/kadmlia_key.h"
#include "xpbase/base/uint64_bloomfilter.h"
#include "xwrouter/register_message_handler.h"
#include "xpbase/base/redis_client.h"
#include "xgossip/include/gossip_utils.h"
#include "xtransport/udp_transport/transport_util.h"


using namespace top::kadmlia;  // NOLINT

namespace top {
static const uint64_t   kStatMapPrintPeriod = 10ll * 1000ll * 1000ll;

TopCommands::TopCommands()
        : wait_mutex_(),
          wait_cond_var_(),
          init_mutex_(),
          first_node_(false),
          show_cmd_(false),
          inited_(false),
          destroy_(false) {
    wrouter::WrouterRegisterMessageHandler(kTestChainTrade, [this](
    transport::protobuf::RoutingMessage& message,
    base::xpacket_t& packet) {
    HandleTestChainTrade(message, packet);
    TOP_DEBUG("HandleMessage kTestChainTrade");
    });

}

TopCommands::~TopCommands() {
    wrouter::WrouterUnregisterMessageHandler(kTestChainTrade);
    ec_vhost_ = nullptr;
}

// use special impelment in edge/client
TopCommands* TopCommands::Instance() {
    static TopCommands ins;
    return &ins;
}

bool TopCommands::Init(bool first_node, bool show_cmd) {
    TOP_INFO("TopCommands::Init(%s, %s) ...", first_node ? "true" : "false",
        show_cmd ? "true" : "false");
    std::unique_lock<std::mutex> lock(init_mutex_);
    if (inited_) {
        TOP_WARN("TopCommands::Init() before");
        return true;
    }

    show_cmd_ = show_cmd;
    first_node_ = first_node;
    AddBaseCommands();
    AddExtraCommands();
    if (!InitExtra()) {
        TOP_FATAL("init extra failed");
        return false;
    }

    stat_map_timer_ = std::make_shared<base::TimerRepeated>(base::TimerManager::Instance(), "XwrouterDemoTopCommands::stat_map_timer");
    stat_map_timer_->Start(
            1ll * 1000ll * 1000ll,
            kStatMapPrintPeriod,
            std::bind(&TopCommands::PrintStat, this));

    inited_ = true;
    TOP_INFO("TopCommands::Init() success");
    return true;
}


void TopCommands::Run() {
    PrintUsage();

    while (!destroy_) {
        if (!show_cmd_) {
            SleepUs(200000);
            continue;
        }

        std::cout << std::endl << std::endl << "Enter command > ";
        std::string cmdline;
        std::getline(std::cin, cmdline);
        {
            ProcessCommand(cmdline);
        }
    }
    std::cout << "command exited." << std::endl;
}


void TopCommands::PrintUsage() {
    std::cout << "\thelp Print options.\n";
    std::cout << "\tprt Print Local Routing Table.\n";
    std::cout << "\troot Print Local Root-Routing Table.\n";
}

void TopCommands::ProcessCommand(const std::string& cmdline) {
    if (cmdline.empty()) {
        return;
    }

    std::string cmd;
    Arguments args;
    try {
        top::base::LineParser line_split(cmdline.c_str(), ' ', cmdline.size());
        cmd = "";
        for (uint32_t i = 0; i < line_split.Count(); ++i) {
            if (strlen(line_split[i]) == 0) {
                continue;
            }

            if (cmd == "")
                cmd = line_split[i];
            else
                args.push_back(line_split[i]);
        }
    } catch (const std::exception& e) {
        TOP_WARN("Error processing command: %s", e.what());
    }

    std::unique_lock<std::mutex> lock(map_commands_mutex_);
    auto it = map_commands_.find(cmd);
    if (it == map_commands_.end()) {
        std::cout << "Invalid command : " << cmd << std::endl;
        PrintUsage();
    } else {
        (it->second)(args);  // call command procedure
    }
}

void TopCommands::AddBaseCommands() try {
    AddCommand("help", [this](const Arguments& args){
        this->PrintUsage();
    });
    AddCommand("gid", [this](const Arguments& args) {
        const auto gid = global_xid->Get();
        std::cout << "global_xid: " << HexEncode(gid) << std::endl;
    });

    AddCommand("getnode", [this](const Arguments& args) {
        uint32_t xnetwork_id = 10;
        if (args.size() >= 1) {
            xnetwork_id  = check_cast<uint32_t, const char*>(args[0].c_str());
        }
        std::vector<kadmlia::NodeInfoPtr> nodes;
        GetRootNodes(xnetwork_id, nodes);
        for (auto& n : nodes) {
            std::cout << "getnode:" << HexEncode(n->node_id) << " ip:" << n->public_ip << " port:" << n->public_port << std::endl;
        }
    });

    // unregister routing table
    AddCommand("rmrt", [this](const Arguments& args) {
        uint32_t xnetwork_id = 10;
        if (args.size() >= 1) {
            xnetwork_id  = check_cast<uint32_t, const char*>(args[0].c_str());
        }

        auto kad_key = std::make_shared<base::PlatformKadmliaKey>(xnetwork_id, 1);
        auto service_type = kad_key->GetServiceType();

        top::wrouter::UnregisterRoutingTable(service_type);
        std::cout << "unregister routing table: xnetwork_id:" << xnetwork_id << " service_type:" << service_type << std::endl;
    });

    AddCommand("chainctd", [this](const Arguments& args) {
        uint32_t num = 1;
        if (args.size() >= 1) {
            num = check_cast<uint32_t, const char*>(args[0].c_str());
        }
        uint32_t xnetwork_id = 10;
        if (args.size() >= 2) {
            xnetwork_id  = check_cast<uint32_t, const char*>(args[1].c_str());
        }
        bool broadcast = true;
        if (args.size() >= 3) {
            broadcast = check_cast<uint32_t, const char*>(args[2].c_str());
        }
        bool root = false;
        if (xnetwork_id == 1) {
            root = true;
        }
        auto kad_key = std::make_shared<base::PlatformKadmliaKey>(xnetwork_id, 1);
        std::string des_node_id = kad_key->Get();

        for (uint32_t i = 0; i < num; ++i) {
            test_send(des_node_id, broadcast, root);
        }
    });

    AddCommand("chainctdtimer", [this](const Arguments& args) {
        uint32_t num = 1;
        if (args.size() >= 1) {
            num = check_cast<uint32_t, const char*>(args[0].c_str());
        }
        uint32_t xnetwork_id = 10;
        if (args.size() >= 2) {
            xnetwork_id  = check_cast<uint32_t, const char*>(args[1].c_str());
        }
        bool broadcast = true;
        if (args.size() >= 3) {
            broadcast = check_cast<uint32_t, const char*>(args[2].c_str());
        }
        bool root = false;
        if (xnetwork_id == 1) {
            root = true;
        }
        auto kad_key = std::make_shared<base::PlatformKadmliaKey>(xnetwork_id, 1);
        std::string des_node_id = kad_key->Get();

        test_send(des_node_id, broadcast, root);

        auto callback = [&num, &des_node_id, &broadcast, &root, this]() {
                for (uint32_t i = 0; i < num; ++i) {
                    test_send(des_node_id, broadcast, root);
                }
             };

        if (!test_timer_) {
            test_timer_ = std::make_shared<base::TimerRepeated>(base::TimerManager::Instance(), "XwrouterDemoTopCommands::testchain_timer");
        } else {
            test_timer_->Join();
            test_timer_ = nullptr;
            test_timer_ = std::make_shared<base::TimerRepeated>(base::TimerManager::Instance(), "XwrouterDemoTopCommands::testchain_timer");
        }

        uint32_t tnum = 30 * 2; // 2min
        test_timer_->Start(
                1ll * 1000ll * 1000ll,
                100ll * 1000ll,  // 100ms
                callback);
        while(tnum) {
            SleepUs(2000000); // 2 s
            --tnum;
        }
        test_timer_->Join();
        std::cout << "timer send finished" << std::endl;
        test_timer_ = nullptr;
    });

    // test chain trade
    AddCommand("ctd", [this](const Arguments& args) {
        uint64_t mystart = GetCurrentTimeMicSec();
        std::cout << "start: " << mystart << std::endl;
        auto start = std::chrono::system_clock::now();
        transport::protobuf::RoutingMessage message;
        uint32_t num = 1;
        if (args.size() >= 1) {
            num = check_cast<uint32_t, const char*>(args[0].c_str());
        }

        uint32_t backup = 1;
        if (args.size() >= 2) {
            backup = check_cast<uint32_t, const char*>(args[1].c_str());
        }

        uint32_t neighbors_num = 3;
        if (args.size() >= 3) {
            neighbors_num = check_cast<uint32_t, const char*>(args[2].c_str());
        }

        uint32_t stop_times = 3;
        if (args.size() >= 4) {
            stop_times = check_cast<uint32_t, const char*>(args[3].c_str());
        }

        uint32_t max_hop_num = 10;
        if (args.size() >= 5) {
            max_hop_num = check_cast<uint32_t, const char*>(args[4].c_str());
        }

        uint32_t evil_rate = 0;
        if (args.size() >= 6) {
            evil_rate = check_cast<uint32_t, const char*>(args[5].c_str());
        }

        uint32_t gossip_type = 1;
        if (args.size() >= 7) {
            gossip_type = check_cast<uint32_t, const char*>(args[6].c_str());
        }

        uint32_t layer_switch_hop_num = 2;
        if (args.size() >= 8) {
            layer_switch_hop_num = check_cast<uint32_t, const char*>(args[7].c_str());
        }
        uint32_t left_overlap = 0;
        if (args.size() >= 9) {
            left_overlap = check_cast<uint32_t, const char*>(args[8].c_str());
        }
        uint32_t right_overlap = 0;
        if (args.size() >= 10) {
            right_overlap = check_cast<uint32_t, const char*>(args[9].c_str());
        }
        uint32_t des_network_id = 10;
        if (args.size() >= 11) {
            des_network_id = check_cast<uint32_t, const char*>(args[10].c_str());
        }
        bool block_header = false;
        if (args.size() >= 12) {
            block_header = check_cast<bool, const char*>(args[11].c_str());
            std::cout << "using header and block broadcast" << std::endl;
        }

        TestChainTrade(
                message,
                des_network_id,
                num,
                backup,
                neighbors_num,
                stop_times,
                max_hop_num,
                evil_rate,
                gossip_type,
                layer_switch_hop_num,
                left_overlap,
                right_overlap,
                block_header);
        auto end = std::chrono::system_clock::now();
    
        std::chrono::duration<double> diff = end - start;
        std::cout << "time taks:" << diff.count() << std::endl;
    });

    // test chain trade with timer
    AddCommand("ctdtimer", [this](const Arguments& args) {
        uint64_t mystart = GetCurrentTimeMicSec();
        std::cout << "start: " << mystart << std::endl;
        uint32_t num = 1;
        if (args.size() >= 1) {
            num = check_cast<uint32_t, const char*>(args[0].c_str());  // number of gossips for every 200ms
        }

        uint32_t backup = 1;
        if (args.size() >= 2) {
            backup = check_cast<uint32_t, const char*>(args[1].c_str());
        }

        uint32_t neighbors_num = 3;
        if (args.size() >= 3) {
            neighbors_num = check_cast<uint32_t, const char*>(args[2].c_str());
        }

        uint32_t stop_times = 3;
        if (args.size() >= 4) {
            stop_times = check_cast<uint32_t, const char*>(args[3].c_str());
        }

        uint32_t max_hop_num = 10;
        if (args.size() >= 5) {
            max_hop_num = check_cast<uint32_t, const char*>(args[4].c_str());
        }

        uint32_t evil_rate = 0;
        if (args.size() >= 6) {
            evil_rate = check_cast<uint32_t, const char*>(args[5].c_str());
        }

        uint32_t gossip_type = 1;
        if (args.size() >= 7) {
            gossip_type = check_cast<uint32_t, const char*>(args[6].c_str());
        }

        uint32_t layer_switch_hop_num = 2;
        if (args.size() >= 8) {
            layer_switch_hop_num = check_cast<uint32_t, const char*>(args[7].c_str());
        }
        uint32_t left_overlap = 0;
        if (args.size() >= 9) {
            left_overlap = check_cast<uint32_t, const char*>(args[8].c_str());
        }
        uint32_t right_overlap = 0;
        if (args.size() >= 10) {
            right_overlap = check_cast<uint32_t, const char*>(args[9].c_str());
        }
        uint32_t des_network_id = 10;
        if (args.size() >= 11) {
            des_network_id = check_cast<uint32_t, const char*>(args[10].c_str());
        }
        bool block_header = false;
        if (args.size() >= 12) {
            block_header = check_cast<bool, const char*>(args[11].c_str());
            std::cout << "using header and block broadcast:" << block_header << std::endl;
        }

        uint32_t tnum = 15 * 1;   // 
        if (args.size() >= 13) {
            tnum = check_cast<uint32_t, const char*>(args[12].c_str());  // total number of gossips
            // tnum = tnum / num / 5;
            std::cout << "tnum:" << tnum << std::endl;
        }
       

        uint32_t send_count = 0;
        auto callback = [&send_count, &des_network_id, &num, &backup, &neighbors_num,
             &stop_times, &max_hop_num, &evil_rate, &gossip_type, &layer_switch_hop_num, &left_overlap, &right_overlap ,&block_header, this]() {
                 send_count += num;
                 transport::protobuf::RoutingMessage message;
                 TestChainTrade(
                         message,
                         des_network_id,
                         num,
                         backup,
                         neighbors_num,
                         stop_times,
                         max_hop_num,
                         evil_rate,
                         gossip_type,
                         layer_switch_hop_num,
                         left_overlap,
                         right_overlap,
                         block_header);
             };
        if (!test_timer_) {
            test_timer_ = std::make_shared<base::TimerRepeated>(base::TimerManager::Instance(), "XwrouterDemoTopCommands::test_timer");
        }
        test_timer_->Start(
                1ll * 1000ll * 1000ll,
                200ll * 1000ll,  // 200ms
                callback);
        while(1) {
            SleepUs(500000);
            std::cout<<"send_count:"<<send_count<<std::endl;
            if (send_count >= tnum)
                break;
        }
        test_timer_->Join();
        std::cout << "timer send finished:"<<send_count << std::endl;
        test_timer_ = nullptr;

    });
    AddCommand("prt", [this](const Arguments& args){
        uint32_t network_id = 10;
        uint8_t network_type = kRoleEdge;
        if (args.size() >= 1) {
            network_id = check_cast<uint32_t, const char*>(args[0].c_str());
        }
        auto kad_key = std::make_shared<base::PlatformKadmliaKey>();
        kad_key->set_xnetwork_id(network_id);
        kad_key->set_network_type(kRoleEdge); // for platfom, xnetwork_id and xnetwork_type use only
        PrintRoutingTable(kad_key->GetServiceType());
    });
    AddCommand("root", [this](const Arguments& args) {
        uint32_t network_id = kRoot;
        uint8_t network_type = kRoleEdge;
        if (args.size() >= 1) {
            network_id = check_cast<uint32_t, const char*>(args[0].c_str());
        }
        auto kad_key = std::make_shared<base::PlatformKadmliaKey>();
        kad_key->set_xnetwork_id(network_id);
        if (network_id == kRoot) {
            PrintRoutingTable(kRoot, true);
        } else {
            PrintRoutingTable(kad_key->GetServiceType(), true);
        }
    });
 } catch (std::exception& e) {
     std::cout << "catch error: (" << e.what() << ") check_cast failed" << std::endl;
 }

void TopCommands::AddCommand(const std::string& cmd_name, CommandProc cmd_proc) {
    assert(cmd_proc);
    std::unique_lock<std::mutex> lock(map_commands_mutex_);

    auto it = map_commands_.find(cmd_name);
    if (it != map_commands_.end()) {
        TOP_WARN("command(%s) exist and ignore new one", cmd_name.c_str());
        return;
    }

    map_commands_[cmd_name] = cmd_proc;
    TOP_INFO("add command(%s)", cmd_name.c_str());
}

int TopCommands::GetRootNodes(uint64_t service_type) {
    kadmlia::RoutingTablePtr ptr = wrouter::GetRoutingTable(top::kRoot, true);
    wrouter::RootRouting*  routing_table_ptr =
        dynamic_cast<wrouter::RootRouting*>(ptr.get());
    if (!routing_table_ptr) {
        return 1;
    }
    std::vector<kadmlia::NodeInfoPtr> nodes;
    routing_table_ptr->GetRootNodes(service_type, nodes);
    std::cout << "get " << nodes.size() << " of service_type: " << service_type << std::endl;
    for (auto& nptr : nodes) {
        std::cout << "node:" << HexEncode(nptr->node_id)
            << " public_ip:" << nptr->public_ip
            << " public_port:" << nptr->public_port << std::endl;
    }
    return 0;
}

int TopCommands::GetRootNodes(const std::string& hex_target_id) {
    kadmlia::RoutingTablePtr ptr = wrouter::GetRoutingTable(top::kRoot, true);
    wrouter::RootRouting*  routing_table_ptr =
        dynamic_cast<wrouter::RootRouting*>(ptr.get());
    if (!routing_table_ptr) {
        return 1;
    }
    std::vector<kadmlia::NodeInfoPtr> nodes;
    routing_table_ptr->GetRootNodes(HexDecode(hex_target_id), nodes);
    std::cout << "get " << nodes.size() << std::endl;
    for (auto& nptr : nodes) {
        std::cout << "node:" << HexEncode(nptr->node_id)
            << " public_ip:" << nptr->public_ip
            << " public_port:" << nptr->public_port << std::endl;
    }
    return 0;
}

void TopCommands::TestChainTrade(
        transport::protobuf::RoutingMessage& message,
        uint32_t des_network_id,
        uint32_t test_num,
        uint32_t backup,
        uint32_t neighbors_num,
        uint32_t stop_times,
        uint32_t max_hop_num,
        uint32_t evil_rate,
        uint32_t gossip_type,
        uint32_t layer_switch_hop_num,
        uint32_t left_overlap,
        uint32_t right_overlap,
        bool block_header) {
    auto src_kad_key = std::make_shared<base::PlatformKadmliaKey>(10, 1);
    auto src_routing_table = wrouter::GetRoutingTable(src_kad_key->GetServiceType(), false);
    if (!src_routing_table) {
        src_routing_table = wrouter::GetRoutingTable(kRoot, true);
    }

    backup = 1;
    message.set_broadcast(true);
    message.set_src_node_id(src_routing_table->get_local_node_info()->id());
    auto des_kad_key = std::make_shared<base::PlatformKadmliaKey>(des_network_id, 1);
    if (des_network_id == 1) {
        message.set_is_root(true);
    }
    message.set_des_node_id(des_kad_key->Get());
    message.set_type(kTestChainTrade);
    message.set_id(CallbackManager::MessageId());
    message.set_xid(global_xid->Get());

    std::string mdata = "testdata" + RandomString(5);
    std::string header_hash = std::to_string(base::xhash32_t::digest(mdata));
    message.set_data(mdata);

    auto gossip = message.mutable_gossip();
    gossip->set_neighber_count(neighbors_num);
    gossip->set_stop_times(stop_times);
    gossip->set_gossip_type(gossip_type);
    gossip->set_max_hop_num(max_hop_num);
    gossip->set_evil_rate(evil_rate);
//    gossip->set_switch_layer_hop_num(layer_switch_hop_num);
    gossip->set_left_overlap(left_overlap);
    gossip->set_right_overlap(right_overlap);
    gossip->set_ign_bloomfilter_level(0);
    
    if (block_header) {
        gossip->set_block(mdata);
        gossip->set_header_hash(header_hash);
    }
    
    
    std::string data;
    if (!message.SerializeToString(&data)) {
        TOP_WARN("wrouter message SerializeToString failed");
        return;
    }

    uint8_t local_buf[kUdpPacketBufferSize];
    static std::atomic<uint32_t> total_send_count(0);
    uint32_t send_count = 0;

    _xip2_header header;
    memset(&header, 0, sizeof(header));
    std::string xheader((const char*)&header, enum_xip2_header_len);
    std::string xdata = xheader + data;
//    std::cout << "msg.type:" << message.type() << " packet size:" << xdata.size() << std::endl;
    uint32_t looop = test_num * backup;
    std::vector<std::string> msghash_vec;

    uint64_t start2 = GetCurrentTimeMsec();
    base::xpacket_t packet(base::xcontext_t::instance(), local_buf, sizeof(local_buf), 0, false);
    std::string redis_base_key = "sender:" + std::to_string(des_network_id);
    for (uint32_t n = 0; n < looop ; ++n) {
        message.set_id(CallbackManager::MessageId());
        // set send_timestamp in src_service_type temporary
        uint64_t t = GetCurrentTimeMsec();
        message.set_src_service_type(t);

        uint32_t msg_hash = base::xhash32_t::digest(message.xid() + std::to_string(message.id()) + message.data());
        message.mutable_gossip()->set_msg_hash(msg_hash);

        message.SerializeToString(&data);
        xdata = xheader + data;
        packet.reset();
        packet.get_body().push_back((uint8_t*)xdata.data(), xdata.size());
        wrouter::Wrouter::Instance()->send(packet);
        ++send_count;
        ++total_send_count;
        msghash_vec.push_back(std::to_string(msg_hash));

        uint32_t tmp_t = total_send_count;
        TOP_ERROR("send testchaintrade msg_hash:%u header_hash:%s  time:%llu total_send_count:%d", msg_hash,HexEncode(header_hash).c_str(), t, tmp_t);
//        std::cout << "send testchaintrade msg_hash:" << msg_hash << " header_hash:" << HexEncode(header_hash) << " time:" << t << " total_send_count:" << total_send_count << std::endl;
    }
#ifdef USE_REDIS
    uint32_t tmp_total = total_send_count;
    auto redis_cli = base::RedisClient::Instance()->redis_cli();
    if (redis_cli->is_connected()) {
        redis_cli->incrby(redis_base_key + ":total_send_count", tmp_total);
        redis_cli->lpush(redis_base_key + ":msghash", msghash_vec);
        redis_cli->commit();
        msghash_vec.clear();
    }
#endif

    /*
    auto use_time_ms = double(GetCurrentTimeMsec() - start2) / 1000.0;
    std::cout << "send " << send_count << " use time: " << use_time_ms
        << " sec. QPS: " << (uint32_t)((double)send_count / use_time_ms) * neighbors_num 
        << " total_send:" << total_send_count << std::endl;
        */
    if (block_header) {
        transport::protobuf::RoutingMessage message_header(message);
        message_header.clear_bloomfilter();
        message_header.set_hop_num(0);
        // different from block id
        message_header.set_id(CallbackManager::MessageId());
        auto gossip_header = message_header.mutable_gossip();
        gossip_header->clear_msg_hash();
        gossip_header->clear_block();

        gossip_header->set_neighber_count(neighbors_num);
        gossip_header->set_stop_times(stop_times);
        gossip_header->set_gossip_type(1);
        gossip_header->set_max_hop_num(max_hop_num);
        gossip_header->set_evil_rate(evil_rate);
        gossip_header->set_switch_layer_hop_num(layer_switch_hop_num);
        gossip_header->set_left_overlap(left_overlap);
        gossip_header->set_right_overlap(right_overlap);
        gossip_header->set_ign_bloomfilter_level(1);

        gossip_header->set_header_hash(header_hash);

        if (wrouter::Wrouter::Instance()->send(message_header) != 0) {
            std::cout << "send header failed" << std::endl;
            return;
        }
        std::cout << "send header success" << std::endl;
    }
}

void TopCommands::HandleTestChainTrade(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    if (!message.has_broadcast() || !message.broadcast()) {
        //point 2 point
        return;
    }
    uint32_t des_network_id;
    if (message.has_is_root() && message.is_root()) {
        des_network_id = 1; // kRoot
    } else {
        auto kad_key = base::GetKadmliaKey(message.des_node_id());
        des_network_id = kad_key->xnetwork_id();
    }

    std::string data = message.data(); // point2point or broadcast without header and block
    if (message.has_gossip()) {
        // broadcast  with header and block
        auto gossip = message.gossip();
        if (!gossip.has_block() && gossip.has_header_hash()) {
            TOP_DEBUG("HandleRumorMessage header arrive, msg.id(%d)", message.id());
            return;
        }

        if (gossip.has_block()) {
            // broadcast with header and block (block arrive)
            data = gossip.block();
        }
    }

    std::unique_lock<std::mutex> lock(stat_map_mutex_);
    auto gossip = message.gossip();
    PacketInfo pkinfo;
    pkinfo.des_network_id =  des_network_id;
    pkinfo.msg_hash = gossip.msg_hash();
    pkinfo.hop_num = message.hop_num();
    if (message.has_ack_id() && message.ack_id() == 99) {
        // message come from sync
        pkinfo.spread_time_sync = GetCurrentTimeMsec() - message.src_service_type(); // store send time in src_service_type
        pkinfo.spread_time = 0;
    } else {
        pkinfo.spread_time = GetCurrentTimeMsec() - message.src_service_type(); // store send time in src_service_type
        pkinfo.spread_time_sync = 0; 
    }
    pkinfo.msg_src_id = HexEncode(message.src_node_id());
    auto ifind = stat_map_.find(des_network_id);
    if (ifind == stat_map_.end()) {
        auto ptr = std::make_shared<std::vector<PacketInfo>>();
        stat_map_[des_network_id] = ptr;
    }

    //stat_map_[des_network_id]->push_back(pkinfo);

    TOP_ERROR("testchaintrade:msg_hash:%u pkinfo.des_network_id:%d pkinfo.hop_num:%d pkinfo.spread_time:%llu pkinfo.spread_time_sync:%llu pkinfo.msg_src_id:%s",
            pkinfo.msg_hash,
            pkinfo.des_network_id,
            pkinfo.hop_num,
            pkinfo.spread_time,
            pkinfo.spread_time_sync,
            pkinfo.msg_src_id.c_str());
#ifdef USE_REDIS
    uint32_t track = message.gossip().msg_hash() % 100 == 0?true:false;
    std::string redis_base_key = "recver:" + std::to_string(des_network_id) + ":" + HexEncode(global_xid->Get());
    auto redis_cli = base::RedisClient::Instance()->redis_cli();
    if (redis_cli->is_connected()) {
        if (track) {
            // info of each msg
            redis_cli->hset(redis_base_key + ":" + std::to_string(pkinfo.msg_hash), "des_network_id", std::to_string(pkinfo.des_network_id));
            redis_cli->hset(redis_base_key + ":" + std::to_string(pkinfo.msg_hash), "msg_hash", std::to_string(pkinfo.msg_hash));
            redis_cli->hset(redis_base_key + ":" + std::to_string(pkinfo.msg_hash), "hop_num", std::to_string(pkinfo.hop_num));
            redis_cli->hset(redis_base_key + ":" + std::to_string(pkinfo.msg_hash), "spread_time", std::to_string(pkinfo.spread_time));
            redis_cli->hset(redis_base_key + ":" + std::to_string(pkinfo.msg_hash), "spread_time_sync", std::to_string(pkinfo.spread_time_sync));
            // each msg-hash recved by how many nodes
            redis_cli->lpush("recver:" + std::to_string(des_network_id) + ":" + std::to_string(pkinfo.msg_hash), {HexEncode(global_xid->Get())});
        }
        // all recv-msg hash
        redis_cli->lpush(redis_base_key + ":msghash", {std::to_string(pkinfo.msg_hash)});
        redis_cli->commit();
    }

#endif
}

void TopCommands::PrintStat() {
    std::unique_lock<std::mutex> lock(stat_map_mutex_);
    for (auto& item : stat_map_) {
        uint32_t des_network_id = item.first;
        auto ptr = item.second;
        TOP_INFO("stat_map_timer:total_recv_count:%d for des_network_id:%d",
                ptr->size(),
                des_network_id);
        /*
        for (uint32_t i = 0; i < ptr->size(); ++i) {
            TOP_INFO("stat_map_timer:pkinfo.des_network_id:%d pkinfo.msg_hash:%d pkinfo.hop_num:%d pkinfo.spread_time:%llu pkinfo.msg_src_id:%s",
                    ptr->at(i).des_network_id,
                    ptr->at(i).msg_hash,
                    ptr->at(i).hop_num,
                    ptr->at(i).spread_time,
                    ptr->at(i).msg_src_id.c_str());
        }
        */
    }
}

void TopCommands::PrintRoutingTable(uint64_t service_type, bool root) {
    RoutingTablePtr routing_table = wrouter::GetRoutingTable(service_type, root);
    if (!routing_table) {
        TOP_WARN("routing table not registered[%llu]", service_type);
        return;
    }

    LocalNodeInfoPtr local_node = routing_table->get_local_node_info();
    if (!local_node) {
        return;
    }

    auto udp_transport = routing_table->get_transport();
    if (!udp_transport) {
        return;
    }

    std::cout << "self: " << HexEncode(local_node->id()) << ", " <<
        local_node->local_ip() << ":" << local_node->local_port() << ", " <<
        local_node->public_ip() << ":" << local_node->public_port() <<  ", " << std::endl;
//        "[" << local_node->nat_type() << "]" <<
//        ", " << HexEncode(local_node->xip()) << ", xid:" << HexEncode(local_node->xid()) << std::endl;
    std::vector<NodeInfoPtr> nodes = routing_table->GetClosestNodes(
        local_node->id(),
        kRoutingMaxNodesSize);
    if (nodes.empty()) {
        return;
    }
    NodeInfoPtr node = nodes[0];
    for (uint32_t i = 0; i < nodes.size(); ++i) {
        std::cout << HexEncode(nodes[i]->node_id) << ", " <<
            nodes[i]->local_ip << ":" << nodes[i]->local_port << ", " <<
            nodes[i]->public_ip << ":" << nodes[i]->public_port <<  ", "
//            << "[" << nodes[i]->nat_type << "]" << ", " << HexEncode(nodes[i]->xip)
//            << ", xid:" << HexEncode(nodes[i]->xid)
            << "bucket_index:" << nodes[i]->bucket_index <<  std::endl;
    }
    std::cout << "all node size(include self) " << nodes.size() + 1  << std::endl;
}

void TopCommands::GetRootNodes(uint32_t xnetwork_id, std::vector<kadmlia::NodeInfoPtr>& nodes) {
    auto kad_key = std::make_shared<base::PlatformKadmliaKey>(xnetwork_id, 1);
    auto routing = wrouter::GetRoutingTable(kRoot, true);
    auto root = dynamic_cast<wrouter::RootRouting*>(routing.get());
    if (!root)  {
        std::cout << "get kRoot failedl" << std::endl;
        return;
    }
    root->GetRootNodes(kad_key->GetServiceType(), nodes);
}

void TopCommands::test_send(
        const std::string& des_node_id,
        bool broadcast,
        bool root) {
    auto src_kad_key = std::make_shared<base::PlatformKadmliaKey>(10, 1);
    RoutingTablePtr routing_table = wrouter::GetRoutingTable(src_kad_key->GetServiceType(), root);

    if (!routing_table || routing_table->nodes_size() == 0) {
        TOP_DEBUG("get routing table with root.");
        routing_table = wrouter::GetRoutingTable(kRoot, true);
    }

    if (!routing_table) {
        TOP_WARN("send chain msg failed, routing table empty");
        return;
    }

    transport::protobuf::RoutingMessage pbft_message;
    if (broadcast) {
        pbft_message.set_broadcast(true);
    }

    routing_table->SetFreqMessage(pbft_message);
    pbft_message.set_is_root(true);
    if (!root) {
        pbft_message.set_is_root(false);
    }

    pbft_message.set_src_node_id(routing_table->get_local_node_info()->id());
    pbft_message.set_des_node_id(des_node_id);
    pbft_message.set_type(kTestChainTrade);
    pbft_message.set_id(CallbackManager::MessageId());
    pbft_message.set_xid(global_xid->Get());

    uint64_t t = GetCurrentTimeMsec();
    pbft_message.set_src_service_type(t);

    std::string vdata = "testdata" + RandomString(10);
    pbft_message.set_data(vdata);

    uint32_t chain_data_hash = base::xhash32_t::digest(vdata);

    static std::atomic<uint32_t> total_send_count(0);
    ++total_send_count;
    uint32_t tmp_t = total_send_count;

    std::string header_hash = std::to_string(chain_data_hash);
    TOP_ERROR("send testchaintrade msg_hash:%u header_hash:%s  time:%llu total_send_count:%d", chain_data_hash, HexEncode(header_hash).c_str(), t, tmp_t);
    std::cout << "send testchaintrade msg_hash:" << chain_data_hash << " header_hash:" << header_hash << " time:" << t << " total_send_count:" << tmp_t << std::endl;

#ifdef USE_REDIS
    uint32_t track = pbft_message.gossip().msg_hash() % 100 == 0?true:false;
    std::string redis_base_key = "sender:" + HexEncode(des_node_id);
    auto redis_cli = base::RedisClient::Instance()->redis_cli();
    if (redis_cli->is_connected()) {
        redis_cli->incrby(redis_base_key + ":total_send_count", tmp_t);
        redis_cli->lpush(redis_base_key + ":msghash", {std::to_string(chain_data_hash)});
        redis_cli->commit();
    }
#endif

    // point to point
    if (!pbft_message.has_broadcast() || !pbft_message.broadcast()) {
        pbft_message.set_data(vdata);
#ifndef NDEBUG
        std::string debug_info = base::StringUtil::str_fmt(
                "[ec_net]chain message point2point[%d][hash: %u][id: %u][src: %s][des: %s]",
                pbft_message.broadcast(),
                chain_data_hash,
                pbft_message.id(),
                HexEncode(pbft_message.src_node_id()).c_str(),
                HexEncode(pbft_message.des_node_id()).c_str());
        pbft_message.set_debug(debug_info);
        TOP_NETWORK_DEBUG_FOR_PROTOMESSAGE("begin", pbft_message);
#endif // NDEBUG
        if (wrouter::Wrouter::Instance()->send(pbft_message) != 0) {
            TOP_WARN("chain message point2point failed");
            return ;
        }
        return ;
    }

    // broadcast to all use gossip-blooomfilter
    if (pbft_message.is_root()) {
        GossipWithHeaderBlock(
                pbft_message,
                gossip::kGossipBloomfilter,
                chain_data_hash,
                pbft_message.id());
        return;
    }

    GossipWithHeaderBlock(
            pbft_message,
            gossip::kGossipBloomfilterAndLayered,
            chain_data_hash,
            pbft_message.id());
}

void TopCommands::GossipWithHeaderBlock(
        transport::protobuf::RoutingMessage& pbft_message,
        uint32_t block_gossip_type,
        uint32_t chain_data_hash,
        uint32_t chain_msgid) {
    bool large_packet = false;
    if (pbft_message.data().size() > 1500) {
        large_packet = true;
    }
    std::string header_hash = std::to_string(chain_data_hash);

    // broadcast block
    auto gossip_block = pbft_message.mutable_gossip();
    // add msg_hash
    gossip_block->set_msg_hash(chain_data_hash);
    gossip_block->set_neighber_count(4);
    gossip_block->set_stop_times(gossip::kGossipSendoutMaxTimes);
    gossip_block->set_gossip_type(block_gossip_type);
    gossip_block->set_max_hop_num(10);
    gossip_block->set_evil_rate(0);
    if (pbft_message.is_root()) {
        gossip_block->set_switch_layer_hop_num(gossip::kGossipSwitchLayerCount);
        gossip_block->set_ign_bloomfilter_level(gossip::kGossipBloomfilterIgnoreLevel);
    } else {
        gossip_block->set_switch_layer_hop_num(0);
        gossip_block->set_ign_bloomfilter_level(0);
    }
    gossip_block->set_left_overlap(0);
    gossip_block->set_right_overlap(0);
    gossip_block->set_block(pbft_message.data());
    gossip_block->set_header_hash(header_hash);
    pbft_message.clear_data();

#ifndef NDEBUG
    std::string debug_info_block = base::StringUtil::str_fmt(
            "[ec_net]chain message block [is_root: %d][broadcast: %d][hash: %u][header_hash: %s][id: %u][src: %s][des: %s]",
            pbft_message.is_root(),
            pbft_message.broadcast(),
            chain_data_hash,
            HexEncode(header_hash).c_str(),
            chain_msgid,
            HexEncode(pbft_message.src_node_id()).c_str(),
            HexEncode(pbft_message.des_node_id()).c_str());
    pbft_message.set_debug(debug_info_block);
    TOP_NETWORK_DEBUG_FOR_PROTOMESSAGE("begin", pbft_message);
#endif // NDEBUG
    if (wrouter::Wrouter::Instance()->send(pbft_message) != 0) {
        TOP_WARN("chain message block [is_root: %d] broadcast failed", pbft_message.is_root());
        return;
    }

    if (!pbft_message.is_root() && !large_packet) {
        TOP_INFO("not root, not large packet, no need broadcast header");
        return ;
    }

    transport::protobuf::RoutingMessage pbft_message_header(pbft_message);
    pbft_message_header.clear_bloomfilter();
    pbft_message_header.set_hop_num(0);
    // different from block id
    pbft_message_header.set_id(CallbackManager::MessageId());
    auto gossip_header = pbft_message_header.mutable_gossip();
    gossip_header->set_neighber_count(4);
    gossip_header->set_stop_times(gossip::kGossipSendoutMaxTimes);
    gossip_header->set_gossip_type(gossip::kGossipBloomfilter);
    gossip_header->set_max_hop_num(10);
    gossip_header->set_evil_rate(0);
    gossip_header->set_switch_layer_hop_num(gossip::kGossipSwitchLayerCount);
    gossip_header->set_ign_bloomfilter_level(gossip::kGossipBloomfilterIgnoreLevel);
    //gossip_header->set_switch_layer_hop_num(0);
    //gossip_header->set_ign_bloomfilter_level(0);
    gossip_header->set_header_hash(header_hash);
    gossip_header->clear_msg_hash();
    gossip_header->clear_block();

#ifndef NDEBUG
    std::string debug_info_header = base::StringUtil::str_fmt(
            "[ec_net]chain message header [is_root: %d][broadcast: %d][hash: %u][header_hash: %s][id: %u][src: %s][des: %s]",
            pbft_message_header.is_root(),
            pbft_message_header.broadcast(),
            chain_data_hash,
            HexEncode(header_hash).c_str(),
            chain_msgid,
            HexEncode(pbft_message_header.src_node_id()).c_str(),
            HexEncode(pbft_message_header.des_node_id()).c_str());
    pbft_message_header.set_debug(debug_info_header);
    TOP_NETWORK_DEBUG_FOR_PROTOMESSAGE("begin", pbft_message_header);
#endif // NDEBUG
    if (wrouter::Wrouter::Instance()->send(pbft_message_header) != 0) {
        TOP_WARN("chain message header [is_root: %d] broadcast failed", pbft_message_header.is_root());
        return ;
    }
    std::cout << "send header success" << std::endl;
    return ;
}


}  //  namespace top
