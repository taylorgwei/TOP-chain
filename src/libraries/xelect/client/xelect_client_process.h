// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.#pragma once

#pragma once

#include "xchain_timer/xchain_timer_face.h"
#include "xcommon/xip.h"
#include "xdata/xelection/xelection_result_store.h"
#include "xmbus/xbase_sync_event_monitor.hpp"
#include "xstore/xstore_face.h"

NS_BEG2(top, elect)

using elect_update_handler2 = std::function<void(data::election::xelection_result_store_t const &, common::xzone_id_t const &, std::uint64_t const, bool starup_flag)>;

class xelect_client_process : public mbus::xbase_sync_event_monitor_t {
public:
    xelect_client_process(common::xnetwork_id_t const & network_id,
                          observer_ptr<mbus::xmessage_bus_face_t> const & mb,
                          elect_update_handler2 cb2,
                          observer_ptr<time::xchain_time_face_t> const & xchain_timer,
                          observer_ptr<store::xstore_face_t> const & store);

protected:
    bool filter_event(const mbus::xevent_ptr_t & e) override;
    void process_event(const mbus::xevent_ptr_t & e) override;

protected:
    void process_timer(const mbus::xevent_ptr_t & e);
    void process_elect(const mbus::xevent_ptr_t & e);

private:
    common::xnetwork_id_t m_network_id;
    elect_update_handler2 m_update_handler2{};
    observer_ptr<time::xchain_time_face_t> m_xchain_timer{nullptr};
    observer_ptr<store::xstore_face_t> m_store{nullptr};
};
NS_END2
