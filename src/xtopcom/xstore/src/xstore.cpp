// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <assert.h>
#include <inttypes.h>
#include <mutex>
#include <utility>
#include <stack>
#include <string>

#include "xbase/xcontext.h"
#include "xbase/xlog.h"
#include "xbase/xutl.h"
#include "xbase/xhash.h"
#include "xvledger/xvdbkey.h"
#include "xvledger/xvledger.h"
#include "xvledger/xvblockstore.h"
#include "xbasic/xmodule_type.h"
#include "xbase/xobject_ptr.h"
#include "xcrypto/xcrypto_util.h"
#include "xdata/xblock.h"
#include "xdata/xblockchain.h"
#include "xdata/xlightunit.h"
#include "xdata/xproperty.h"
#include "xdata/xpropertylog.h"
#include "xdata/xtableblock.h"
#include "xdata/xfull_tableblock.h"
#include "xdata/xgenesis_data.h"
#include "xdb/xdb_factory.h"

#include "xmbus/xevent_behind.h"
#include "xmbus/xevent_consensus.h"
#include "xmbus/xevent_store.h"
#include "xmetrics/xmetrics.h"
#include "xdata/xaccount_cmd.h"
#include "xstore/xaccount_context.h"
#include "xstore/xstore.h"
#include "xstore/xstore_error.h"

#include "xdata/xgenesis_data.h"
#include "xdata/xtablestate.h"

using namespace top::base;
using namespace top::data;

namespace top {
namespace store {

REG_XMODULE_LOG(chainbase::enum_xmodule_type::xmodule_type_xstore, store::xstore_error_to_string, store::xstore_error_base + 1, store::xstore_error_max);

uint64_t get_gmttime_s() {
    struct timeval val;
    base::xtime_utl::gettimeofday(&val);
    return (uint64_t)val.tv_sec;
}

enum xstore_key_type {
    xstore_key_type_transaction = 0,
    xstore_key_type_property = 1,
    xstore_key_type_property_log = 2,
    xstore_key_type_block = 3,
    xstore_key_type_blockchain = 4,
    xstore_key_type_block_offstate = 5,
    xstore_key_type_max
};

enum xstore_block_type {
    xstore_block_type_none = 0,
    xstore_block_type_header = 1,
    xstore_block_type_input = 2,
    xstore_block_type_output = 3,
    xstore_block_type_max
};

#define ENUM_STR(ENUM) #ENUM

static std::string store_key_type_names[] = {
    ENUM_STR(xstore_key_type_transaction),
    ENUM_STR(xstore_key_type_property),
    ENUM_STR(xstore_key_type_property_log),
    ENUM_STR(xstore_key_type_block),
    ENUM_STR(xstore_key_type_blockchain),
    ENUM_STR(xtore_key_type_max),
};

static std::string store_block_type_names[] = {
    ENUM_STR(xstore_block_type_none),
    ENUM_STR(xstore_block_type_header),
    ENUM_STR(xstore_block_type_input),
    ENUM_STR(xstore_block_type_output),
    ENUM_STR(xstore_block_type_max),
};


xstore::xstore_transaction_t::xstore_transaction_t(std::shared_ptr<db::xdb_face_t> db) : m_db(db) {
    // call db's begin transaction API
    m_db_txn = db->begin_transaction();
    if (m_db_txn == nullptr) {
        xerror("xstore_transaction_t::xstore_transaction_t create transaction error");
    }
}

xstore::xstore_transaction_t::~xstore_transaction_t() {
    // call db's commit transaction api
    if (m_db_txn != nullptr) {
        delete m_db_txn;
    }
}

bool xstore::xstore_transaction_t::do_read(const std::string& key, std::string& value) {
    if (m_db_txn == nullptr) {
        xwarn("xstore_transaction_t::do_read key(%s) failed transaction null", key.c_str());
        return false;
    }
    return m_db_txn->read(key, value);
}

bool xstore::xstore_transaction_t::do_get(const xstore_key_t &key, base::xdataobj_t** obj) const {
    if (m_db_txn == nullptr) {
        xwarn("xstore_transaction_t::do_get key(%s) failed transaction null", key.printable_key().c_str());
        *obj = nullptr;
        return false;
    }

    std::string value;
    bool succ = m_db_txn->read(key.to_db_key(), value);
    if (!succ) {
        xwarn("xstore_transaction_t::do_get key(%s) failed transaction read", key.printable_key().c_str());
        *obj = nullptr;
        return false;
    }

    if (value.empty()) {
        xdbg("xstore_transaction_t::do_get key(%s) successfully value empty", key.printable_key().c_str());
        *obj = nullptr;
        return true;
    }

    base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)value.data(), (uint32_t)value.size());
    base::xdataobj_t *data = base::xdataobj_t::read_from(stream);
    xassert(nullptr != data);
    if (nullptr == data) {
        xwarn("xstore_transaction_t::do_get, object unserialize fail, key(%s)", key.printable_key().c_str());
        data->release_ref();
        *obj = nullptr;
        return false;
    }

    *obj = data;
    return true;
}

bool xstore::xstore_transaction_t::do_write(const std::string& key, const std::string& value) {
    if (m_db_txn == nullptr) {
        return false;
    }
    return m_db_txn->write(key, value);
}

bool xstore::xstore_transaction_t::do_set(const xstore_key_t &key, base::xdataobj_t *object) {
    if (m_db_txn == nullptr) {
        return false;
    }

    if (object == nullptr) {
        return false;
    }

    base::xstream_t stream(base::xcontext_t::instance());
    auto ret = object->full_serialize_to(stream);
    assert(0 != ret);

    std::string value((const char *)stream.data(), stream.size());
    return m_db_txn->write(key.to_db_key(), value);
}

