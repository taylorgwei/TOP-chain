// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xbasic/xmemory.hpp"
#include "xcontract_runtime/xtransaction_execution_result.h"
#include "xcontract_runtime/xvm/xruntime_face_fwd.h"
#include "xdata/xcons_transaction.h"
#include "xcontract_common/xcontract_state_fwd.h"
#include "xdata/xtop_action.h"

#include <system_error>

NS_BEG2(top, contract_runtime)

class xtop_session {
private:
    observer_ptr<vm::xruntime_face_t> m_associated_runtime;
    observer_ptr<contract_common::xcontract_state_t> m_contract_state;

public:
    xtop_session(xtop_session const &) = delete;
    xtop_session & operator=(xtop_session const &) = delete;
    xtop_session(xtop_session &&) = default;
    xtop_session & operator=(xtop_session &&) = default;
    ~xtop_session() = default;

    xtop_session(observer_ptr<vm::xruntime_face_t> associated_runtime, observer_ptr<contract_common::xcontract_state_t> contract_state) noexcept;

    xtransaction_execution_result_t execute_transaction(data::xcons_transaction_ptr_t const & tx);
    xtransaction_execution_result_t execute_action(data::xbasic_top_action_t const & action);
};
using xsession_t = xtop_session;

NS_END2
