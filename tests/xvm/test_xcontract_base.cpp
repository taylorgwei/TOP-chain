#include <gtest/gtest.h>
#include "xvm/xcontract/xcontract_base.h"
#include "xbase/xmem.h"
#include "xbase/xcontext.h"
#include "xstore/xaccount_context.h"
#include "xstore/test/test_datamock.hpp"
#include "xbasic/xserializable_based_on.h"

using namespace top::store;
using namespace top::data;
using namespace top::xvm::xcontract;
using namespace top::base;
using namespace top::xvm;

class test_xcontract_base_sub : public xcontract_base, public testing::Test {
public:
    using xbase_t = xcontract_base;
    XDECLARE_DELETED_COPY_DEFAULTED_MOVE_SEMANTICS(test_xcontract_base_sub);
    XDECLARE_DEFAULTED_OVERRIDE_DESTRUCTOR(test_xcontract_base_sub);

    test_xcontract_base_sub() : xbase_t{ top::common::xtopchain_network_id } {
    }

    xcontract_base* clone() override {return nullptr;}
    void exec(xvm_context* vm_ctx) {}
};

struct test_xcontract_base_entity final : public xserializable_based_on<void> {
    test_xcontract_base_entity(uint32_t v) :
    value(v) {}

private:
    int32_t do_read(xstream_t& stream) override {
        uint32_t size = stream.size();
        stream >> value;
        return size - stream.size();
    }

    int32_t do_write(xstream_t& stream) const override {
        uint32_t size = stream.size();
        stream << value;
        return stream.size() - size;
    }

public:
    int32_t value{};
};

// xblock_ptr_t test_xcontract_base_create_block(test_datamock_t& dm, const std::string& address, const test_xcontract_base_entity& entity) {
//     xstream_t stream(xcontext_t::instance());
//     entity.serialize_to(stream);
//     std::string value((char*) stream.data(), stream.size());
//     auto tx = make_object_ptr<xlightunit_state_t>();
//     tx->m_native_property.native_map_set(data::XPORPERTY_CONTRACT_BLOCK_CONTENT_KEY, "_default", value);
//     auto block = dm.create_lightunit(address, make_object_ptr<xlightunit_input_t>(), tx);
//     dm.m_store->set_block(block);
//     return block;
// }
