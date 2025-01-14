// Copyright (c) 2018-2020 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once
 
#include "xbase/xvmethod.h"

namespace top
{
    namespace base
    {
        //xvaction_t is a fucntion-call of contract,which required to execute at uri:"Type/ContractAddr/Version"
        //xvaction_t is multiple-thread safe
        class xvaction_t : public xvmethod_t
        {
            friend class xvinentity_t;
        public:
            enum{enum_obj_type = enum_xobject_type_vaction};
        public:
            xvaction_t(const std::string & tx_hash,const std::string & caller_addr,const std::string & target_uri,const std::string & method_name);
            xvaction_t(const std::string & tx_hash,const std::string & caller_addr,const std::string & target_uri,const std::string & method_name,xvalue_t & param);
            xvaction_t(const std::string & tx_hash,const std::string & caller_addr,const std::string & target_uri,const std::string & method_name,xvalue_t & param1,xvalue_t & param2);
            xvaction_t(const std::string & tx_hash,const std::string & caller_addr,const std::string & target_uri,const std::string & method_name,xvalue_t & param1,xvalue_t & param2,xvalue_t & param3);
            
            xvaction_t(const xvaction_t & obj);
            xvaction_t(xvaction_t && moved);
            virtual ~xvaction_t();
            
        private:
            xvaction_t();
            xvaction_t & operator = (const xvaction_t & obj);
            xvaction_t & operator = (xvaction_t && moved); //dont implement it
            
        public:
            //caller respond to cast (void*) to related  interface ptr
            virtual void*       query_minterface(const int32_t _enum_xobject_type_) const override;
            
            inline const std::string   get_caller()             const {return get_caller_uri();}
            inline const std::string   get_contract_uri()       const {return get_method_uri();}
            inline const xvmethod_t&   get_contract_function()  const {return *this;}
            inline const std::string   get_org_tx_hash()        const {return m_org_tx_hash;}

        protected:
            //serialize header and object,return how many bytes is writed
            virtual int32_t do_write(xstream_t & stream) const override;
            virtual int32_t do_read(xstream_t & stream) override;
            
            using           xvmethod_t::serialize_from; //just open for certain friend class to use
        private:
            void            parse_uri();
            void            close();
            std::string     m_org_tx_hash;       //which transaction generated this action
        };

    }//end of namespace of base

}//end of namespace top
