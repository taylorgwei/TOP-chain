// Copyright (c) 2018-2020 Telos Foundation & contributors
// taylor.wei@topnetwork.org
// Licensed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <cinttypes>
#include "../xvstatestore.h"
#include "../xvledger.h"
#include "../xvdbkey.h"

namespace top
{
    namespace base
    {
        //----------------------------------------xvblkstate_api-------------------------------------//
        bool   xvblkstatestore_t::write_state_to_db(xvbstate_t & target_state,const std::string & target_block_hash)
        {
            if(target_block_hash.empty())
            {
                xerror("xvblkstatestore_t::write_state_to_db,nil block hash for state(%s)",target_state.dump().c_str());
                return false;
            }
            xvaccount_t target_account(target_state.get_address());
            const std::string state_db_key = xvdbkey_t::create_block_state_key(target_account,target_block_hash);
            
            std::string state_db_bin;
            if(target_state.serialize_to_string(state_db_bin))
            {
                if(xvchain_t::instance().get_xdbstore()->set_value(state_db_key,state_db_bin))
                    return true;
                
                xerror("xvblkstatestore_t::write_state_to_db,fail to write into db for path(%s)",state_db_key.c_str());
                return false;
            }
            xerror("xvblkstatestore_t::write_state_to_db,fail to serialize for state of block(%s)",target_state.dump().c_str());
            return false;
        }

        xvbstate_t*     xvblkstatestore_t::read_state_from_db(const xvbindex_t * for_index)
        {
            if(NULL == for_index)
                return NULL;

            return read_state_from_db(*for_index,for_index->get_block_hash());
        }
        xvbstate_t*     xvblkstatestore_t::read_state_from_db(const xvblock_t * for_block)
        {
            if(NULL == for_block)
                return NULL;
            
            xvaccount_t target_account(for_block->get_account());
            return read_state_from_db(target_account,for_block->get_block_hash());
        }
        xvbstate_t*     xvblkstatestore_t::read_state_from_db(const xvaccount_t & target_account, const std::string & block_hash)
        {
            const std::string state_db_key = xvdbkey_t::create_block_state_key(target_account,block_hash);
            const std::string state_db_bin = xvchain_t::instance().get_xdbstore()->get_value(state_db_key);
            if(state_db_bin.empty())
            {
                xwarn("xvblkstatestore_t::read_state_from_db,fail to read from db for path(%s)",state_db_key.c_str());
                return NULL;
            }
            
            xvbstate_t* state_ptr = xvblock_t::create_state_object(state_db_bin);
            if(NULL == state_ptr)//remove the error data for invalid data
            {
                xvchain_t::instance().get_xdbstore()->delete_value(state_db_key);
                xerror("xvblkstatestore_t::read_state_from_db,invalid data at db for path(%s)",state_db_key.c_str());
                return NULL;
            }
            if(state_ptr->get_address() != target_account.get_address())
            {
                xerror("xvblkstatestore_t::read_state_from_db,bad state(%s) vs ask(account:%s) ",state_ptr->dump().c_str(),target_account.get_address().c_str());
                state_ptr->release_ref();
                return NULL;
            }
            return state_ptr;
        }

        bool   xvblkstatestore_t::delete_state_of_db(const xvbindex_t & target_index)
        {
            const std::string state_db_key = xvdbkey_t::create_block_state_key(target_index,target_index.get_block_hash());
            return xvchain_t::instance().get_xdbstore()->delete_value(state_db_key);
        }
        bool   xvblkstatestore_t::delete_state_of_db(const xvblock_t & target_block)
        {
            xvaccount_t target_account(target_block.get_account());
            const std::string state_db_key = xvdbkey_t::create_block_state_key(target_account,target_block.get_block_hash());
            return xvchain_t::instance().get_xdbstore()->delete_value(state_db_key);
        }
        bool   xvblkstatestore_t::delete_state_of_db(const xvaccount_t & target_account,const std::string & block_hash)
        {
            const std::string state_db_key = xvdbkey_t::create_block_state_key(target_account,block_hash);
            return xvchain_t::instance().get_xdbstore()->delete_value(state_db_key);
        }
        bool   xvblkstatestore_t::delete_states_of_db(const xvaccount_t & target_account,const uint64_t block_height)
        {
            //delete all stated'object under target height
            xvbindex_vector auto_vector( xvchain_t::instance().get_xblockstore()->load_block_index(target_account,block_height));
            for(auto index : auto_vector.get_vector())
            {
                if(index != NULL)
                {
                    delete_state_of_db(target_account,index->get_block_hash());
                }
            }
            return true;
        }

