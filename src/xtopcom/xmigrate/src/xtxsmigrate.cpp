// Copyright (c) 2018-Present Telos Foundation & contributors
// taylor.wei@topnetwork.org
// Licensed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xtxsmigrate.h"

namespace top
{
    namespace base
    {
        xtxsmigrate_t::xtxsmigrate_t()
        {
        }
    
        xtxsmigrate_t:: ~xtxsmigrate_t()
        {
        }
        
        bool  xtxsmigrate_t::is_valid(const uint32_t obj_ver) //check version
        {
            return xtxsfilter_t::is_valid(obj_ver);
        }
    
        int   xtxsmigrate_t::init(const xvconfig_t & config_obj)
        {
            return enum_xcode_successful;
        }
        
        //caller respond to cast (void*) to related  interface ptr
        void*  xtxsmigrate_t::query_interface(const int32_t _enum_xobject_type_)
        {
            if(_enum_xobject_type_ == enum_sys_object_type_filter)
                return this;
            
            return xtxsfilter_t::query_interface(_enum_xobject_type_);
        }
    
        enum_xfilter_handle_code xtxsmigrate_t::transfer_keyvalue(xdbevent_t & event,xvfilter_t* last_filter)
        {
            if(get_object_version() > 0)
            {
                //XTODO add code for specific version
                if(event.get_target_store() != nullptr)
                    event.get_target_store()->set_value(event.get_db_key(), event.get_db_value());
            }

            return enum_xfilter_handle_code_success;
        }
    
        enum_xfilter_handle_code xtxsmigrate_t::transfer_tx(xdbevent_t & event,xvfilter_t* last_filter)
        {
            return enum_xfilter_handle_code_success;
        }
            
    }//end of namespace of base
}//end of namespace top