bool xstore::xstore_transaction_t::do_set(const std::map<std::string, std::string>& write_pairs) {
    if (m_db_txn == nullptr) {
        return false;
    }

    bool saved = true;
    for(const auto& entry : write_pairs) {
        saved = m_db_txn->write(entry.first, entry.second);
        if (!saved) {
            return false;
        }
    }
    return true;
}

bool xstore::xstore_transaction_t::do_delete(const std::string& key) {
    if (m_db_txn == nullptr) {
        return false;
    }
    return m_db_txn->erase(key);
}

bool xstore::xstore_transaction_t::commit() {
    return m_db_txn->commit();
}

bool xstore::xstore_transaction_t::rollback() {
    return m_db_txn->rollback();
}

std::string xstore_key_t::to_db_key() const {
    char buf[KEY_BUFFER_SIZE];
    memset(buf, 0, sizeof(buf));

    char *begin = buf;

    memcpy(begin, &type, sizeof(type));
    begin += sizeof(type);

    memcpy(begin, ":", 1);
    begin += 1;

    memcpy(begin, &block_component, sizeof(block_component));
    begin += sizeof(block_component);

    memcpy(begin, ":", 1);
    begin += 1;

    memcpy(begin, owner.c_str(), owner.size());
    begin += owner.size();

    memcpy(begin, ":", 1);
    begin += 1;

    memcpy(begin, subowner.c_str(), subowner.size());
    begin += subowner.size();

    memcpy(begin, ":", 1);
    begin += 1;

    memcpy(begin, id.c_str(), id.size());
    begin += id.size();

    return std::string(buf, begin - buf);
}

bool xstore_key_t::is_numeric(const std::string &str) const {
    for (size_t i = 0; i < str.length(); i++)
        if (isdigit(str[i]) == false)
            return false;
    return true;
}

std::string xstore_key_t::printable_key() const {
    char buf[KEY_BUFFER_SIZE];
    memset(buf, 0, sizeof(buf));

    int n;
    if (!id.empty() && is_numeric(id)) {
        n = snprintf(buf, sizeof(buf), "%d:%d:%s:%s:%s",
                     type, block_component, owner.c_str(), subowner.c_str(), id.c_str());
    } else {
        n = snprintf(buf, sizeof(buf), "%d:%d:%s:%s:%s",
                     type, block_component,
                     owner.c_str(), subowner.c_str(),
                     id.empty() ? "" : data::to_hex_str(id).c_str());
    }

    return std::string(buf, n);
}

xstore::xstore(const std::shared_ptr<db::xdb_face_t> &db)
    : m_db(db) {}

xaccount_ptr_t xstore::query_account(const std::string &address) const {
    auto blockchain = clone_account(address);
    xblockchain_ptr_t obj;
    obj.attach(blockchain);
    return obj;
}

xblockchain2_t *xstore::clone_account(const std::string &owner) const {
    // auto obj = get_object(xstore_key_t(xstore_key_type_blockchain, xstore_block_type_none, owner, {}));
    // return dynamic_cast<xblockchain2_t *>(obj);

    auto dbkey = xstore_key_t(xstore_key_type_blockchain, xstore_block_type_none, owner, {});
    std::string value = get_value(dbkey);
    if (!value.empty()) {
        xblockchain2_t* account = new xblockchain2_t(owner);
        account->serialize_from_string(value);
        return account;
    }
    return nullptr;
}
std::map<std::string, std::string> xstore::generate_block_object(xblock_t *block) {
    xassert(block != nullptr);

    std::map<std::string, std::string> block_pairs;

    xstore_key_t header_key(xstore_key_type_block, xstore_block_type_header, block->get_account(), {}, std::to_string(block->get_height()));
    auto block_header_pair = generate_db_object(header_key, block);
    // set block (header/cert)
    block_pairs.emplace(block_header_pair);

    if (block->get_block_class() != base::enum_xvblock_class_nil) {
        if (!block->get_input()->get_resources_hash().empty()) {
            auto input = block->get_input()->get_resources_data();
            if (!input.empty()) {
                xstore_key_t input_key(xstore_key_type_block, xstore_block_type_input, block->get_account(), {}, std::to_string(block->get_height()));
                block_pairs.insert(std::make_pair(input_key.to_db_key(), input));
            } else {
                xerror("xstore::generate_block_object, block:%s has no input", block->dump().c_str());
                return {};
            }
        }

        if (!block->get_output()->get_resources_hash().empty()) {
            auto output = block->get_output()->get_resources_data();
            if (!output.empty()) {
                xstore_key_t output_key(xstore_key_type_block, xstore_block_type_output, block->get_account(), {}, std::to_string(block->get_height()));
                block_pairs.insert(std::make_pair(output_key.to_db_key(), output));
            } else {
                xerror("xstore::generate_block_object, block:%s has no output", block->dump().c_str());
                return {};
            }
        }
    }

    return std::move(block_pairs);
}

uint64_t xstore::get_blockchain_height(const std::string &owner) {
    base::xauto_ptr<xblockchain2_t> blockchain = clone_account(owner);
    if (blockchain != nullptr) {
        return blockchain->get_chain_height();
    }
    return 0;
}

