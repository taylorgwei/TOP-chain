// Copyright (c) 2018-2020 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once
 
#include <string>
#include <vector>
#include "xvaction.h"
#include "xventity.h"
#include "xvexeunit.h"
#include "xvcontract.h"

namespace top
{
    namespace base
    {
        class xvexecontext_t
        {
        protected:
            xvcontract_t *  load_contract(const std::string & contract_addr, const std::string & contract_name)
            {
                const std::string contract_key = contract_addr + "." + contract_name;
                auto it = m_loaded_contracts.find(contract_key);
                if(it != m_loaded_contracts.end())
                    return it->second;
                
                return NULL;
            }
        public:
            /*
            //[input entity]->[contract]->[output_entity]
            virtual xauto_ptr<xvoutentity_t>  execute(xvinentity_t * input_entity)
            {
                xauto_ptr<xvcanvas_t> output_canvas(new xvcanvas_t());
                xassert(input_entity != NULL);
                if(input_entity != NULL)
                {
                    for(auto & action_ptr : input_entity->get_actions())
                    {
                        if(action_ptr != NULL)
                        {
                            xvcontract_t * target_contract = load_contract(action_ptr->get_contract_address(),action_ptr->get_contract_name());
                            if(target_contract != NULL)
                            {
                                target_contract->execute(action_ptr->get_method(),output_canvas.get());
                            }
                        }
                    }
                }
                std::string output_bin_log;
                output_canvas->encode(xvcanvas_t::enum_compile_optimization_all,output_bin_log);
                
                return new xvoutentity_t(output_bin_log);
            }
            //[input entity...]->[VM]->[output_entity...]
            virtual std::vector< xauto_ptr<xvoutentity_t> > execute(std::vector<xvinentity_t*> & input_entities)
            {
                std::vector< xauto_ptr<xvoutentity_t> > output_entities;
                for(auto & entity : input_entities)
                {
                    output_entities.emplace_back(execute(entity));
                }
                return output_entities;
            }
            */
        private:
            std::map<std::string,xvcontract_t*> m_loaded_contracts;
        };

    }//end of namespace of base

}//end of namespace top
