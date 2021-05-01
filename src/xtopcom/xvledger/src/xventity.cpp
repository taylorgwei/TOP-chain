// Copyright (c) 2018-2020 Telos Foundation & contributors
// taylor.wei@topnetwork.org
// Licensed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <cinttypes>
#include "../xventity.h"
#include "xbase/xcontext.h"
 
namespace top
{
    namespace base
    {
        xventity_t::xventity_t(enum_xdata_type type)
        :xdataunit_t(type)
        {
            m_exe_module = NULL;
            m_entity_index = uint16_t(-1);
        }
        
        xventity_t::~xventity_t()
        {
            if(m_exe_module != NULL)
                m_exe_module->release_ref();
        }
        
        bool xventity_t::close(bool force_async)
        {
            set_exe_module(NULL); //reset to null
            
            return xdataunit_t::close(force_async);
        }
        
        void  xventity_t::set_exe_module(xvexemodule_t * exemodule_ptr)
        {
            if(exemodule_ptr != NULL)
                exemodule_ptr->add_ref();
            
            xvexemodule_t * old_ptr = xatomic_t::xexchange(m_exe_module, exemodule_ptr);
            if(old_ptr != NULL)
            {
                old_ptr->release_ref();
                old_ptr = NULL;
            }
        }
    
        //caller need to cast (void*) to related ptr
        void*  xventity_t::query_interface(const int32_t _enum_xobject_type_)
        {
            if(_enum_xobject_type_ == enum_xobject_type_ventity)
                return this;
            
            return xdataunit_t::query_interface(_enum_xobject_type_);
        }
        
        int32_t   xventity_t::do_write(xstream_t & stream) //allow subclass extend behavior
        {
            const int32_t begin_size = stream.size();
            //stream.write_short_string(m_raw_data);
            stream << m_entity_index;
            return (stream.size() - begin_size);
        }
        
        int32_t   xventity_t::do_read(xstream_t & stream)  //allow subclass extend behavior
        {
            const int32_t begin_size = stream.size();
            //stream.read_short_string(m_raw_data);
            stream >> m_entity_index;
            return (begin_size - stream.size());
        }
        
        xvbinentity_t::xvbinentity_t()
        :xventity_t(enum_xdata_type(enum_xobject_type_binventity))
        {
        }
        
        xvbinentity_t::xvbinentity_t(const std::string & raw_bin_data)
        :xventity_t(enum_xdata_type(enum_xobject_type_binventity))
        {
            m_raw_data = raw_bin_data;
        }
        
        xvbinentity_t::~xvbinentity_t()
        {
        }
    
        //caller need to cast (void*) to related ptr
        void*   xvbinentity_t::query_interface(const int32_t _enum_xobject_type_)
        {
            if(_enum_xobject_type_ == enum_xobject_type_binventity)
                return this;
            
            return xventity_t::query_interface(_enum_xobject_type_);
        }
        
        int32_t   xvbinentity_t::do_write(xstream_t & stream) //allow subclass extend behavior
        {
            const int32_t begin_size = stream.size();
            xventity_t::do_write(stream);
            
            //stream.write_short_string(m_raw_data);
            stream << m_raw_data;
            return (stream.size() - begin_size);
        }
        
        int32_t   xvbinentity_t::do_read(xstream_t & stream)  //allow subclass extend behavior
        {
            m_raw_data.clear();
            const int32_t begin_size = stream.size();
            xventity_t::do_read(stream);
            
            //stream.read_short_string(m_raw_data);
            stream >> m_raw_data;
            return (begin_size - stream.size());
        }
        
        xvexemodule_t::xvexemodule_t(enum_xdata_type type)
            :xdataunit_t(type)
        {
            m_resources_obj = NULL;
        }
        
        xvexemodule_t::xvexemodule_t(const std::vector<xventity_t*> & entitys, const std::string & raw_resource_data,enum_xdata_type type)
            :xdataunit_t(type)
        {
            m_resources_obj = NULL;
            set_resources_data(raw_resource_data);
            
            for(size_t i = 0; i < entitys.size(); ++i)
            {
                xventity_t * v = entitys[i];
                v->add_ref();
                v->set_entity_index(i);
                v->set_exe_module(this);
                m_entitys.push_back(v);
            }
        }
        