data::xblock_t *xstore::get_block_by_height(const std::string &owner, uint64_t height) const {
    data::xblock_t *  block = nullptr;
    base::xvblock_t * vblock = get_vblock_header(owner, owner, height);
    block = dynamic_cast<data::xblock_t *>(vblock);
    if (block == nullptr) {
        return nullptr;
    }

    bool ret = get_vblock_input(owner, vblock);
    if (!ret) {
        xassert(0);
        block->release_ref();
        return nullptr;
    }

    ret = get_vblock_output(owner, vblock);
    if (!ret) {
        xassert(0);
        block->release_ref();
        return nullptr;
    }

    return block;
}

bool xstore::save_block(const std::string & store_path, data::xblock_t *block) {
    bool save_successfully = true;
    bool committed = false;

    XMETRICS_TIME_RECORD_KEY_WITH_TIMEOUT("store_block_set_time", block->get_account() + ":" + std::to_string(block->get_height()), uint32_t(2000000));
    std::map<std::string, std::string> write_pairs = generate_block_object(block);
    if (write_pairs.empty()) {
        xassert(0);
        return false;
    }

    size_t trying_count = 5;
    size_t i = 0;
    for (; i < trying_count; ++i) {
        xstore_transaction_t txn(m_db);
        save_successfully = txn.do_set(write_pairs);
        if (!save_successfully) {
            xwarn("xstore::save_block failed writing block:%s in %zu time", block->dump().c_str(), i+1);
            XMETRICS_COUNTER_INCREMENT("store_block_retry", 1);
            txn.rollback();
            base::xtime_utl::sleep_ms(1);
            continue;
        }

        if (block->is_lightunit()) {
            xlightunit_block_t* lightunit = dynamic_cast<xlightunit_block_t*>(block);
            const std::vector<xlightunit_tx_info_ptr_t> & txs = lightunit->get_txs();
            for (auto & tx : txs) {
                save_successfully = set_transaction_hash(txn, block->get_height(), tx->get_tx_hash(), tx->get_tx_subtype(), tx->get_raw_tx().get());
                if (!save_successfully) {
                    xwarn("xstore::save_block set transaction fail. tx=%s", tx->get_tx_dump_key().c_str());
                    // stop trying to save other transaction in the block, trying again from starting saving account
                    XMETRICS_COUNTER_INCREMENT("store_transaction_retry", 1);
                    txn.rollback();
                    base::xtime_utl::sleep_ms(1);
                    break;
                }
                xinfo("xstore::save_block transaction succ. unit=%s,tx=%s", lightunit->dump().c_str(), tx->get_tx_dump_key().c_str());
            }
        }
        if (save_successfully) {
            committed = txn.commit();
        }

        if (committed) {
            XMETRICS_COUNTER_INCREMENT("store_block_set", 1);
            break;
        }
    }

    if (i < trying_count) {
        size_t len = 0;
        for (auto & v : write_pairs) {
            len += v.second.size();
        }
        xinfo("xstore::save_block block succ. block=%s block_size=%d, block_pairs=%d, in %zu time", block->dump().c_str(), len, write_pairs.size(), i+1);
        return true;
    }

    xerror("xstore::save_block fail for block=%s", block->dump().c_str());
    return false;
}

bool xstore::set_transaction_hash(xstore_transaction_t& txn, const uint64_t unit_height, const std::string &txhash, enum_transaction_subtype txtype, xtransaction_t* tx) {
    xstore_key_t db_key(xstore_key_type_transaction, xstore_block_type_none, {}, {}, txhash);

    xdataobj_t* obj = nullptr;
    bool succ = txn.do_get(db_key, &obj);
    if (!succ) {
        xwarn("xstore::set_transaction_hash failed reading tx:%s", data::to_hex_str(txhash).c_str());
        return false;
    }
    auto tx_store = dynamic_cast<xtransaction_store_t*>(obj);

    if (tx_store == nullptr) {
        tx_store = new xtransaction_store_t();
    }
    tx_store->set_raw_tx(tx);
    if (txtype == enum_transaction_subtype_self) {
        tx_store->set_send_unit_height(unit_height);
        tx_store->set_recv_unit_height(unit_height);
        tx_store->set_recv_ack_unit_height(unit_height);
    } else if (txtype == enum_transaction_subtype_send) {
        tx_store->set_send_unit_height(unit_height);
    } else if (txtype == enum_transaction_subtype_recv) {
        tx_store->set_recv_unit_height(unit_height);
    } else if (txtype == enum_transaction_subtype_confirm) {
        tx_store->set_recv_ack_unit_height(unit_height);
    }

    succ = txn.do_set(db_key, tx_store);
    if (!succ) {
        xwarn("xstore::set_transaction_hash failed writing tx:%s", data::to_hex_str(txhash).c_str());
        tx_store->release_ref();
        return false;
    }

    tx_store->release_ref();
    return true;
}

int32_t xstore::string_get(const std::string &address, const std::string &key, std::string &value) const {
    // TODO(jimmy) need optimize account context
    xaccount_context_t context(address, const_cast<xstore *>(this));
    if (key == data::XPROPERTY_CONTRACT_CODE) {
        return context.get_contract_code(value);
    }
    return context.string_get(key, value);
}

int32_t xstore::list_get(const std::string &address, const std::string &key, const uint32_t index, std::string &value) {
    xaccount_context_t context(address, this);
    return context.list_get(key, index, value);
}

int32_t xstore::list_size(const std::string &address, const std::string &key, int32_t &size) {
    xaccount_context_t context(address, this);
    return context.list_size(key, size);
}

