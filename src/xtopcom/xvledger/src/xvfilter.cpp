// Copyright (c) 2018-Present Telos Foundation & contributors
// taylor.wei@topnetwork.org
// Licensed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <string>
#include <vector>
#include "xbase/xatom.h"
#include "xvdbkey.h"
#include "xvdbstore.h"
#include "xvfilter.h"

namespace top
{
    namespace base
    {
        xvfilter_t::xvfilter_t(xvfilter_t * front_filter)
        {
            m_front_filter = nullptr;
            m_back_filter  = nullptr;
            memset(m_event_handlers, NULL, sizeof(m_event_handlers));
            
            m_front_filter = front_filter;
            if(front_filter != nullptr)
                front_filter->add_ref();
        }
    
        xvfilter_t::xvfilter_t(xvfilter_t * front_filter,xvfilter_t * back_filter)
        {
            m_front_filter = nullptr;
            m_back_filter  = nullptr;
            memset(m_event_handlers, NULL, sizeof(m_event_handlers));
            
            m_front_filter = front_filter;
            m_back_filter  = back_filter;
            
            if(front_filter != nullptr)
                front_filter->add_ref();
            
            if(back_filter != nullptr)
                back_filter->add_ref();
        }
    
        xvfilter_t::~xvfilter_t()
        {
            if(m_front_filter != nullptr)
                m_front_filter->release_ref();
            
            if(m_back_filter != nullptr)
                m_back_filter->release_ref();
            
            for(int i = 0; i < enum_max_event_ids_count; ++i)
            {
                xevent_handler * handler = m_event_handlers[i];
                if(handler != nullptr)
                    delete handler;
            }
        }
    
        bool xvfilter_t::close(bool force_async)//must call close before release
        {
            if(is_close() == false)
            {
                xobject_t::close(force_async); //mark closed first
                
                xvfilter_t*old_front_ptr = xatomic_t::xexchange(m_front_filter, (xvfilter_t*)NULL);
                if(old_front_ptr != nullptr)
                {
                    old_front_ptr->release_ref();
                }
                
                xvfilter_t*old_back_ptr = xatomic_t::xexchange(m_back_filter, (xvfilter_t*)NULL);
                if(old_back_ptr != nullptr)
                {
                    old_back_ptr->close(force_async);
                    old_back_ptr->release_ref();
                }
            }
            return true;
        }
    
        bool xvfilter_t::reset_front_filter(xvfilter_t * front_filter)
        {
            if(is_close())
                return false;
            
            if(front_filter != nullptr)
            {
                if(m_front_filter != nullptr)//not allow overwrite exiting one
                {
                    xassert(m_front_filter == nullptr);
                    return false;
                }
                front_filter->add_ref();
            }

            xvfilter_t*old_ptr = xatomic_t::xexchange(m_front_filter, front_filter);
            if(old_ptr != nullptr)
                old_ptr->release_ref();
            
            return true;
        }
    
        bool xvfilter_t::reset_back_filter(xvfilter_t * back_filter)
        {
            if(is_close())
                return false;
            
            if(back_filter != nullptr)
            {
                if(m_back_filter != nullptr)//not allow overwrite exiting one
                {
                    xassert(m_back_filter == nullptr);
                    return false;
                }
                back_filter->add_ref();
            }

            xvfilter_t*old_ptr = xatomic_t::xexchange(m_back_filter, back_filter);
            if(old_ptr != nullptr)
                old_ptr->release_ref();
            
            return true;
        }
    
        enum_xfilter_handle_code  xvfilter_t::push_event_back(const xvevent_t & event,xvfilter_t* last_filter)//throw event from prev front to back
        {
            if(is_close())
                return enum_xfilter_handle_code_closed;
            
            const enum_xfilter_handle_code result = fire_event(event,last_filter);
            if(result < enum_xfilter_handle_code_success)
            {
                xwarn("xvfilter_t::push_event_back,error(%d) of executed event(0x%x)",result,event.get_type());
                return result;
            }
            else if(result == enum_xfilter_handle_code_finish)
            {
                xdbg("xvfilter_t::push_event_back,finish to execute event(0x%x)",result,event.get_type());
                return result; //stop and return result
            }
            
            if(m_back_filter != nullptr)//continue run to next(back) filter
                return m_back_filter->push_event_back(event,this);
            
            return enum_xfilter_handle_code_finish;
        }
        
        enum_xfilter_handle_code  xvfilter_t::push_event_front(const xvevent_t & event,xvfilter_t* last_filter)//push event from back to front
        {
            if(is_close())
                return enum_xfilter_handle_code_closed;
            
            const enum_xfilter_handle_code result = fire_event(event,last_filter);
            if(result < enum_xfilter_handle_code_success)
            {
                xwarn("xvfilter_t::push_event_front,error(%d) of executed event(0x%x)",result,event.get_type());
                return result;
            }
            else if(result == enum_xfilter_handle_code_finish)
            {
                xdbg("xvfilter_t::push_event_front,finish to execute event(0x%x)",result,event.get_type());
                return result; //stop and return result
            }
            
            if(m_front_filter != nullptr) //continue to prev(front) filter
                return m_front_filter->push_event_front(event,this);
            
            return enum_xfilter_handle_code_finish;
        }
        
        enum_xfilter_handle_code xvfilter_t::fire_event(const xvevent_t & event,xvfilter_t* last_filter)
        {
            const uint8_t event_id = event.get_event_id();
            if(event_id >= enum_max_event_ids_count)
            {
                xerror("xvfilter_t::fire_event,bad event id for event(0x%x)",event.get_type());
                return enum_xfilter_handle_code_bad_type;
            }
            
            xevent_handler * target_handler = m_event_handlers[event_id];
            if(target_handler != nullptr)
                return (*target_handler)(event,last_filter);
            
            return enum_xfilter_handle_code_success;
        }
    
