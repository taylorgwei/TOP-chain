// Copyright (c) 2018-Present Telos Foundation & contributors
// taylor.wei@topnetwork.org
// Licensed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <string>
#include <vector>
#include "xbase/xatom.h"
#include "xvdbstore.h"
#include "xvdbfilter.h"

namespace top
{
    namespace base
    {
        //*************************************xdbevent_t****************************************//
        xdbevent_t::xdbevent_t(xvdbstore_t* src_db_ptr,xvdbstore_t* dst_db_ptr,enum_xdbevent_code code)
            :xvevent_t(enum_xevent_category_db | code | enum_xdbkey_type_keyvalue)
        {
            m_db_key_type   = enum_xdbkey_type_keyvalue;
            m_src_store_ptr = src_db_ptr;
            m_dst_store_ptr = dst_db_ptr;
        }
    
        xdbevent_t::xdbevent_t(const std::string & db_key,const std::string & db_value,enum_xdbkey_type db_key_type,xvdbstore_t* src_db_ptr,xvdbstore_t* dst_db_ptr,enum_xdbevent_code code)
            :xvevent_t(enum_xevent_category_db | code | db_key_type)
        {
            m_db_key        = db_key;
            m_db_value      = db_value;
            m_db_key_type   = db_key_type;
            
            m_src_store_ptr = src_db_ptr;
            m_dst_store_ptr = dst_db_ptr;
        }
    
        xdbevent_t::~xdbevent_t()
        {
        }

        //*************************************xdbfilter_t****************************************//
        xdbfilter_t::xdbfilter_t()
        {
        }
    
        xdbfilter_t::xdbfilter_t(xdbfilter_t * front_filter)
            :xvfilter_t(front_filter)
        {
        }
        
        xdbfilter_t::xdbfilter_t(xdbfilter_t * front_filter,xdbfilter_t * back_filter)
            :xvfilter_t(front_filter,back_filter)
        {
        }
        
        xdbfilter_t::~xdbfilter_t()
        {
        }
    
        enum_xfilter_handle_code xdbfilter_t::fire_event(const xvevent_t & event,xvfilter_t* last_filter)
        {
            if(event.get_event_category() != enum_xevent_category_db)
            {
                xerror("xdbfilter_t::fire_event,bad event category for event(0x%x)",event.get_type());
                return enum_xfilter_handle_code_bad_type;
            }

            const uint8_t event_key = event.get_event_key();
            if(event_key >= enum_max_event_keys_count)
            {
                xerror("xdbfilter_t::fire_event,bad event id for event(0x%x)",event.get_type());
                return enum_xfilter_handle_code_bad_type;
            }
            
            xevent_handler * target_handler = get_event_handlers()[event_key];
            if(target_handler != nullptr)
                return (*target_handler)(event,last_filter);
            
            return enum_xfilter_handle_code_success;
        }
        
        //*************************************xkeyvfilter_t****************************************//
        xkeyvfilter_t::xkeyvfilter_t()
        {
            INIT_EVENTS_HANDLER();
        }
    
        xkeyvfilter_t::xkeyvfilter_t(xdbfilter_t * front_filter)
            :xdbfilter_t(front_filter)
        {
            INIT_EVENTS_HANDLER();
        }
    
        xkeyvfilter_t::xkeyvfilter_t(xdbfilter_t * front_filter,xdbfilter_t * back_filter)
            :xdbfilter_t(front_filter,back_filter)
        {
            INIT_EVENTS_HANDLER();
        }
    
        xkeyvfilter_t::~xkeyvfilter_t()
        {
        }
    
        enum_xfilter_handle_code xkeyvfilter_t::on_keyvalue_transfer(const xvevent_t & event,xvfilter_t* last_filter)
        {
            xdbevent_t* db_event_ptr = (xdbevent_t*)&event;
            return transfer_keyvalue(*db_event_ptr,last_filter);//transfer key first if need
        }
    
        enum_xfilter_handle_code xkeyvfilter_t::transfer_keyvalue(xdbevent_t & event,xvfilter_t* last_filter)
        {
            return enum_xfilter_handle_code_success;
        }
 
        //*************************************xbksfilter_t****************************************//
        xblkfilter_t::xblkfilter_t()
        {
            INIT_EVENTS_HANDLER();
        }
    
        xblkfilter_t::xblkfilter_t(xdbfilter_t * front_filter)
            :xdbfilter_t(front_filter)
        {
            INIT_EVENTS_HANDLER();
        }
        
        xblkfilter_t::xblkfilter_t(xdbfilter_t * front_filter,xdbfilter_t * back_filter)
            :xdbfilter_t(front_filter,back_filter)
        {
            INIT_EVENTS_HANDLER();
        }
        