int32_t xstore::list_get_all(const std::string &address, const std::string &key, std::vector<std::string> &values) {
    xaccount_context_t context(address, this);
    return context.list_get_all(key, values);
}

void xstore::list_clear(const std::string &address, const std::string &key) {
    xaccount_context_t context(address, this);
    context.list_clear(key);
}

int32_t xstore::map_get(const std::string &address, const std::string &key, const std::string &field, std::string &value) {
    xaccount_context_t context(address, this);
    return context.map_get(key, field, value);
}

int32_t xstore::map_size(const std::string &address, const std::string &key, int32_t &size) {
    xaccount_context_t context(address, this);
    return context.map_size(key, size);
}

int32_t xstore::map_copy_get(const std::string &address, const std::string &key, std::map<std::string, std::string> &map) const {
    xaccount_ptr_t account = query_account(address);
    if (nullptr == account) {
        return -1;
    }
    xaccount_context_t context(account.get(), const_cast<xstore *>(this));
    return context.map_copy_get(key, map);
}

xtransaction_store_ptr_t xstore::query_transaction_store(const uint256_t &hash) {
    xassert(!hash.empty());
    std::string hash_str = std::string((const char*)hash.data(), hash.size());
    xtransaction_store_ptr_t tx_store;
    auto                     obj = get_object(xstore_key_t(xstore_key_type_transaction, xstore_block_type_none, {}, {}, hash_str));
    tx_store.attach(dynamic_cast<xtransaction_store_t *>(obj));
    return tx_store;
}

std::string xstore::get_full_offstate(const std::string & account, uint64_t height) {
    xstore_key_t last_full_offstate_key = xstore_key_t(xstore_key_type_block_offstate, xstore_block_type_none, account, std::to_string(height));
    std::string value = get_value(last_full_offstate_key);
    return value;
}

base::xdataunit_t* xstore::get_full_block_offstate(const std::string & account, uint64_t height) const {
    xstore_key_t last_full_offstate_key = xstore_key_t(xstore_key_type_block_offstate, xstore_block_type_none, account, std::to_string(height));
    std::string value = get_value(last_full_offstate_key);
    base::xdataunit_t* offstate = nullptr;
    if (!value.empty()) {
        base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)value.c_str(), (uint32_t)value.size());
        offstate = base::xdataunit_t::read_from(stream);
    }
    return offstate;
}

bool xstore::set_full_block_offstate(const std::string & account, uint64_t height, base::xdataunit_t* offstate) {
    xstore_key_t key = xstore_key_t(xstore_key_type_block_offstate, xstore_block_type_none, account, std::to_string(height));
    std::string value;
    offstate->serialize_to_string(value);
    xassert(!value.empty());
    return set_value(key.to_db_key(), value);
}

bool xstore::execute_tableblock_light(xblockchain2_t* account, const xblock_t *block) {
    base::xvaccount_t _vaccount(block->get_account());
    uint64_t last_full_height = block->get_last_full_block_height();
    std::string old_full_data_str;
    if (last_full_height != 0) {
        const std::string offdata_key = base::xvdbkey_t::create_block_offdata_key(_vaccount, block->get_last_full_block_hash());
        old_full_data_str = get_value(offdata_key);
        if (old_full_data_str.empty()) {
            xerror("xstore::execute_tableblock_light get full data fail.block=%s", block->dump().c_str());
            return false;
        }
    }

    std::string binlog_str = account->get_extend_data(xblockchain2_t::enum_blockchain_ext_type_binlog);
    if (block->get_last_full_block_height() == block->get_height() - 1) {
        xassert(binlog_str.empty());
    }

    xassert(block->get_height() - 1 == account->get_last_height());
    xtablestate_ptr_t tablestate = make_object_ptr<xtablestate_t>(old_full_data_str, last_full_height, binlog_str, block->get_height() - 1);
    if (!tablestate->execute_block((base::xvblock_t*)block)) {
        xerror("xstore::execute_tableblock_light execute fail.block=%s", block->dump().c_str());
        return false;
    }
    xdbg("xstore::execute,%s,height=%ld,receiptid_binlog=%s", block->get_account().c_str(), block->get_height(), tablestate->get_receiptid_state()->get_binlog()->dump().c_str());
    std::string new_binlog_data_str = tablestate->serialize_to_binlog_data_string();
    account->set_extend_data(xblockchain2_t::enum_blockchain_ext_type_binlog, new_binlog_data_str);
    account->add_light_table(block);
    xdbg("xstore::execute_tableblock_light account=%s,height=%ld", account->get_account().c_str(), block->get_height());
    return true;
}

