// Copyright (c) 2018-2020 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once
 
#include "xbase/xvmethod.h"

namespace top
{
    namespace base
    {
        //Action is a fucntion-call of contract,which required to execute at the specified "ContractAddr.ContractName.Block-height.*"
        class xvaction_t : public xvmethod_t
        {
        public:
            enum{enum_obj_type = enum_xobject_type_vaction};
        public:
            xvaction_t(const std::string & caller_addr,const std::string & target_uri,const uint8_t method_type,const std::string & method_name);
            xvaction_t(const std::string & caller_addr,const std::string & target_uri,const uint8_t method_type,const std::string & method_name,xvalue_t & param);
            xvaction_t(const std::string & caller_addr,const std::string & target_uri,const uint8_t method_type,const std::string & method_name,xvalue_t & param1,xvalue_t & param2);
            xvaction_t(const std::string & caller_addr,const std::string & target_uri,const uint8_t method_type,const std::string & method_name,xvalue_t & param1,xvalue_t & param2,xvalue_t & param3);
            
            xvaction_t(const xvaction_t & obj);
            xvaction_t(xvaction_t && moved);
            xvaction_t & operator = (const xvaction_t & obj);
            xvaction_t & operator = (xvaction_t && moved);
            virtual ~xvaction_t();
            
        protected:
            xvaction_t();
        public:
            //caller respond to cast (void*) to related  interface ptr
            virtual void*      query_minterface(const int32_t _enum_xobject_type_) const override;
            
            const std::string   get_contract_caller()    const {return get_caller_uri();}
            const std::string   get_contract_uri()       const {return get_method_uri();}
            const std::string   get_contract_address()   const {return m_contract_addr;}
            const std::string   get_contract_name()      const {return m_contract_name;}
            const xvmethod_t &  get_contract_function()  const {return *this;}
            
            const int64_t   get_used_tgas()          const {return m_used_tgas;}
            const int64_t   get_max_tgas()           const {return m_max_tgas;}
            void            add_used_tgas(const int64_t _tags) { m_used_tgas += _tags;}
            void            set_max_tgas(const int64_t new_max_tags)   { m_max_tgas = new_max_tags;}
            
        private:
            void            init(const int64_t init_max_tags = 0,const int64_t init_used_tgas = 0);
            std::string     m_contract_addr;     //which contract. contract addr is same as account address
            std::string     m_contract_name;     //contract name could be "default",or like "TEP0","TEP1"
            uint64_t        m_taget_block_height;//ask executed at target block'height
            
        private://those are just work at runtime,NOT included into serialization
            int64_t         m_max_tgas;          //max tgas allow used for this method
            int64_t         m_used_tgas;         //return how many tgas(virtual cost) used for caller
        };

    }//end of namespace of base

}//end of namespace top