        xblkfilter_t::~xblkfilter_t()
        {
        }
    
        enum_xfilter_handle_code xblkfilter_t::on_keyvalue_transfer(const xvevent_t & event,xvfilter_t* last_filter)
        {
            xdbevent_t* db_event_ptr = (xdbevent_t*)&event;
            //transfer key first
            enum_xfilter_handle_code result = transfer_keyvalue(*db_event_ptr,last_filter);
            
            //then check whether need do more deeper handle
            if(db_event_ptr->get_db_key_type() == enum_xdbkey_type_block_index)
                transfer_block_index(*db_event_ptr,last_filter);
            else if(db_event_ptr->get_db_key_type() == enum_xdbkey_type_block_object)
                transfer_block_object(*db_event_ptr,last_filter);

            return result;
        }
    
        enum_xfilter_handle_code xblkfilter_t::transfer_keyvalue(xdbevent_t & event,xvfilter_t* last_filter)
        {
            return enum_xfilter_handle_code_success;
        }
    
        enum_xfilter_handle_code xblkfilter_t::on_block_index_transfer(const xvevent_t & event,xvfilter_t* last_filter)
        {
            xdbevent_t* db_event_ptr = (xdbevent_t*)&event;
 
            enum_xfilter_handle_code result = transfer_block_index(*db_event_ptr,last_filter);
            if(result != enum_xfilter_handle_code_ignore)
                return result;
            else //fail back to default handle
                return transfer_keyvalue(*db_event_ptr,last_filter);
        }
    
        enum_xfilter_handle_code xblkfilter_t::transfer_block_index(xdbevent_t & event,xvfilter_t* last_filter)
        {
            return enum_xfilter_handle_code_ignore;
        }
 
        enum_xfilter_handle_code xblkfilter_t::on_block_object_transfer(const xvevent_t & event,xvfilter_t* last_filter)
        {
            xdbevent_t* db_event_ptr = (xdbevent_t*)&event;
           
            enum_xfilter_handle_code result = transfer_block_object(*db_event_ptr,last_filter);//then transfer block object
            if(result != enum_xfilter_handle_code_ignore)
                return result;
            else //fail back to default handle
                return transfer_keyvalue(*db_event_ptr,last_filter);
        }
    
        enum_xfilter_handle_code   xblkfilter_t::transfer_block_object(xdbevent_t & event,xvfilter_t* last_filter)
        {
            return enum_xfilter_handle_code_ignore;
        }
    
    
        //*************************************xtxsfilter_t****************************************//
        xtxsfilter_t::xtxsfilter_t()
        {
            INIT_EVENTS_HANDLER();
        }
    
        xtxsfilter_t::xtxsfilter_t(xdbfilter_t * front_filter)
            :xdbfilter_t(front_filter)
        {
            INIT_EVENTS_HANDLER();
        }
        
        xtxsfilter_t::xtxsfilter_t(xdbfilter_t * front_filter,xdbfilter_t * back_filter)
            :xdbfilter_t(front_filter,back_filter)
        {
            INIT_EVENTS_HANDLER();
        }
        
        xtxsfilter_t::~xtxsfilter_t()
        {
        }
    
        enum_xfilter_handle_code xtxsfilter_t::on_keyvalue_transfer(const xvevent_t & event,xvfilter_t* last_filter)
        {
            xdbevent_t* db_event_ptr = (xdbevent_t*)&event;
            //transfer key first
            enum_xfilter_handle_code result = transfer_keyvalue(*db_event_ptr,last_filter);
            
            //then check whether need do more deeper handle
            if(db_event_ptr->get_db_key_type() == enum_xdbkey_type_transaction)
            {
                transfer_tx(*db_event_ptr,last_filter);
            }
            return result;
        }
    
        enum_xfilter_handle_code  xtxsfilter_t::transfer_keyvalue(xdbevent_t & event,xvfilter_t* last_filter)
        {
            return enum_xfilter_handle_code_success;
        }
        
        enum_xfilter_handle_code xtxsfilter_t::on_tx_transfer(const xvevent_t & event,xvfilter_t* last_filter)
        {
            xdbevent_t* db_event_ptr = (xdbevent_t*)&event;
           
            enum_xfilter_handle_code result = transfer_tx(*db_event_ptr,last_filter);
            if(result != enum_xfilter_handle_code_ignore)
                return result;
            else //fail back default handle
                return transfer_keyvalue(*db_event_ptr,last_filter);
        }
    
        enum_xfilter_handle_code   xtxsfilter_t::transfer_tx(xdbevent_t & event,xvfilter_t* last_filter)
        {
            return enum_xfilter_handle_code_ignore;
        }
 
    }//end of namespace of base
}//end of namespace top