bool xstore::execute_tableblock_full(xblockchain2_t* account, xfull_tableblock_t *block, std::map<std::string, std::string> & kv_pairs) {
    if (nullptr == block->get_offdata()) {
        // get latest state
        base::xvaccount_t _vaccount(block->get_account());
        uint64_t last_full_height = block->get_last_full_block_height();
        std::string old_full_data_str;
        if (last_full_height != 0) {
            const std::string offdata_key = base::xvdbkey_t::create_block_offdata_key(_vaccount, block->get_last_full_block_hash());
            old_full_data_str = get_value(offdata_key);
            if (old_full_data_str.empty()) {
                xerror("xstore::execute_tableblock_full get full data fail.block=%s", block->dump().c_str());
                return false;
            }
        }

        std::string binlog_str = account->get_extend_data(xblockchain2_t::enum_blockchain_ext_type_binlog);
        if (binlog_str.empty()) {
            xerror("xstore::execute_tableblock_full get binlog data fail.block=%s", block->dump().c_str());
            return false;
        }

        xtablestate_ptr_t tablestate = make_object_ptr<xtablestate_t>(old_full_data_str, last_full_height, binlog_str, block->get_height() - 1);

        // apply block on latest state to generate new state
        if (!tablestate->execute_block(block)) {
            xerror("xstore::execute_tableblock_full execute fail.block=%s", block->dump().c_str());
            return false;
        }
        xdbg("xstore::execute_tableblock_full,%s,height=%ld,receiptid_full=%s", block->get_account().c_str(), block->get_height(), tablestate->get_receiptid_state()->get_last_full_state()->dump().c_str());
    }

    // store new offdata and binlog
    set_vblock_offdata({}, block);
    account->set_extend_data(xblockchain2_t::enum_blockchain_ext_type_binlog, std::string());  // clear binlo

    if (!account->add_full_table(block)) {
        xerror("xstore::execute_tableblock_full add full table fail");
        return false;
    }
    xdbg("xstore::execute_tableblock_full account=%s,height=%ld", account->get_account().c_str(), block->get_height());
    return true;
}

int32_t xstore::get_map_property(const std::string &address, uint64_t height, const std::string &name, std::map<std::string, std::string> &value) {
    xaccount_ptr_t account = get_target_state(address, height);
    if (nullptr == account) {
        return -1;
    }

    bool ret = account->map_get(name, value);
    if (!ret) {
        return -1;
    }
    return xsuccess;
}

int32_t xstore::get_string_property(const std::string &address, uint64_t height, const std::string &name, std::string &value) {
    xaccount_ptr_t account = get_target_state(address, height);
    if (nullptr == account) {
        return -1;
    }
    bool ret = account->string_get(name, value);
    if (!ret) {
        return -1;
    }
    return xsuccess;
}

bool xstore::load_blocks_from_full_or_state(const xaccount_ptr_t & state, const xblock_ptr_t & latest_block, std::map<uint64_t, xblock_ptr_t> & blocks) const {
    if (state->get_last_height() == latest_block->get_height() && state->get_last_block_hash() == latest_block->get_block_hash()) {
        return true;
    }

    base::xvaccount_t _vaddr(latest_block->get_account());
    // load latest blocks and then apply to state
    xblock_ptr_t current_block = latest_block;
    blocks[current_block->get_height()] = current_block;
    while (true) {
        if ( current_block->is_genesis_block() || current_block->is_fullblock() ) {
            break;
        }
        if (current_block->get_last_block_hash() == state->get_last_block_hash()) {
            break;
        }

        auto _block = base::xvchain_t::instance().get_xblockstore()->load_block_object(_vaddr, current_block->get_height() - 1, current_block->get_last_block_hash(), true);
        if (_block == nullptr) {
            xwarn("xstore::load_blocks_from_full_or_state fail-load block.account=%s,height=%ld", latest_block->get_account().c_str(), current_block->get_height() - 1);
            return false;
        }
        current_block = xblock_t::raw_vblock_to_object_ptr(_block.get());
        blocks[current_block->get_height()] = current_block;
    }
    xdbg("xstore::load_blocks_from_full_or_state state_height=%ld,latest_block_height=%ld,blocks_size=%zu",
        state->get_last_height(), latest_block->get_height(), blocks.size());
    return true;
}

xaccount_ptr_t xstore::get_target_state(base::xvblock_t* block) const {
    if (block == nullptr) {
        xerror("xstore::get_target_state block is null");
        return nullptr;
    }
    const std::string & address = block->get_account();
    base::xvaccount_t _vaddr(address);
    uint64_t target_height = block->get_height();
    xaccount_ptr_t current_state = query_account(address);
    if (current_state == nullptr) {
        // genesis block must has been executed
        xwarn("xstore::get_target_state query account fail. block=%s", block->dump().c_str());
        return nullptr;
    }
    xassert(!current_state->get_last_block_hash().empty());

    xdbg("xstore::get_target_state after load. state:height=%ld,hash=%s,block:height=%ld,hash=%s",
        current_state->get_last_height(), base::xstring_utl::to_hex(current_state->get_last_block_hash()).c_str(),
        block->get_height(),  base::xstring_utl::to_hex(block->get_block_hash()).c_str());
    if (current_state->get_last_height() == target_height) {
        if (current_state->get_last_block_hash() != block->get_block_hash()) {
            xassert(false);
            current_state = make_object_ptr<xblockchain2_t>(address);  // reset state to genesis
        } else {
            return current_state;
        }
    } else if (current_state->get_last_height() > target_height) {
        current_state = make_object_ptr<xblockchain2_t>(address);  // reset state to genesis
    }

    xblock_ptr_t latest_block = xblock_t::raw_vblock_to_object_ptr(block);
    std::map<uint64_t, xblock_ptr_t> blocks;
    if (false == load_blocks_from_full_or_state(current_state, latest_block, blocks)) {
        xwarn("xstore::get_target_state load block fail.block=%s", block->dump().c_str());
        return nullptr;
    }

    for (auto & v : blocks) {
        auto & _block = v.second;
        if (false == current_state->apply_block(_block.get())) {
            xerror("xstore::get_target_state fail apply block.block=%s", _block->dump().c_str());
            return nullptr;
        }
    }

    xassert(current_state->get_last_height() == target_height && current_state->get_last_block_hash() == block->get_block_hash());
    return current_state;
}

