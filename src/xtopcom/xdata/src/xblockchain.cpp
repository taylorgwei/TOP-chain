// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xdata/xblockchain.h"
#include "xbase/xint.h"
#include "xbase/xmem.h"
#include "xbase/xutl.h"
#include "xbasic/xversion.h"
#include "xconfig/xconfig_register.h"
#include "xdata/xblocktool.h"
#include "xdata/xdata_common.h"
#include "xdata/xdata_error.h"
#include "xdata/xfullunit.h"
#include "xdata/xgenesis_data.h"
#include "xdata/xlightunit.h"
#include "xdata/xfull_tableblock.h"
#include "xdata/xaccount_cmd.h"
#include "xconfig/xpredefined_configurations.h"

#include <assert.h>
#include <string>
#include <vector>

namespace top {
namespace data {

REG_CLS(xblockchain2_t);

xblockchain2_t::xblockchain2_t(uint32_t chainid, const std::string & account, base::enum_xvblock_level level) : m_account(account), m_block_level(level) {
    m_bstate = make_object_ptr<base::xvbstate_t>(account, (uint64_t)0, (uint64_t)0, std::string(), std::string(), (uint64_t)0, (uint32_t)0, (uint16_t)0);
    add_modified_count();
}

xblockchain2_t::xblockchain2_t(const std::string & account, base::enum_xvblock_level level) : m_account(account), m_block_level(level) {
    m_bstate = make_object_ptr<base::xvbstate_t>(account, (uint64_t)0, (uint64_t)0, std::string(), std::string(), (uint64_t)0, (uint32_t)0, (uint16_t)0);
    add_modified_count();
}

xblockchain2_t::xblockchain2_t(const std::string & account) : m_account(account), m_block_level(base::enum_xvblock_level_unit) {
    m_bstate = make_object_ptr<base::xvbstate_t>(account, (uint64_t)0, (uint64_t)0, std::string(), std::string(), (uint64_t)0, (uint32_t)0, (uint16_t)0);
    add_modified_count();
}

xblockchain2_t::xblockchain2_t() {}

int32_t xblockchain2_t::do_write(base::xstream_t & stream) {
    KEEP_SIZE();
    SERIALIZE_FIELD_BT(m_version);
    SERIALIZE_FIELD_BT(m_account);
    SERIALIZE_FIELD_BT(m_block_level);
    SERIALIZE_FIELD_BT(m_last_state_block_height);
    SERIALIZE_FIELD_BT(m_last_state_block_hash);
    SERIALIZE_FIELD_BT(m_property_confirm_height);
    SERIALIZE_FIELD_BT(m_last_full_block_height);
    SERIALIZE_FIELD_BT(m_last_full_block_hash);
    SERIALIZE_FIELD_BT(m_ext);
    SERIALIZE_FIELD_BT(m_create_time);

    std::string bstate_bin;
    m_bstate->serialize_to_string(bstate_bin);
    stream << bstate_bin;
    xdbg("jimmy xblockchain2_t::do_write account=%s,height=%ld,hash=%s,bstate_size=%zu,bstate_hash=%ld",
        m_account.c_str(), m_last_state_block_height, base::xstring_utl::to_hex(m_last_state_block_hash).c_str(), bstate_bin.size(), base::xhash64_t::digest(bstate_bin));

    xobject_ptr_t<base::xvbstate_t> bstate2 = make_object_ptr<base::xvbstate_t>(m_account, (uint64_t)0, (uint64_t)0, std::string(), std::string(), (uint64_t)0, (uint32_t)0, (uint16_t)0);
    bstate2->serialize_from_string(bstate_bin);

    return CALC_LEN();
}

int32_t xblockchain2_t::do_read(base::xstream_t & stream) {
    KEEP_SIZE();
    DESERIALIZE_FIELD_BT(m_version);
    DESERIALIZE_FIELD_BT(m_account);
    DESERIALIZE_FIELD_BT(m_block_level);
    DESERIALIZE_FIELD_BT(m_last_state_block_height);
    DESERIALIZE_FIELD_BT(m_last_state_block_hash);
    DESERIALIZE_FIELD_BT(m_property_confirm_height);
    DESERIALIZE_FIELD_BT(m_last_full_block_height);
    DESERIALIZE_FIELD_BT(m_last_full_block_hash);
    DESERIALIZE_FIELD_BT(m_ext);
    DESERIALIZE_FIELD_BT(m_create_time);

    std::string bstate_bin;
    stream >> bstate_bin;
    xdbg("jimmy xblockchain2_t::do_read account=%s,height=%ld,hash=%s,bstate_size=%zu,bstate_hash=%ld",
        m_account.c_str(), m_last_state_block_height, base::xstring_utl::to_hex(m_last_state_block_hash).c_str(), bstate_bin.size(), base::xhash64_t::digest(bstate_bin));
    m_bstate->serialize_from_string(bstate_bin);

    return CALC_LEN();
}

xtransaction_ptr_t xblockchain2_t::make_transfer_tx(const std::string & to, uint64_t amount, uint64_t firestamp, uint16_t duration, uint32_t deposit, const std::string& token_name) {
    xtransaction_ptr_t tx = make_object_ptr<xtransaction_t>();
    // data::xproperty_asset asset(token_name, amount);
    // tx->make_tx_transfer(asset);
    // tx->set_last_trans_hash_and_nonce(account_send_trans_hash(), account_send_trans_number());
    // tx->set_different_source_target_address(get_account(), to);
    // tx->set_fire_timestamp(firestamp);
    // tx->set_expire_duration(duration);
    // tx->set_deposit(deposit);
    // tx->set_digest();
    // tx->set_len();

    xassert(false);
    // update account send tx hash and number
    // set_account_send_trans_hash(tx->digest());
    // set_account_send_trans_number(tx->get_tx_nonce());
    return tx;
}

xtransaction_ptr_t xblockchain2_t::make_run_contract_tx(const std::string & to,
                                                        const std::string & func_name,
                                                        const std::string & func_param,
                                                        uint64_t amount,
                                                        uint64_t firestamp,
                                                        uint16_t duration,
                                                        uint32_t deposit) {
    xtransaction_ptr_t tx = make_object_ptr<xtransaction_t>();
    // data::xproperty_asset asset(amount);
    // tx->make_tx_run_contract(asset, func_name, func_param);
    // tx->set_last_trans_hash_and_nonce(account_send_trans_hash(), account_send_trans_number());
    // tx->set_different_source_target_address(get_account(), to);
    // tx->set_fire_timestamp(firestamp);
    // tx->set_expire_duration(duration);
    // tx->set_deposit(deposit);
    // tx->set_digest();
    // tx->set_len();
    xassert(false);
    // update account send tx hash and number
    // set_account_send_trans_hash(tx->digest());
    // set_account_send_trans_number(tx->get_tx_nonce());
    return tx;
}

bool xblockchain2_t::add_light_unit(const xblock_t * block) {
    const xlightunit_block_t * unit = dynamic_cast<const xlightunit_block_t *>(block);
    if (unit == nullptr) {
        xerror("xblockchain2_t::add_light_unit block=%s", block->dump().c_str());
    }

    std::string binlog = unit->get_property_binlog();
    xdbg("xblockchain2_t::add_light_unit apply_changes_of_binlog account=%s,height=%ld,binlog_size=%zu",
        block->get_account().c_str(), block->get_height(), binlog.size());
    if (!binlog.empty()) {
        bool ret = m_bstate->apply_changes_of_binlog(binlog);
        xassert(ret);
    }
    return true;
}

bool xblockchain2_t::add_full_unit(const xblock_t * block) {
    const xfullunit_block_t * fullunit = dynamic_cast<const xfullunit_block_t *>(block);
    if (fullunit == nullptr) {
        xerror("xblockchain2_t::add_full_unit block=%s", block->dump().c_str());
    }

    std::string snapshot = fullunit->get_property_snapshot();
    m_bstate = make_object_ptr<base::xvbstate_t>(*block);
    m_bstate->serialize_from_string(snapshot);  // TODO(jimmy)

    m_last_full_block_height = block->get_height();
    m_last_full_block_hash = block->get_block_hash();
    return true;
}

bool xblockchain2_t::add_light_table(const xblock_t* block) {
    xassert(block->is_lighttable());
    const xtable_block_t* light_table = dynamic_cast<const xtable_block_t*>(block);
    xassert(light_table != nullptr);

    // TODO(jimmy) unconfirmed accounts should come from table mbt
    std::set<std::string> unconfirmed_accounts = get_unconfirmed_accounts();
    auto & units = block->get_tableblock_units(false);
    if (!units.empty()) {
        for (auto & unit : units) {
            if (!unit->is_emptyblock()) {
                auto account_name = unit->get_account();
                if (unit->get_unconfirm_sendtx_num() > 0) {
                    unconfirmed_accounts.insert(account_name);
                } else {
                    unconfirmed_accounts.erase(account_name);
                }
            }
        }
        set_unconfirmed_accounts(unconfirmed_accounts);
        xdbg("xblockchain2_t::set_unconfirmed_accounts size=%zu, block=%s", unconfirmed_accounts.size(), block->dump().c_str());
    }
    return true;
}

bool xblockchain2_t::add_full_table(const xblock_t* block) {
    m_last_full_block_height = block->get_height();
    m_last_full_block_hash = block->get_block_hash();
    return true;
}

xobject_ptr_t<xblockchain2_t> xblockchain2_t::clone_state() {
    std::string stream_str;
    serialize_to_string(stream_str);
    xobject_ptr_t<xblockchain2_t> blockchain = make_object_ptr<xblockchain2_t>(get_account());
    blockchain->serialize_from_string(stream_str);
    return blockchain;
}

bool xblockchain2_t::apply_block(const xblock_t* block) {
    xdbg("jimmybstate xblockchain2_t::apply_block block=%s", block->dump().c_str());
    bool ret = true;
    if (block == nullptr) {
        xerror("xblockchain2_t::apply_block block is null");
    }

    if (block->is_genesis_block()) {
        m_bstate = make_object_ptr<base::xvbstate_t>(*block);
    }

    if (block->is_lightunit()) {
        ret = add_light_unit(block);
    } else if (block->is_fullunit()) {
        ret = add_full_unit(block);
    }
    if (!ret) {
        xerror("xblockchain2_t::apply_block fail.block=%s", block->dump().c_str());
        return ret;
    }

    update_block_height_hash_info(block);

    if ( (block->get_block_level() == base::enum_xvblock_level_unit)
        && (block->is_genesis_block() || block->get_height() == 1) ) {
        update_account_create_time(block);
    }
    add_modified_count();
    return true;
}

void xblockchain2_t::update_block_height_hash_info(const xblock_t * block) {
    m_last_state_block_height = block->get_height();
    m_last_state_block_hash = block->get_block_hash();
    if (block->is_genesis_block() || block->is_fullblock()) {
        m_last_full_block_height = block->get_height();
        m_last_full_block_hash = block->get_block_hash();
    }
}

void xblockchain2_t::update_account_create_time(const xblock_t * block) {
    if (block->get_block_level() != base::enum_xvblock_level_unit) {
        xassert(false);
        return;
    }

    if (block->is_genesis_block()) {
        // if the genesis block is not the nil block, it must be a "genesis account".
        // the create time of genesis account should be set to the gmtime of genesis block
        if (!get_account_create_time() && block->get_header()->get_block_class() != base::enum_xvblock_class_nil) {
            set_account_create_time(block->get_cert()->get_gmtime());
            xdbg("xblockchain2_t::update_account_create_time,address:%s account_create_time:%ld", block->get_account().c_str(), get_account_create_time());
        }
    } else if (block->get_height() == 1) {
        // the create time of non-genesis account should be set to the gmtime of height#1 block
        if (!get_account_create_time()) {
            set_account_create_time(block->get_cert()->get_gmtime());
            xdbg("xblockchain2_t::update_account_create_time,address:%s account_create_time:%ld", block->get_account().c_str(), get_account_create_time());
        }
    } else {
        xassert(false);
    }
}

bool xblockchain2_t::update_state_by_genesis_block(const xblock_t * block) {
    return apply_block(block);
}

bool xblockchain2_t::update_state_by_next_height_block(const xblock_t * block) {
    return apply_block(block);
}

std::string xblockchain2_t::to_basic_string() const {
    std::stringstream ss;
    ss << "{";
    ss << ",state_height=" << m_last_state_block_height;
    ss << ",state_hash=" << base::xstring_utl::to_hex(m_last_state_block_hash);
    ss << "}";
    return ss.str();
}

uint64_t xblockchain2_t::get_free_tgas() const {
    uint64_t total_asset = balance() + lock_balance() + tgas_balance() + disk_balance() + vote_balance();
    if (total_asset >= XGET_ONCHAIN_GOVERNANCE_PARAMETER(min_free_gas_balance)) {
        return XGET_ONCHAIN_GOVERNANCE_PARAMETER(free_gas);
    } else {
        return 0;
    }
}

// how many tgas you can get from pledging 1TOP
uint32_t xblockchain2_t::get_token_price(uint64_t onchain_total_pledge_token) {
    uint64_t initial_total_pledge_token = XGET_ONCHAIN_GOVERNANCE_PARAMETER(initial_total_locked_token);
    xinfo("tgas_disk get total pledge token from beacon: %llu, %llu", initial_total_pledge_token, onchain_total_pledge_token);
    uint64_t total_pledge_token = onchain_total_pledge_token + initial_total_pledge_token;
    return XGET_ONCHAIN_GOVERNANCE_PARAMETER(total_gas_shard) * XGET_ONCHAIN_GOVERNANCE_PARAMETER(validator_group_count) * TOP_UNIT / total_pledge_token;
}

uint64_t xblockchain2_t::get_total_tgas(uint32_t token_price) const {
    uint64_t pledge_token = tgas_balance();
    uint64_t total_tgas = pledge_token * token_price / TOP_UNIT + get_free_tgas();
    uint64_t max_tgas;
    // contract account, max tgas is different
    if (is_user_contract_address(common::xaccount_address_t{m_account})) {
        max_tgas = XGET_ONCHAIN_GOVERNANCE_PARAMETER(max_gas_contract);
    } else {
        max_tgas = XGET_ONCHAIN_GOVERNANCE_PARAMETER(max_gas_account);
    }
    return std::min(total_tgas, max_tgas);
}

uint64_t xblockchain2_t::get_last_tx_hour() const {
    std::string v;
    string_get(XPROPERTY_LAST_TX_HOUR_KEY, v);
    if (!v.empty()) {
        return (uint64_t)std::stoull(v);
    }
    return 0;
}

uint64_t xblockchain2_t::get_used_tgas() const {
    std::string v;
    string_get(XPROPERTY_USED_TGAS_KEY, v);
    if (!v.empty()) {
        return (uint64_t)std::stoull(v);
    }
    return 0;
}

uint64_t xblockchain2_t::calc_decayed_tgas(uint64_t timer_height) const {
    uint32_t last_hour = get_last_tx_hour();
    uint64_t used_tgas{0};
    uint32_t decay_time = XGET_ONCHAIN_GOVERNANCE_PARAMETER(usedgas_decay_cycle);
    if (timer_height <= last_hour) {
        used_tgas = get_used_tgas();
    } else if (timer_height - last_hour < decay_time) {
        used_tgas = (decay_time - (timer_height - last_hour)) * get_used_tgas() / decay_time;
    }
    return used_tgas;
}

uint64_t xblockchain2_t::get_available_tgas(uint64_t timer_height, uint32_t token_price) const {
    uint64_t used_tgas = calc_decayed_tgas(timer_height);
    uint64_t total_tgas = get_total_tgas(token_price);
    uint64_t available_tgas{0};
    if (total_tgas > used_tgas) {
        available_tgas = total_tgas - used_tgas;
    }
    return available_tgas;
}

// TODO(jimmy) delete
void xblockchain2_t::set_unconfirmed_accounts(const std::set<std::string> & accounts) {
    if (accounts.empty()) {
        m_ext.erase(enum_blockchain_ext_type_uncnfirmed_accounts);
    } else {
        base::xstream_t stream(base::xcontext_t::instance());
        stream << accounts;
        const std::string accounts_str((const char*)stream.data(),stream.size());
        m_ext[enum_blockchain_ext_type_uncnfirmed_accounts] = accounts_str;
    }
}

const std::set<std::string> xblockchain2_t::get_unconfirmed_accounts() const {
    std::set<std::string> accounts;
    auto iter = m_ext.find(enum_blockchain_ext_type_uncnfirmed_accounts);
    if (iter == m_ext.end()) {
        return accounts;
    }

    base::xstream_t stream(base::xcontext_t::instance(), (uint8_t*)(iter->second.data()), (uint32_t)(iter->second.size()));
    stream >> accounts;
    xdbg("xblockchain2_t::get_unconfirmed_accounts size=%zu", accounts.size());
    return accounts;
}

void xblockchain2_t::set_extend_data(uint16_t name, const std::string & value) {
    m_ext[name] = value;
}

std::string xblockchain2_t::get_extend_data(uint16_t name) {
    auto iter = m_ext.find(name);
    if (iter != m_ext.end()) {
        return iter->second;
    }
    return std::string();
}

bool xblockchain2_t::string_get(const std::string& prop, std::string& value) const {
    if (false == get_bstate()->find_property(prop)) {
        xwarn("xblockchain2_t::string_get fail-find property.account=%s,propname=%s", get_account().c_str(), prop.c_str());
        return false;
    }
    auto propobj = get_bstate()->load_string_var(prop);
    if (nullptr == propobj) {
        xerror("xblockchain2_t::string_get fail-find load string var.account=%s,propname=%s", get_account().c_str(), prop.c_str());
        return false;
    }
    value = propobj->query();
    return true;
}

bool xblockchain2_t::deque_get(const std::string& prop, std::deque<std::string> & deque) const {
    if (false == get_bstate()->find_property(prop)) {
        xwarn("xblockchain2_t::deque_get fail-find property.account=%s,propname=%s", get_account().c_str(), prop.c_str());
        return false;
    }
    auto propobj = get_bstate()->load_string_deque_var(prop);
    if (nullptr == propobj) {
        xerror("xblockchain2_t::deque_get fail-find load deque var.account=%s,propname=%s", get_account().c_str(), prop.c_str());
        return false;
    }
    deque = propobj->query();
    return true;
}

bool xblockchain2_t::map_get(const std::string& prop, std::map<std::string, std::string> & map) const {
    if (false == get_bstate()->find_property(prop)) {
        xwarn("xblockchain2_t::map_get fail-find property.account=%s,propname=%s", get_account().c_str(), prop.c_str());
        return false;
    }
    auto propobj = get_bstate()->load_string_map_var(prop);
    if (nullptr == propobj) {
        xerror("xblockchain2_t::map_get fail-find load map var.account=%s,propname=%s", get_account().c_str(), prop.c_str());
        return false;
    }
    map = propobj->query();
    return true;
}

uint64_t xblockchain2_t::token_get(const std::string& prop) const {
    if (false == get_bstate()->find_property(prop)) {
        return 0;
    }
    auto propobj = get_bstate()->load_token_var(prop);
    base::vtoken_t balance = propobj->get_balance();
    if (balance < 0) {
        xerror("xblockchain2_t::token_get fail-should not appear. balance=%ld", balance);
        return 0;
    }
    return (uint64_t)balance;
}

uint64_t xblockchain2_t::uint64_property_get(const std::string& prop) const {
    if (false == get_bstate()->find_property(prop)) {
        return 0;
    }
    auto propobj = get_bstate()->load_uint64_var(prop);
    uint64_t value = propobj->get();
    return value;
}

std::string xblockchain2_t::native_map_get(const std::string & prop, const std::string & field) const {
    if (false == get_bstate()->find_property(prop)) {
        return {};
    }
    auto propobj = get_bstate()->load_string_map_var(prop);
    return propobj->query(field);
}

uint32_t xblockchain2_t::get_unconfirm_sendtx_num() const {
    std::string value = native_map_get(XPROPERTY_TX_INFO, XPROPERTY_TX_INFO_UNCONFIRM_TX_NUM);
    if (value.empty()) {
        return 0;
    }
    return base::xstring_utl::touint32(value);
}
uint64_t xblockchain2_t::get_latest_send_trans_number() const {
    std::string value = native_map_get(XPROPERTY_TX_INFO, XPROPERTY_TX_INFO_LATEST_SENDTX_NUM);
    if (value.empty()) {
        return 0;
    }
    return base::xstring_utl::touint64(value);
}

uint64_t xblockchain2_t::account_recv_trans_number() const {
    std::string value = native_map_get(XPROPERTY_TX_INFO, XPROPERTY_TX_INFO_RECVTX_NUM);
    if (value.empty()) {
        return 0;
    }
    return base::xstring_utl::touint64(value);
}

uint256_t xblockchain2_t::account_send_trans_hash() const {
    std::string value = native_map_get(XPROPERTY_TX_INFO, XPROPERTY_TX_INFO_LATEST_SENDTX_HASH);
    if (value.empty()) {
        uint256_t default_value;
        return default_value;
    }
    return uint256_t((uint8_t*)value.c_str());
}
uint64_t xblockchain2_t::account_send_trans_number() const {
    return get_latest_send_trans_number();
}

}  // namespace data
}  // namespace top