        xvbstate_t*   xvblkstatestore_t::rebuild_state_for_full_block(const xvbindex_t & target_index)
        {
            base::xauto_ptr<base::xvblock_t> target_block(
            xvchain_t::instance().get_xblockstore()->load_block_object(target_index, target_index.get_height(),target_index.get_block_hash(),false));
            if(!target_block)
            {
                xerror("xvblkstatestore_t::rebuild_state_for_full_block,fail to load raw block from index(%s)",target_index.dump().c_str());
                return nullptr;
            }
            return rebuild_state_for_full_block(*target_block);
        }
        xvbstate_t*   xvblkstatestore_t::rebuild_state_for_full_block(xvblock_t & target_block)
        {
            xvbstate_t* init_state = new xvbstate_t(target_block);//start from ground with empty state
            if(rebuild_state_for_block(target_block,*init_state))
                return init_state;
            
            init_state->release_ref();
            return nullptr;
        }
        bool   xvblkstatestore_t::rebuild_state_for_block(xvblock_t & target_block,xvbstate_t & base_state)
        {
            if(   (base_state.get_block_height() != target_block.get_height())
               || (base_state.get_address() != target_block.get_account()) )
            {
                xerror("xvblkstatestore_t::rebuild_state_for_block,state is not match as block(%s)",target_block.dump().c_str());
                return false;
            }
            if(target_block.get_block_class() == enum_xvblock_class_nil)
            {
                xinfo("xvblkstatestore_t::rebuild_state_for_block,nothing chanage for state of nil block(%s)",target_block.dump().c_str());
                return true;
            }
            else //build new state from instruction code based on base_state
            {
                if(NULL == target_block.get_output())
                {
                    xvaccount_t target_account(target_block.get_account());
                    if(false == xvchain_t::instance().get_xblockstore()->load_block_output(target_account, &target_block))
                    {
                        xerror("xvblkstatestore_t::rebuild_state_for_block,fail to load output for block(%s)",target_block.dump().c_str());
                        return false;
                    }
                }

                const std::string binlog(target_block.get_output()->get_binlog());
                if(binlog.empty() == false)
                {
                    if(false == base_state.apply_changes_of_binlog(binlog))
                    {
                        xerror("xvblkstatestore_t::rebuild_state_for_block,invalid binlog and abort it for block(%s)",target_block.dump().c_str());
                        return false;
                    }
                }
                xinfo("xvblkstatestore_t::get_block_state,successful rebuilt state for block(%s)",target_block.dump().c_str());
                return true;
            }
        }

