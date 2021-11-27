// Copyright (c) 2018-Present Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once
 
#include "xdbmigrate.h"

namespace top
{
    namespace base
    {
        template<uint32_t migrate_version>
        class xkeymigrate_t : public xkeyvfilter_t,public xsyscreator<xkeymigrate_t<migrate_version> >
        {
            friend class xsyscreator<xkeymigrate_t<migrate_version> >;
            static const char* get_register_key()
            {
                return "/init/migrate/db/key";
            }
            static const uint32_t get_register_version()
            {
                return migrate_version;
            }
        protected:
            xkeymigrate_t()
            {
                xsysobject_t::set_object_version(get_register_version());
            }
            virtual ~xkeymigrate_t()
            {
            }
        private:
            xkeymigrate_t(xkeymigrate_t &&);
            xkeymigrate_t(const xkeymigrate_t &);
            xkeymigrate_t & operator = (const xkeymigrate_t &);
        public:
            virtual bool  is_valid(const uint32_t obj_ver) override//check version
            {
                return xkeyvfilter_t::is_valid(obj_ver);
            }
        };
            
        #define declare_key_migrate(version) \
            class xkeymigrate_##version : public xkeymigrate_t<version> \
            {\
            protected:\
                xkeymigrate_##version(){};\
                virtual ~xkeymigrate_##version(){};\
            };
    
        //***************************DECLARE OBJECTS WITH VERSION***************************//
        declare_key_migrate(1);
    
    }//end of namespace of base
}//end of namespace top
