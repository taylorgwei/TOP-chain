﻿// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>
#include <vector>
#include "xvledger/xreceiptid.h"
#include "xblockmaker/xblockmaker_face.h"

NS_BEG2(top, blockmaker)

class xlightunit_builder_para_t : public xblock_builder_para_face_t {
 public:
    xlightunit_builder_para_t(const std::vector<xcons_transaction_ptr_t> & origin_txs, const base::xreceiptid_state_ptr_t & receiptid_state, const xblockmaker_resources_ptr_t & resources)
    : xblock_builder_para_face_t(resources), m_origin_txs(origin_txs), m_receiptid_state(receiptid_state) {}
    virtual ~xlightunit_builder_para_t() {}

    const std::vector<xcons_transaction_ptr_t> &    get_origin_txs() const {return m_origin_txs;}
    const std::vector<xcons_transaction_ptr_t> &    get_fail_txs() const {return m_fail_txs;}
    void                                            set_fail_tx(const xcons_transaction_ptr_t & tx) {m_fail_txs.push_back(tx);}
    void                                            set_fail_txs(const std::vector<xcons_transaction_ptr_t> & txs) {m_fail_txs = txs;}
    const base::xreceiptid_state_ptr_t &            get_receiptid_state() const {return m_receiptid_state;}
 private:
    std::vector<xcons_transaction_ptr_t>        m_origin_txs;
    std::vector<xcons_transaction_ptr_t>        m_fail_txs;
    base::xreceiptid_state_ptr_t                m_receiptid_state;
};

class xlightunit_builder_t : public xblock_builder_face_t {
 public:
    xlightunit_builder_t();
    virtual xblock_ptr_t        build_block(const xblock_ptr_t & prev_block,
                                            const xaccount_ptr_t & prev_state,
                                            const data::xblock_consensus_para_t & cs_para,
                                            xblock_builder_para_ptr_t & build_para);
 private:
    void    alloc_tx_receiptid(const std::vector<xcons_transaction_ptr_t> & input_txs, const base::xreceiptid_state_ptr_t & receiptid_state);
};

class xfullunit_builder_t : public xblock_builder_face_t {
 public:
    virtual xblock_ptr_t        build_block(const xblock_ptr_t & prev_block,
                                            const xaccount_ptr_t & prev_state,
                                            const data::xblock_consensus_para_t & cs_para,
                                            xblock_builder_para_ptr_t & build_para);
};

class xemptyunit_builder_t : public xblock_builder_face_t {
 public:
    virtual xblock_ptr_t        build_block(const xblock_ptr_t & prev_block,
                                            const xaccount_ptr_t & prev_state,
                                            const data::xblock_consensus_para_t & cs_para,
                                            xblock_builder_para_ptr_t & build_para);
};

#if 0// TODO(jimmy)
class xtop_lightunit_builder2 : public xblock_builder_face_t {
public:
    xblock_ptr_t build_block(xblock_ptr_t const & prev_block,
                             xaccount_ptr_t const & prev_state,
                             data::xblock_consensus_para_t const & cs_para,
                             xblock_builder_para_ptr_t & build_para) override;

private:
    void alloc_tx_receiptid(const std::vector<xcons_transaction_ptr_t> & input_txs, const base::xreceiptid_state_ptr_t & receiptid_state);
};
using xlightunit_builder2_t = xtop_lightunit_builder2;
#endif
NS_END2