bool xstore::string_property_get(base::xvblock_t* block, const std::string& prop, std::string& value) const {
    xaccount_ptr_t state = get_target_state(block);
    if (state == nullptr) {
        xwarn("xstore::string_property_get get target state fail.block=%s,prop=%s", block->dump().c_str(), prop.c_str());
        return false;
    }
    return state->string_get(prop, value);
}

xaccount_ptr_t xstore::get_target_state(const std::string &address, uint64_t height) const {
    base::xvaccount_t _vaddr(address);
    base::xauto_ptr<base::xvblock_t> _block = base::xvchain_t::instance().get_xblockstore()->load_block_object(_vaddr, height, base::enum_xvblock_flag_committed, true);
    if (_block == nullptr) {
        xwarn("xstore::get_target_state load block fail.account=%s,height=%ld", address.c_str(), height);
        return nullptr;
    }

    xaccount_ptr_t current_state = get_target_state(_block.get());
    if (current_state == nullptr) {
        xwarn("xstore::get_target_state get target state fail.account=%s,height=%ld", address.c_str(), height);
        return nullptr;
    }
    return current_state;
}

bool xstore::set_object(const xstore_key_t &key, const std::string &value, const std::string &detail_info) {
    assert(!value.empty());
    assert(key.type < xstore_key_type_max);
    xkinfo("[STORE] %s %s", key.printable_key().c_str(), detail_info.c_str());

    bool success = true;
    XMETRICS_TIME_RECORD_KEY_WITH_TIMEOUT("store_set_time", key.printable_key(), uint32_t(2000000));
    success = m_db->write(key.to_db_key(), value.c_str(), value.size());
    XMETRICS_COUNTER_INCREMENT("store_set", 1);
    return success;
}

std::pair<std::string, std::string> xstore::generate_db_object(const xstore_key_t &key, base::xdataobj_t *object) {
    xassert(object != nullptr);
    xassert(key.type < xstore_key_type_max);

    // base::xstream_t stream(base::xcontext_t::instance());
    // auto ret = object->full_serialize_to(stream);
    // assert(0 != ret);

    // return std::make_pair(key.to_db_key(), std::string((const char *)stream.data(), stream.size()));

    std::string binstr;
    object->serialize_to_string(binstr);
    return std::make_pair(key.to_db_key(), binstr);
}

bool xstore::set_object(const xstore_key_t &key, base::xdataobj_t *value, const std::string &detail_info) {
    assert(value != nullptr);
    assert(key.type < xstore_key_type_max);
    xkinfo("[STORE] %s %s", key.printable_key().c_str(), detail_info.c_str());

    bool success = true;
    XMETRICS_TIME_RECORD_KEY_WITH_TIMEOUT("store_set_time", key.printable_key(), uint32_t(2000000));

    success = set(key.to_db_key(), value);
    XMETRICS_COUNTER_INCREMENT("store_set", 1);
    return success;
}

base::xdataobj_t *xstore::get_object(const xstore_key_t &key) const {
    base::xdataobj_t *obj = nullptr;
    obj = get(key);
    XMETRICS_COUNTER_INCREMENT("store_get", 1);
    xdbg("[STORE] %s, value: %s", key.printable_key().c_str(), obj == nullptr ? "not exist" : "exists");
    return obj;
}

std::string xstore::get_value(const xstore_key_t &key) const {
    std::string value;
    base::xdataobj_t *obj;
    std::string       db_key = key.to_db_key();
    XMETRICS_TIME_RECORD_KEY_WITH_TIMEOUT("store_get_time", key.printable_key(), uint32_t(2000000));
    m_db->read(db_key, value);
    XMETRICS_COUNTER_INCREMENT("store_get", 1);
    xdbg("[STORE] %s value: %s", key.printable_key().c_str(), value.empty() ? "does NOT exist" : "exists");
    return std::move(value);
}

base::xdataobj_t *xstore::get(const xstore_key_t &key) const {
    std::string value;
    {
        XMETRICS_TIME_RECORD_KEY_WITH_TIMEOUT("store_get_time", key.printable_key(), uint32_t(2000000));
        auto ret = m_db->read(key.to_db_key(), value); //  todo use slice return and set xstream
        if (!ret) {
            // xdbg("xstore_t::get, db not found, key(%s)", key.printable_key().c_str());
            return nullptr;
        }
    }
    XMETRICS_TIME_RECORD_KEY_WITH_TIMEOUT("store_get_serialization", key.printable_key(), uint32_t(2000000));
    base::xstream_t   stream(base::xcontext_t::instance(), (uint8_t *)value.data(), (uint32_t)value.size());
    base::xdataobj_t *data = base::xdataobj_t::read_from(stream);
    xassert(nullptr != data);
    if (nullptr == data) {
        xwarn("xstore_t::get, object unserialize fail, key(%s)", key.printable_key().c_str());
        return nullptr;
    }

    return data;
}

bool xstore::set(const std::string &key, base::xdataobj_t *object) {
    xassert(object != nullptr);
    base::xstream_t stream(base::xcontext_t::instance());
    auto            ret = object->full_serialize_to(stream);
    assert(0 != ret);

    return m_db->write(key, (const char *)stream.data(), stream.size());
}

xobject_ptr_t<xstore_face_t> xstore_factory::create_store_with_memdb() {
    std::shared_ptr<db::xdb_face_t> db = db::xdb_factory_t::create_memdb();
    auto                            store = top::make_object_ptr<xstore>(db);
    return store;
}

