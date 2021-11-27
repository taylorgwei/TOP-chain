// Copyright (c) 2018-Present Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xvmigrate.h"
#include "xvledger/xvdbstore.h"
#include "xvledger/xvdbfilter.h"

namespace top
{
    namespace base
    {
        class xdbtransfer_event : public xdbevent_t
        {
        public:
            xdbtransfer_event(xvdbstore_t* src_db_ptr,xvdbstore_t* dst_db_ptr,enum_xdbevent_code code);
            xdbtransfer_event(const std::string & db_key,const std::string & db_value,enum_xdbkey_type db_key_type,xvdbstore_t* src_db_ptr,xvdbstore_t* dst_db_ptr,enum_xdbevent_code code);
            virtual ~xdbtransfer_event();
        private:
            xdbtransfer_event();
            xdbtransfer_event(xdbtransfer_event &&);
            xdbtransfer_event(const xdbtransfer_event & obj);
            xdbtransfer_event& operator = (const xdbtransfer_event & obj);
            
        public:
            inline xvdbstore_t*  get_source_store() const {return get_db_store();}
            inline xvdbstore_t*  get_target_store() const {return m_target_store;}
        private:
            xvdbstore_t*   m_target_store; //note:just copy ptr without reference control
        };
    
        template<uint32_t migrate_version>
        class xdbmigrate_t : public xvmigrate_t,public xsyscreator<xdbmigrate_t<migrate_version> >
        {
            friend class xsyscreator<xdbmigrate_t<migrate_version> >;
            static const char* get_register_key()
            {
                return "/init/migrate/db";
            }
            static const uint32_t get_register_version()
            {
                return migrate_version;
            }
        protected:
            xdbmigrate_t()
            {
                xsysobject_t::set_object_version(get_register_version());
            }
            virtual ~xdbmigrate_t()
            {
            }
        private:
            xdbmigrate_t(xdbmigrate_t &&);
            xdbmigrate_t(const xdbmigrate_t &);
            xdbmigrate_t & operator = (const xdbmigrate_t &);
        public:
            virtual bool  is_valid(const uint32_t obj_ver) override//check version
            {
                return xvmigrate_t::is_valid(obj_ver);
            }
        };

        #define declare_db_migrate(version) \
            class xdbmigrate_##version : public xdbmigrate_t<version> \
            {\
            protected:\
                xdbmigrate_##version(){};\
                virtual ~xdbmigrate_##version(){};\
            };
    

        //***************************DECLARE OBJECTS WITH VERSION***************************//
        declare_db_migrate(1);

    
    }//end of namespace of base
}//end of namespace top