        bool   xvfilter_t::register_event(const uint16_t event_full_type,const xevent_handler & api_function)
        {
            const uint8_t event_id = xvevent_t::get_event_id(event_full_type);
            if(event_id >= enum_max_event_ids_count)
            {
                xerror("xvfilter_t::register_event,bad event id over max value,event(0x%x)",event_full_type);
                return false;
            }
            if(m_event_handlers[event_id] != nullptr)
            {
                xerror("xvfilter_t::register_event,repeat register the event(0x%x)",event_full_type);
                return false;
            }
            
            xevent_handler * handler = new xevent_handler(api_function);
            m_event_handlers[event_id] = handler;
            return true;
        }
    
        xdbevent_t::xdbevent_t(const int _event_type,const std::string & db_key,const std::string & db_value,enum_xdbkey_type db_key_type,xvdbstore_t* db_store_ptr)
            :xvevent_t(_event_type)
        {
            m_db_key        = db_key;
            m_db_value      = db_value;
            m_db_key_type   = db_key_type;
            m_db_store      = db_store_ptr;
        }
    
        xdbevent_t::~xdbevent_t()
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
            if(event.get_event_category() != enum_xdbevent_category)
            {
                xerror("xdbfilter_t::fire_event,bad event category for event(0x%x)",event.get_type());
                return enum_xfilter_handle_code_bad_type;
            }
            const uint8_t event_id = event.get_event_id();
            if(event_id >= enum_max_event_ids_count)
            {
                xerror("xdbfilter_t::fire_event,bad event id for event(0x%x)",event.get_type());
                return enum_xfilter_handle_code_bad_type;
            }
            
            xevent_handler * target_handler = get_event_handlers()[event_id];
            if(target_handler != nullptr)
                return (*target_handler)(event,last_filter);
            
            return enum_xfilter_handle_code_success;
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
            return enum_xfilter_handle_code_success;
        }
 
        xbksfilter_t::xbksfilter_t(xdbfilter_t * front_filter)
            :xdbfilter_t(front_filter)
        {
            INIT_EVENTS_HANDLER();
        }
        
        xbksfilter_t::xbksfilter_t(xdbfilter_t * front_filter,xdbfilter_t * back_filter)
            :xdbfilter_t(front_filter,back_filter)
        {
            INIT_EVENTS_HANDLER();
        }
        
        xbksfilter_t::~xbksfilter_t()
        {
        }
    
        enum_xfilter_handle_code xbksfilter_t::on_keyvalue_transfer(const xvevent_t & event,xvfilter_t* last_filter)
        {
            xdbevent_t* db_event_ptr = (xdbevent_t*)&event;
            //DO related logic for keyvalue_transfer
            
            //then check whether need do more deeper handle
            if(db_event_ptr->get_db_key_type() == enum_xdbkey_type_keyvalue)
            {
                const enum_xdbkey_type db_key_type = xvdbkey_t::get_dbkey_type(db_event_ptr->get_db_key());
                if(db_key_type == enum_xdbkey_type_block_index)
                    on_block_index_transfer(event,last_filter);
                else if(db_key_type == enum_xdbkey_type_block_object)
                    on_block_object_transfer(event,last_filter);
            }
            return enum_xfilter_handle_code_success;
        }
    
        enum_xfilter_handle_code xbksfilter_t::on_block_index_transfer(const xvevent_t & event,xvfilter_t* last_filter)
        {
            xdbevent_t* db_event_ptr = (xdbevent_t*)&event;
            //DO related logic for on_tx_transfer
            
            //then check whether need do default handle for on_keyvalue_transfer
            if(db_event_ptr->get_db_key_type() == enum_xdbkey_type_block_index)
            {
                on_keyvalue_transfer(event,last_filter);
            }
            return enum_xfilter_handle_code_success;
        }
 
        enum_xfilter_handle_code xbksfilter_t::on_block_object_transfer(const xvevent_t & event,xvfilter_t* last_filter)
        {
            xdbevent_t* db_event_ptr = (xdbevent_t*)&event;
            //DO related logic for on_tx_transfer
            
            //then check whether need do default handle for on_keyvalue_transfer
            if(db_event_ptr->get_db_key_type() == enum_xdbkey_type_block_object)
            {
                on_keyvalue_transfer(event,last_filter);
            }
            return enum_xfilter_handle_code_success;
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
            //DO related logic for keyvalue_transfer
            
            //then check whether need do more deeper handle
            if(db_event_ptr->get_db_key_type() == enum_xdbkey_type_keyvalue)
            {
                const enum_xdbkey_type db_key_type = xvdbkey_t::get_dbkey_type(db_event_ptr->get_db_key());
                if(db_key_type == enum_xdbkey_type_transaction)
                    on_tx_transfer(event,last_filter);
            }
            return enum_xfilter_handle_code_success;
        }
        
        enum_xfilter_handle_code xtxsfilter_t::on_tx_transfer(const xvevent_t & event,xvfilter_t* last_filter)
        {
            xdbevent_t* db_event_ptr = (xdbevent_t*)&event;
            //DO related logic for on_tx_transfer
            
            //then check whether need do default handle for on_keyvalue_transfer
            if(db_event_ptr->get_db_key_type() == enum_xdbkey_type_transaction)
            {
                on_keyvalue_transfer(event,last_filter);
            }
            return enum_xfilter_handle_code_success;
        }
 
    }//end of namespace of base
}//end of namespace top
