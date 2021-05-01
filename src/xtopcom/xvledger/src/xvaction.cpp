// Copyright (c) 2018-2020 Telos Foundation & contributors
// taylor.wei@topnetwork.org
// Licensed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <cinttypes>
#include "xbase/xutl.h"
#include "../xvaction.h"
 
namespace top
{
    namespace base
    {
        xvaction_t::xvaction_t(const std::string & caller_addr,const std::string & target_uri,const uint8_t method_type,const std::string & method_name)
            :xvmethod_t(target_uri,method_type,method_name)
        {
            set_caller_uri(caller_addr);
            init();
        }
    
        xvaction_t::xvaction_t(const std::string & caller_addr,const std::string & target_uri,const uint8_t method_type,const std::string & method_name,xvalue_t & param)
            :xvmethod_t(target_uri,method_type,method_name,param)
        {
            set_caller_uri(caller_addr);
            init();
        }
    
        xvaction_t::xvaction_t(const std::string & caller_addr,const std::string & target_uri,const uint8_t method_type,const std::string & method_name,xvalue_t & param1,xvalue_t & param2)
            :xvmethod_t(target_uri,method_type,method_name,param1,param2)
        {
            set_caller_uri(caller_addr);
            init();
        }
    
        xvaction_t::xvaction_t(const std::string & caller_addr,const std::string & target_uri,const uint8_t method_type,const std::string & method_name,xvalue_t & param1,xvalue_t & param2,xvalue_t & param3)
            :xvmethod_t(target_uri,method_type,method_name,param1,param2,param3)
        {
            set_caller_uri(caller_addr);
            init();
        }
    
        xvaction_t::~xvaction_t()
        {
        }
    
        void xvaction_t::init(const int64_t init_max_tags,const int64_t init_used_tgas)
        {
            m_max_tgas  = init_max_tags;
            m_used_tgas = init_used_tgas;
            m_taget_block_height = 0;
            
            std::vector<std::string>  domains;
            if(xstring_utl::split_string(get_method_uri(),'.',domains) >= 3)
            {
                m_contract_addr = domains[0];
                m_contract_name = domains[1];
                m_taget_block_height = xstring_utl::touint64(domains[2]);
            }
        }
    
        xvaction_t::xvaction_t(const xvaction_t & obj)
            :xvmethod_t(obj)
        {
            init(obj.m_max_tgas,obj.m_used_tgas);
        }
    
        xvaction_t::xvaction_t(xvaction_t && moved)
            :xvmethod_t(moved)
        {
            init(moved.m_max_tgas,moved.m_used_tgas);
        }
    
        xvaction_t & xvaction_t::operator = (const xvaction_t & obj)
        {
            xvmethod_t::operator=(obj);
            init(obj.m_max_tgas,obj.m_used_tgas);
            return *this;
        }
    
        xvaction_t & xvaction_t::operator = (xvaction_t && moved)
        {
            xvmethod_t::operator=(moved);
            init(moved.m_max_tgas,moved.m_used_tgas);
            return *this;
        }
    
        //caller respond to cast (void*) to related  interface ptr
        void*    xvaction_t::query_minterface(const int32_t _enum_xobject_type_) const
        {
            if(_enum_xobject_type_ == enum_xobject_type_vaction)
                return (xvaction_t*)this;
            
            return xvmethod_t::query_minterface(_enum_xobject_type_);
        }
    
    };//end of namespace of base
};//end of namespace of top
