// Copyright (c) 2018-Present Telos Foundation & contributors
// taylor.wei@topnetwork.org
// Licensed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xdbmigrate.h"

namespace top
{
    namespace base
    {
        xdbtransfer_event::xdbtransfer_event(xvdbstore_t* src_db_ptr,xvdbstore_t* dst_db_ptr,enum_xdbevent_code code)
            :xdbevent_t(src_db_ptr,code)
        {
            m_target_store = dst_db_ptr;
        }
    
        xdbtransfer_event::xdbtransfer_event(const std::string & db_key,const std::string & db_value,enum_xdbkey_type db_key_type,xvdbstore_t* src_db_ptr,xvdbstore_t* dst_db_ptr,enum_xdbevent_code code)
            :xdbevent_t(db_key,db_value,db_key_type,src_db_ptr,code)
        {
            m_target_store = dst_db_ptr;
        }
    
        xdbtransfer_event::~xdbtransfer_event()
        {
        }
    
    }//end of namespace of base
}//end of namespace top