xobject_ptr_t<xstore_face_t> xstore_factory::create_store_with_kvdb(const std::string &db_path) {
    xinfo("store init with one db, db path %s", db_path.c_str());
    std::shared_ptr<db::xdb_face_t> db = db::xdb_factory_t::create_kvdb(db_path);
    auto                            store = top::make_object_ptr<xstore>(db);
    return store;
}

xobject_ptr_t<xstore_face_t> xstore_factory::create_store_with_static_kvdb(const std::string &db_path) {
    xinfo("static store init with one db, db path %s", db_path.c_str());
    static std::shared_ptr<db::xdb_face_t> db = db::xdb_factory_t::create_kvdb(db_path);
    auto                                   store = top::make_object_ptr<xstore>(db);
    return store;
}

xobject_ptr_t<xstore_face_t> xstore_factory::create_store_with_static_kvdb(std::shared_ptr<db::xdb_face_t>& db) {
    auto                                   store = top::make_object_ptr<xstore>(db);
    return store;
}

bool xstore::set_vblock(const std::string & store_path, base::xvblock_t* vblock) {
    if (vblock == nullptr) {
        xassert(0);
        return false;
    }
    xinfo("xstore::set_vblock begin block:%s", vblock->dump().c_str());
    auto block = dynamic_cast<data::xblock_t*>(vblock);
    auto success = save_block(store_path, block);
    if (!success) {
        return success;
    }

    return true;
}

bool xstore::set_vblock_header(const std::string & store_path, base::xvblock_t* vblock) {
    if (vblock == nullptr) {
        return false;
    }

    xinfo("xstore::set_vblock_header enter block:%s", vblock->dump().c_str());
    return set_object(xstore_key_t(xstore_key_type_block, xstore_block_type_header, vblock->get_account(), {}, std::to_string(vblock->get_height())), vblock, {});
}

base::xvblock_t *xstore::get_vblock(const std::string & store_path, const std::string &account, uint64_t height) const {
    return get_block_by_height(account, height);
}

base::xvblock_t * xstore::get_vblock_header(const std::string & store_path, const std::string &account, uint64_t height) const {
    base::xvblock_t *  block = nullptr;
    base::xdataobj_t *obj = get_object(xstore_key_t(xstore_key_type_block, xstore_block_type_header, account, {}, std::to_string(height)));
    block = dynamic_cast<base::xvblock_t *>(obj);
    if (block == nullptr && obj != nullptr) {
        obj->release_ref();
    }
    return block;
}

bool xstore::get_vblock_input(const std::string & store_path, base::xvblock_t* block) const {
    if (block == nullptr) {
        xassert(0);
        return false;
    }
    if (block->get_block_class() != base::enum_xvblock_class_nil) {
        if (!block->get_input()->get_resources_hash().empty()) {
            auto input = get_value(xstore_key_t(xstore_key_type_block, xstore_block_type_input, block->get_account(), {}, std::to_string(block->get_height())));
            xassert(!input.empty());
            if (input.empty()) {
                xerror("xstore::get_vblock_input fail load input. block=%s", block->dump().c_str());
                return false;
            }

            bool ret = block->set_input_resources(input);
            if (!ret) {
                xerror("xstore::get_vblock_input fail set input. block=%s", block->dump().c_str());
                return false;
            }
        }
    }
    return true;
}

bool xstore::get_vblock_output(const std::string &account, base::xvblock_t* block) const {
    if (block == nullptr) {
        xassert(0);
        return false;
    }
    if (block->get_block_class() != base::enum_xvblock_class_nil) {
        if (!block->get_output()->get_resources_hash().empty()) {
            auto output = get_value(xstore_key_t(xstore_key_type_block, xstore_block_type_output, block->get_account(), {}, std::to_string(block->get_height())));
            xassert(!output.empty());
            if (output.empty()) {
                xerror("xstore::get_vblock_output fail load output. block=%s", block->dump().c_str());
                return false;
            }

            bool ret = block->set_output_resources(output);
            if (!ret) {
                xerror("xstore::get_vblock_output fail set output. block=%s", block->dump().c_str());
                return false;
            }
        }
    }
    return true;
}

bool xstore::get_vblock_offdata(const std::string & store_path,base::xvblock_t* for_block) const {
    xassert(for_block->get_block_class() == base::enum_xvblock_class_full);

    xstore_key_t offdata_key = xstore_key_t(xstore_key_type_block_offstate, xstore_block_type_none, for_block->get_account(), std::to_string(for_block->get_height()));
    std::string offdata_value = get_value(offdata_key);
    if (!offdata_value.empty()) {
        base::xauto_ptr<base::xvboffdata_t> offdata = base::xvblock_t::create_offdata_object(offdata_value);
        xassert(offdata != nullptr);
        for_block->reset_block_offdata(offdata.get());  // TODO(jimmy) cache offstate on block
        return true;
    }
    return false;
}

bool xstore::set_vblock_offdata(const std::string & store_path,base::xvblock_t* for_block) {
    xassert(for_block->get_offdata() != nullptr);
    const std::string offdata_key = base::xvdbkey_t::create_block_offdata_key(base::xvaccount_t(for_block->get_account()), for_block->get_block_hash());
    std::string offdata_str;
    for_block->get_offdata()->serialize_to_string(offdata_str);
    xassert(!offdata_str.empty());
    return set_value(offdata_key, offdata_str);
}

