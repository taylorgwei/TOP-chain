// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xelect/client/xelect_client_process.h"

#include "xbasic/xutility.h"
#include "xcodec/xmsgpack_codec.hpp"
#include "xdata/xblock.h"
#include "xdata/xcodec/xmsgpack/xelection_result_store_codec.hpp"
#include "xdata/xelection/xelection_result_property.h"
#include "xdata/xelection/xelection_result_store.h"
#include "xdata/xgenesis_data.h"
#include "xmbus/xevent_store.h"
#include "xmbus/xevent_timer.h"

#include <cinttypes>

NS_BEG2(top, elect)
using base::xstring_utl;
using data::xblock_ptr_t;
using data::XPORPERTY_CONTRACT_BLOCK_CONTENT_KEY;
using mbus::xevent_ptr_t;
using mbus::xevent_store_t;

xelect_client_process::xelect_client_process(common::xnetwork_id_t const & network_id,
                                             observer_ptr<mbus::xmessage_bus_face_t> const & mb,
                                             elect_update_handler2 cb2,
                                             observer_ptr<time::xchain_time_face_t> const & xchain_timer,
                                             observer_ptr<store::xstore_face_t> const & store)
  : xbase_sync_event_monitor_t(mb), m_network_id{network_id}, m_update_handler2{std::move(cb2)}, m_xchain_timer(xchain_timer), m_store(store) {
    assert(m_network_id.has_value());
    xdbg("xelect_client_process created %p", mb);
    mb->add_listener((int)mbus::xevent_major_type_store, std::bind(&xelect_client_process::push_event, this, std::placeholders::_1));
    mb->add_listener((int)mbus::xevent_major_type_chain_timer, std::bind(&xelect_client_process::push_event, this, std::placeholders::_1));
}

bool xelect_client_process::filter_event(const xevent_ptr_t & e) {
    xinfo("xelect_client_process::filter_event major type %d minor type %d", static_cast<int>(e->major_type), static_cast<int>(e->minor_type));
    switch (e->major_type) {
    case mbus::xevent_major_type_store:
        if (e->minor_type == xevent_store_t::type_block_to_db) {
            auto bme = dynamic_xobject_ptr_cast<mbus::xevent_store_block_to_db_t>(e);
            const std::string & owner = bme->owner;
            if (!(owner == sys_contract_rec_elect_edge_addr || owner == sys_contract_rec_elect_archive_addr || owner == sys_contract_rec_elect_rec_addr ||
                  owner == sys_contract_rec_elect_zec_addr || owner == sys_contract_zec_elect_consensus_addr)) {
                return false;
            }
            // only process light unit
            if (bme->blk_class != base::enum_xvblock_class_light) {
                return false;
            }
        } else {
            xdbg("xelect_client_process ignore event %d", static_cast<int>(e->minor_type));
        }
        return e->minor_type == xevent_store_t::type_block_to_db;
    case mbus::xevent_major_type_chain_timer:
        assert(dynamic_xobject_ptr_cast<mbus::xevent_chain_timer_t>(e));
        return dynamic_xobject_ptr_cast<mbus::xevent_chain_timer_t>(e)->time_block->get_account() == sys_contract_beacon_timer_addr;
    default:
        return false;
    }
}

void xelect_client_process::process_event(const xevent_ptr_t & e) {
    switch (e->major_type) {
    case mbus::xevent_major_type_store:
        process_elect(e);
        break;
    case mbus::xevent_major_type_chain_timer:
        process_timer(e);
        break;
    default:
        break;
    }
}

void xelect_client_process::process_timer(const mbus::xevent_ptr_t & e) {
    assert(dynamic_xobject_ptr_cast<mbus::xevent_chain_timer_t>(e));
    auto const & event = dynamic_xobject_ptr_cast<mbus::xevent_chain_timer_t>(e);
    auto block = event->time_block;
    xdbg("[xelect_client_process::process_timer] update xchain timer to %" PRIu64, block->get_height());
    m_xchain_timer->update_time(block->get_height(), time::xlogic_timer_update_strategy_t::discard_old_value);
}

void xelect_client_process::process_elect(const mbus::xevent_ptr_t & e) {

    auto bme = dynamic_xobject_ptr_cast<mbus::xevent_store_block_to_db_t>(e);
    assert(bme);
    xblock_ptr_t const & block = mbus::extract_block_from(bme);

    // only process light unit, others are filtered
    xassert(block->get_block_class() == base::enum_xvblock_class_light);
    xinfo("xelect_client_process::process_event %s, %" PRIu64, block->get_block_owner().c_str(), block->get_height());
    // TODO(jimmy) db event must stable and order
    xaccount_ptr_t block_state = m_store->get_target_state(block.get());
    if (nullptr == block_state) {
        xerror("xelect_client_process::process_event get target state fail.block=%s", block->dump().c_str());
        return;
    }

    std::string result;
    auto property_names = data::election::get_property_name_by_addr(common::xaccount_address_t{block->get_block_owner()});
    for (auto const & property : property_names) {
        block_state->string_get(property, result);
        if (result.empty()) {
            xerror("[zec election] zone elect finish with empty result. property=%s,block=%s", property.c_str(), block->dump().c_str());
            continue;
        }
        using top::data::election::xelection_result_store_t;
        auto const & election_result_store = codec::msgpack_decode<xelection_result_store_t>({std::begin(result), std::end(result)});
        if (election_result_store.size() == 0) {
            xerror("xelect_client_process::process_event decode property empty, block=%s", block->dump().c_str());
            return;
        }
        xdbg("xelect_client_process::process_event property=%s,block=%s", property.c_str(), block->dump().c_str());

        for (auto const & election_result_info : election_result_store) {
            auto const & network_id = top::get<common::xnetwork_id_t const>(election_result_info);
            if (network_id != m_network_id) {
                continue;
            }

            auto const & election_network_result = top::get<data::election::xelection_network_result_t>(election_result_info);
            for (auto const & election_type_result : election_network_result) {
                auto const node_type = top::get<common::xnode_type_t const>(election_type_result);

                xdbg("start processing %s data", common::to_string(node_type).c_str());

                if (common::has<common::xnode_type_t::consensus>(node_type) || common::has<common::xnode_type_t::consensus_validator>(node_type) ||
                    common::has<common::xnode_type_t::consensus_auditor>(node_type)) {
                    m_update_handler2(election_result_store, common::xconsensus_zone_id, block->get_height(), false);
                } else if (common::has<common::xnode_type_t::zec>(node_type)) {
                    m_update_handler2(election_result_store, common::xzec_zone_id, block->get_height(), false);
                } else if (common::has<common::xnode_type_t::committee>(node_type)) {
                    m_update_handler2(election_result_store, common::xcommittee_zone_id, block->get_height(), false);
                } else if (common::has<common::xnode_type_t::edge>(node_type)) {
                    m_update_handler2(election_result_store, common::xedge_zone_id, block->get_height(), false);
                } else if (common::has<common::xnode_type_t::archive>(node_type)) {
                    m_update_handler2(election_result_store, common::xarchive_zone_id, block->get_height(), false);
                } else {
                    assert(false);
                }
            }
        }
    }
    return;
}

NS_END2