        xauto_ptr<xvbstate_t>   xvblkstatestore_t::load_block_state(const xvbindex_t * current_index)
        {
            if(NULL == current_index)
            {
                xassert(current_index != NULL);
                return nullptr;
            }
            if(current_index->check_block_flag(base::enum_xvblock_flag_authenticated) == false)
            {
                xerror("xvblkstatestore_t::load_block_state,failed for un-authenticated block(%s)",current_index->dump().c_str());
                return nullptr;
            }

            //step#1:try load from db for state
            {
                xvbstate_t* db_state_ptr = read_state_from_db(current_index);
                if(db_state_ptr)//read_state_from_db has added addtional reference
                {
                    if(   (current_index->get_height()  == db_state_ptr->get_block_height())
                       && (current_index->get_viewid()  == db_state_ptr->get_block_viewid())
                       && (current_index->get_account() == db_state_ptr->get_address()) )
                    {
                        xdbg("xvblkstatestore_t::load_block_state,found state(%s) for block(%s)",db_state_ptr->dump().c_str(),current_index->dump().c_str());
                        return db_state_ptr;//transfer ownership to xauto_ptr
                    }
                    xerror("xvblkstatestore_t::load_block_state,load bad state(%s) vs block(%s)",db_state_ptr->dump().c_str(),current_index->dump().c_str());
                    delete_state_of_db(*current_index);//exception handle
                    db_state_ptr->release_ref(); //release reference since it added by read_state_from_db
                    //continue to load by other ways
                }
            }
            //step#2:try rebuild state completely for full-block or genesis block
            {
                if(current_index->get_height() == 0) //direct rebuild state from geneis block
                {
                    xvbstate_t* new_state = rebuild_state_for_full_block(*current_index);
                    if(new_state != NULL)//rebuild it completely
                    {
                        write_state_to_db(*new_state,current_index->get_block_hash());//persist full state into db
                        return new_state; //transfer ownership to xauto_ptr
                    }
                    xerror("xvblkstatestore_t::load_block_state,fail to rebuild state for genesis-block(%s)",current_index->dump().c_str());
                    return nullptr;
                }
                else if(current_index->get_block_class() == enum_xvblock_class_full)//direct rebuild state for full-current_index
                {
                    xvbstate_t* new_state = rebuild_state_for_full_block(*current_index);
                    if(new_state != NULL)//rebuild it completely
                    {
                        if(write_state_to_db(*new_state,current_index->get_block_hash()))
                        {
                            if(current_index->get_height() >= 4) //just keep max 4 persisted state
                                delete_states_of_db(*current_index,current_index->get_height() - 4);
                        }
                        return new_state; //transfer ownership to xauto_ptr
                    }
                    xerror("xvblkstatestore_t::load_block_state,fail to rebuild state for full-block(%s)",current_index->dump().c_str());
                    return nullptr;
                }
            }
            //step#3: load prev block'state and apply the bin-log
            xauto_ptr<xvbstate_t> prev_block_state(get_block_state(*current_index,current_index->get_height() - 1,current_index->get_last_block_hash()));
            if(prev_block_state)//each xvbstate_t object present the full state
            {
                base::xauto_ptr<base::xvblock_t> current_block( xvchain_t::instance().get_xblockstore()->load_block_object(*current_index, current_index->get_height(),current_index->get_block_hash(),false));
                if(!current_block)
                {
                    xerror("xvblkstatestore_t::load_block_state,fail to load raw block(%s) from blockstore",current_index->dump().c_str());
                    return nullptr;
                }
                
                //create a new state based on prev-block
                xauto_ptr<xvbstate_t> new_current_state(new xvbstate_t(*current_block,*prev_block_state));
                //then re-execute instruction based on last-state
                if(rebuild_state_for_block(*current_block,*new_current_state))
                {
                    if(write_state_to_db(*new_current_state,current_index->get_block_hash()))
                    {
                        if(current_index->get_height() >= 4) //just keep max 4 persisted state
                            delete_states_of_db(*current_index,current_index->get_height() - 4);
                    }
                    new_current_state->add_ref();
                    return new_current_state();
                }
                xerror("xvstatestore::get_block_state,fail to rebuild state for normal-block(%s) as rebuild fail",current_block->dump().c_str());
                return nullptr;
            }
            xwarn("xvblkstatestore_t::load_block_state,fail to load state for block(%s) as prev-one build state fail",current_index->dump().c_str());
            return nullptr;
        }
    
        xauto_ptr<xvbstate_t>  xvblkstatestore_t::get_block_state(xvblock_t * current_block)
        {
            if(NULL == current_block)
            {
                xassert(current_block != NULL);
                return nullptr;
            }
            
            xvaccount_t target_account(current_block->get_account());
            //step#1: check cached states object per 'account and block'hash
            {
                //XTODO
            }
            
            //step#2:try load from db for state
            {
                xvbstate_t* db_state_ptr = read_state_from_db(target_account,current_block->get_block_hash());
                if(db_state_ptr)
                {
                    if(   (current_block->get_height()  == db_state_ptr->get_block_height())
                       && (current_block->get_viewid()  == db_state_ptr->get_block_viewid())
                       && (current_block->get_account() == db_state_ptr->get_address()) )
                    {
                        xdbg("xvblkstatestore_t::get_block_state,found state(%s) for block(%s)",db_state_ptr->dump().c_str(),current_block->dump().c_str());
                        return db_state_ptr;//read_state_from_db has added addtional reference
                    }
                    xerror("xvblkstatestore_t::get_block_state,load bad state(%s) vs block(%s)",db_state_ptr->dump().c_str(),current_block->dump().c_str());
                    delete_state_of_db(target_account, current_block->get_block_hash());//exception handle
                    db_state_ptr->release_ref(); //release reference since it added by read_state_from_db
                    //continue to load by other ways
                }
            }
            
            //step#3:try rebuild state completely for full-block or genesis block
            {
                if(current_block->get_height() == 0) //direct rebuild state from geneis block
                {
                    xvbstate_t* new_state = rebuild_state_for_full_block(*current_block);
                    if(new_state != NULL)//rebuild it completely
                    {
                        write_state_to_db(*new_state,current_block->get_block_hash());//persist full state into db
                        return new_state; //transfer ownership to xauto_ptr
                    }
                    xerror("xvblkstatestore_t::get_block_state,fail to rebuild state for genesis-block(%s)",current_block->dump().c_str());
                    return nullptr;
                }
                else if(current_block->get_block_class() == enum_xvblock_class_full)//direct rebuild state for full-current_index
                {
                    xvbstate_t* new_state = rebuild_state_for_full_block(*current_block);
                    if(new_state != NULL)//rebuild it completely
                    {
                        if(write_state_to_db(*new_state,current_block->get_block_hash()))
                        {
                            if(current_block->get_height() >= 4) //just keep max 4 persisted state
                                delete_states_of_db(target_account,current_block->get_height() - 4);
                        }
                        return new_state; //transfer ownership to xauto_ptr
                    }
                    xerror("xvblkstatestore_t::get_block_state,fail to rebuild state for full-block(%s)",current_block->dump().c_str());
                    return nullptr;
                }
            }
            
            //step#4: load prev block'state and apply the bin-log
            xauto_ptr<xvbstate_t> prev_block_state(get_block_state(target_account,current_block->get_height() - 1,current_block->get_last_block_hash()));
            if(prev_block_state)//each xvbstate_t object present the full state
            {
                //create a new state based on prev-block
                xauto_ptr<xvbstate_t> new_current_state(new xvbstate_t(*current_block,*prev_block_state));
                if(current_block->check_block_flag(enum_xvblock_flag_authenticated) == false) //a proposal block
                {
                   new_current_state->add_ref();
                   return new_current_state();
                }
                
                //then re-execute instruction based on last-state
                if(rebuild_state_for_block(*current_block,*new_current_state))
                {
                    if(write_state_to_db(*new_current_state,current_block->get_block_hash()))
                    {
                        if(current_block->get_height() >= 4) //just keep max 4 persisted state
                            delete_states_of_db(target_account,current_block->get_height() - 4);
                    }
                    new_current_state->add_ref();
                    return new_current_state();
                }
                xerror("xvstatestore::get_block_state,fail to rebuild state for normal-block(%s) as rebuild fail",current_block->dump().c_str());
                return nullptr;
            }
            xwarn("xvblkstatestore_t::get_block_state,fail to load state for block(%s) as prev-one build state fail",current_block->dump().c_str());
            return nullptr;
        }