bool xstore::get_vblock_offstate(const std::string &store_path, base::xvblock_t* block) const {
    // xassert(block != nullptr);
    // xassert(block->get_block_class() == base::enum_xvblock_class_full);
    // if (block == nullptr || block->get_block_class() != base::enum_xvblock_class_full) {
    //     return false;
    // }
    // xfull_tableblock_t* block_ptr = dynamic_cast<xfull_tableblock_t*>(block);
    // if (block_ptr->get_full_offstate() != nullptr) {
    //     return true;
    // }

    // base::xdataunit_t* offstate_unit = get_full_block_offstate(block->get_account(), block->get_height());
    // xtable_mbt_ptr_t offstate;
    // offstate.attach((xtable_mbt_t*)offstate_unit);
    // xassert(offstate != nullptr);

    // // TODO(jimmy)
    // xassert(false);
    // // if (nullptr != offstate) {
    // //     block_ptr->set_full_offstate(offstate);
    // //     return true;
    // // }

    // xwarn("xstore::get_vblock_offstate fail.account=%s,height=%ld", block->get_account().c_str(), block->get_height());
    xassert(false); // TODO(jimmy)
    return false;
}

bool xstore::delete_block_by_path(const std::string & store_path,const std::string & account, uint64_t height, bool has_input_output) {
    xdbg("xstore::delete_block_by_path path %s account:%s height:% " PRIu64 " has input/output %d", store_path.c_str(), account.c_str(), height, has_input_output);

    std::vector<std::string> delete_keys;
    xstore_key_t header_key(xstore_key_type_block, xstore_block_type_header, account, {}, std::to_string(height));
    delete_keys.push_back(header_key.to_db_key());

    if (has_input_output) {
        xstore_key_t input_key(xstore_key_type_block, xstore_block_type_input, account, {}, std::to_string(height));
        delete_keys.push_back(input_key.to_db_key());

        xstore_key_t output_key(xstore_key_type_block, xstore_block_type_output, account, {}, std::to_string(height));
        delete_keys.push_back(output_key.to_db_key());
    }
    return m_db->batch_change({ }, delete_keys);
}

bool xstore::set_value(const std::string &key, const std::string &value) {
    return m_db->write(key, value);
}

bool xstore::delete_value(const std::string &key) {
    return m_db->erase(key);
}

const std::string xstore::get_value(const std::string &key) const {
    std::string value;

    bool success = m_db->read(key, value);
    if (!success) {
        return std::string();
    }
    return value;
}

bool  xstore::find_values(const std::string & key,std::vector<std::string> & values)//support wild search
{
    xassert(false);
    return false;
}

bool  xstore::execute_block(base::xvblock_t* vblock) {
    auto block = dynamic_cast<data::xblock_t *>(vblock);
    if (block == nullptr) {
        xassert(false);
        return false;
    }

    XMETRICS_TIME_RECORD_KEY_WITH_TIMEOUT("store_block_execution_time", vblock->get_account() + ":" + std::to_string(vblock->get_height()), uint32_t(2000000));
    // if (!block->check_block_flag(base::enum_xvblock_flag_connected)) {
    //     xwarn("xstore::execute_block not connected block=%s", block->dump().c_str());
    //     return false;
    // }

    xassert(block->check_block_flag(base::enum_xvblock_flag_committed));
    xassert(block->check_block_flag(base::enum_xvblock_flag_locked));

    const std::string& address = block->get_account();
    xblockchain2_t* account = clone_account(address);
    if (account == nullptr) {
        account = new xblockchain2_t(address, block->get_block_level());
        xdbg("xstore::execute_block create new blockchain by block=%s", block->dump().c_str());
    } else if (block->get_height() == 0) {
        xwarn("xstore::execute_block already executed genesis block=%s", block->dump().c_str());
        return true;
    }
    base::xauto_ptr<xblockchain2_t> account_ptr(account);

    if (block->get_height() != 0 && block->get_height() <= account->get_last_height()) {
        xwarn("xstore::execute_block already executed block=%s", block->dump().c_str());
        return true;
    }

    std::map<std::string, std::string> kv_pairs;
    bool execute_ret = true;

    // first execute block, then update blockchain
    if (block->is_fulltable()) {
        execute_ret = execute_tableblock_full(account, dynamic_cast<xfull_tableblock_t*>(block), kv_pairs);
    } else if (block->is_lighttable()) {
        execute_ret = execute_tableblock_light(account, block);
    }
    if (!execute_ret) {
        xerror("xstore::execute_block fail-for execute table. block=%s, level:%d class:%d",
            block->dump().c_str(), block->get_block_level(), block->get_block_class());
        return false;
    }

    execute_ret = account->apply_block(block);
    if (!execute_ret) {
        xerror("xstore::execute_block fail-account apply block, block=%s",
            block->dump().c_str());
        return false;
    }

    xassert(account->get_last_height() == block->get_height() && account->get_last_block_hash() == block->get_block_hash());

    xstore_key_t blockchain_key(xstore_key_t(xstore_key_type_blockchain, xstore_block_type_none, account->get_account(), {}));
    std::string account_binstr;
    account->serialize_to_string(account_binstr);
    auto blockchain_pair = std::make_pair(blockchain_key.to_db_key(), account_binstr);
    kv_pairs.emplace(blockchain_pair);

    bool success;
    success = m_db->write(kv_pairs);
    XMETRICS_COUNTER_INCREMENT("store_block_execution", 1);
    xassert(success);

    xinfo("xstore::execute_block success,block=%s, level:%d class:%d account=%s",
        block->dump().c_str(), block->get_block_level(),
        block->get_block_class(), account->to_basic_string().c_str());
    return success;
}

} // namespace store
} // namespace top
