#include "gtest/gtest.h"
#include "xvledger/xvaccount.h"
#include "xvledger/xvstate.h"
#include "xdata/xblocktool.h"
#include "tests/mock/xdatamock_address.hpp"

using namespace top;
using namespace top::base;
using namespace top::data;

class test_bstate : public testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};


TEST_F(test_bstate, bstate_1) {
    std::string addr = "123456789";
    xvaccount_t vaddr(addr);
    xobject_ptr_t<base::xvbstate_t> bstate = make_object_ptr<base::xvbstate_t>(addr, (uint64_t)0, (uint64_t)0, std::string(), std::string(), (uint64_t)0, (uint32_t)0, (uint16_t)0);
    xobject_ptr_t<base::xvcanvas_t> canvas = make_object_ptr<base::xvcanvas_t>();
    auto propobj = bstate->new_uint64_var("prop1", canvas.get());
    propobj->set(100, canvas.get());
    std::string property_binlog;
    canvas->encode(property_binlog);

    xobject_ptr_t<base::xvbstate_t> bstate2 = make_object_ptr<base::xvbstate_t>(addr, (uint64_t)0, (uint64_t)0, std::string(), std::string(), (uint64_t)0, (uint32_t)0, (uint16_t)0);
    bstate2->apply_changes_of_binlog(property_binlog);
    xassert(bstate->find_property("prop1"));
    auto propobj2 = bstate->load_uint64_var("prop1");
    xassert(100 == propobj2->get());
}

TEST_F(test_bstate, bstate_with_block_1) {
    std::string addr = mock::xdatamock_address::make_user_address_random();

    base::xauto_ptr<base::xvblock_t> genesis_block = xblocktool_t::create_genesis_empty_block(addr);
    xobject_ptr_t<base::xvbstate_t> state1_0 = make_object_ptr<base::xvbstate_t>(*genesis_block.get());
    std::string property_binlog;
    {
        xobject_ptr_t<base::xvcanvas_t> canvas = make_object_ptr<base::xvcanvas_t>();
        auto propobj = state1_0->new_uint64_var("prop1", canvas.get());
        propobj->set(100, canvas.get());
        canvas->encode(property_binlog);
    }

    base::xauto_ptr<base::xvblock_t> block_height1 = xblocktool_t::create_next_emptyblock(genesis_block.get());
    xobject_ptr_t<base::xvbstate_t> prev_state1_1 = make_object_ptr<base::xvbstate_t>(*genesis_block.get());
    xobject_ptr_t<base::xvbstate_t> state1_1 = prev_state1_1;
    {
        state1_1->apply_changes_of_binlog(property_binlog);
        xassert(state1_1->find_property("prop1"));
        auto propobj2 = state1_1->load_uint64_var("prop1");
        xassert(100 == propobj2->get());
    }
}


TEST_F(test_bstate, bstate_with_block_2) {
    std::string addr = mock::xdatamock_address::make_user_address_random();

    base::xauto_ptr<base::xvblock_t> genesis_block = xblocktool_t::create_genesis_empty_block(addr);
    xobject_ptr_t<base::xvbstate_t> state1_0 = make_object_ptr<base::xvbstate_t>(*genesis_block.get());
    std::string property_binlog;
    {
        xobject_ptr_t<base::xvcanvas_t> canvas = make_object_ptr<base::xvcanvas_t>();
        auto propobj = state1_0->new_uint64_var("prop1", canvas.get());
        propobj->set(100, canvas.get());
        canvas->encode(property_binlog);
    }

    base::xauto_ptr<base::xvblock_t> block_height1 = xblocktool_t::create_next_emptyblock(genesis_block.get());
    xobject_ptr_t<base::xvbstate_t> prev_state1_1 = make_object_ptr<base::xvbstate_t>(*genesis_block.get());
    xobject_ptr_t<base::xvbstate_t> state1_1 = make_object_ptr<base::xvbstate_t>(*block_height1.get(), *prev_state1_1.get());
    {
        state1_1->apply_changes_of_binlog(property_binlog);
        xassert(state1_1->find_property("prop1"));
        auto propobj2 = state1_1->load_uint64_var("prop1");
        xassert(100 == propobj2->get());
    }
}