        xauto_ptr<xvbstate_t> xvblkstatestore_t::get_block_state(const xvaccount_t & target_account,const uint64_t block_height,const std::string& block_hash)
        {
            //step#1: check cached states object per 'account and block'hash
            {
                //XTODO
            }
            
            base::xauto_ptr<base::xvblock_t> current_block( xvchain_t::instance().get_xblockstore()->load_block_object(target_account,block_height,block_hash,false));
            if(!current_block)
            {
                xerror("xvblkstatestore_t::get_block_state,fail to load raw block(%s->height(%" PRIu64 ")->hash(%s)) from blockstore",target_account.get_address().c_str(),block_height,block_hash.c_str());
                return nullptr;
            }
            return get_block_state(current_block());
        }
    
        xauto_ptr<xvbstate_t> xvblkstatestore_t::get_block_state(const xvaccount_t & target_account,const uint64_t block_height,const uint64_t block_view_id)
        {
            xauto_ptr<xvbindex_t> target_index( xvchain_t::instance().get_xblockstore()->load_block_index(target_account,block_height,block_view_id));
            if(!target_index)
            {
                xwarn("xvblkstatestore_t::get_block_state,fail load index for block(%s->height(%" PRIu64 ")->viewid(%" PRIu64 "))",target_account.get_address().c_str(),block_height,block_view_id);
                return nullptr;
            }
            return load_block_state(target_index());
        }

        //----------------------------------------xvstatestore_t-------------------------------------//
        xvstatestore_t::xvstatestore_t()
        :xobject_t(enum_xobject_type_vstatestore)
        {
            
        }
        
        xvstatestore_t::~xvstatestore_t()
        {
        }
        
        void*   xvstatestore_t::query_interface(const int32_t type)
        {
            if(type == enum_xobject_type_vstatestore)
                return this;
            
            return xobject_t::query_interface(type);
        }
    
        //note: when target_account = block_to_hold_state.get_account() -> return get_block_state
        //othewise return get_account_state
        xauto_ptr<xvexestate_t> xvstatestore_t::load_state(const xvaccount_t & target_account,xvblock_t * block_to_hold_state)
        {
            xassert(block_to_hold_state != NULL);
            if(NULL == block_to_hold_state)
                return nullptr;
            
            //unit block-based state-managment
            if(target_account.get_address() == block_to_hold_state->get_account())
            {
                xauto_ptr<xvbstate_t> block_state(get_block_state(block_to_hold_state));
                if(block_state)
                    block_state->add_ref(); //alloc reference for xauto_ptr<xvexestate_t>
                return  block_state();
            }

            //enter traditional state-management branch
            xerror("XTODO,xvstatestore_t::load_state() try go unsupported branch");
            return nullptr;
        }

 
    
    };//end of namespace of base
};//end of namespace of top