        xvexemodule_t::xvexemodule_t(const std::vector<xventity_t*> & entitys,xstrmap_t & resource_obj, enum_xdata_type type)
            :xdataunit_t(type)
        {
            resource_obj.add_ref();
            m_resources_obj = &resource_obj;
            
            for(size_t i = 0; i < entitys.size(); ++i)
            {
                xventity_t * v = entitys[i];
                v->add_ref();
                v->set_entity_index(i);
                v->set_exe_module(this);
                m_entitys.push_back(v);
            }
        }
        
        xvexemodule_t::~xvexemodule_t()
        {
            for (auto & v : m_entitys){
                v->close();
                v->release_ref();
            }
            
            if(m_resources_obj != NULL){
                m_resources_obj->close();
                m_resources_obj->release_ref();
            }
        }
        
        bool xvexemodule_t::close(bool force_async)
        {
            for (auto & v : m_entitys)
                v->close();
            
            if(m_resources_obj != NULL)
                m_resources_obj->close();
            
            return xdataunit_t::close(force_async);
        }
        
        //note:not safe for multiple thread at this layer
        const std::string xvexemodule_t::query_resource(const std::string & key)//virtual key-value for query resource
        {
            auto_reference<xstrmap_t> map_ptr(xatomic_t::xload(m_resources_obj));
            if(map_ptr != nullptr)
            {
                std::string value;
                map_ptr->get(key,value);
                return value;
            }
            return std::string(); //return empty one
        }
        
        const std::string xvexemodule_t::get_resources_data() //serialzie whole extend resource into one single string
        {
            auto_reference<xstrmap_t> map_ptr(xatomic_t::xload(m_resources_obj ));
            if(map_ptr == nullptr)
                return std::string();
            
            if(map_ptr->size() == 0)
                return std::string();
            
            std::string raw_bin;
            map_ptr->serialize_to_string(raw_bin);
            return raw_bin;
        }
        
        //xvblock has verifyed that raw_resource_data matched by raw_resource_hash
        bool   xvexemodule_t::set_resources_hash(const std::string & raw_resources_hash)
        {
            m_resources_hash = raw_resources_hash;
            return true;
        }
        
        //xvblock has verifyed that raw_resource_data matched by raw_resource_hash
        bool   xvexemodule_t::set_resources_data(const std::string & raw_resource_data)
        {
            if(raw_resource_data.empty() == false)
            {
                xstream_t _stream(xcontext_t::instance(),(uint8_t*)raw_resource_data.data(),(uint32_t)raw_resource_data.size());
                xdataunit_t*  _data_obj_ptr = xdataunit_t::read_from(_stream);
                xassert(_data_obj_ptr != NULL);
                if(NULL == _data_obj_ptr)
                    return false;
                
                xstrmap_t*  map_ptr = (xstrmap_t*)_data_obj_ptr->query_interface(enum_xdata_type_string_map);
                xassert(map_ptr != NULL);
                if(map_ptr == NULL)
                {
                    _data_obj_ptr->release_ref();
                    return false;
                }
                
                xstrmap_t * old_ptr = xatomic_t::xexchange(m_resources_obj, map_ptr);
                if(old_ptr != NULL){
                    old_ptr->release_ref();
                    old_ptr = NULL;
                }
            }
            return true;
        }
        
        int32_t     xvexemodule_t::do_write(xstream_t & stream)//not allow subclass change behavior
        {
            const int32_t begin_size = stream.size();
            stream.write_tiny_string(m_resources_hash);
            
            const uint16_t count = (uint16_t)m_entitys.size();
            stream << count;
            for (auto & v : m_entitys) {
                v->serialize_to(stream);
            }
            return (stream.size() - begin_size);
        }
        
        int32_t     xvexemodule_t::do_read(xstream_t & stream) //not allow subclass change behavior
        {
            const int32_t begin_size = stream.size();
            stream.read_tiny_string(m_resources_hash);
            
            uint16_t count = 0;
            stream >> count;
            for (uint32_t i = 0; i < count; i++) {
                xventity_t* v = dynamic_cast<xventity_t*>(base::xdataunit_t::read_from(stream));
                m_entitys.push_back(v);
            }
            return (begin_size - stream.size());
        }
      
    };//end of namespace of base
};//end of namespace of top
