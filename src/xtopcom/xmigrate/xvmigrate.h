// Copyright (c) 2018-Present Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xvledger/xvfilter.h"
#include "xvledger/xvsysobj.h"

namespace top
{
    namespace base
    {
        class xvmigrate_t : public xvmodule_t
        {
        protected:
            xvmigrate_t(){};
            virtual ~xvmigrate_t(){};
        private:
            xvmigrate_t(xvmigrate_t &&);
            xvmigrate_t(const xvmigrate_t &);
            xvmigrate_t & operator = (const xvmigrate_t &);
        };
    
        class xvmigrate_context
        {
            
        };

        //0. global instance -- registered into xvchain
        //1. version control,every filter and migrate contorl must match version
        //2. factory register & create ->based version
        //3. macro define and register -->template create custom class
        //4. configure drive to load , json files -> catelog & name & version -> create object and combined
    
        //flow: 1.)config ->create policy object and fill policy  2.)policy object ->create handler object ->create filter objects -> connect those objects ->finally serving
    
    }//end of namespace of base
}//end of namespace top
