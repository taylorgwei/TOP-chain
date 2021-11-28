// Copyright (c) 2018-Present Telos Foundation & contributors
// taylor.wei@topnetwork.org
// Licensed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvsysobj.h"
#include "xbase/xutl.h"
#include "xbase/xcontext.h"

namespace top
{
    namespace base
    {
        //version defintion = [8:Features][8:MAJOR][8:MINOR][8:PATCH]
        const std::string xsysobject_t::version_to_string(const uint32_t ver_int)
        {
            char szBuff[32] = {0};
            const int inBufLen = sizeof(szBuff);
            snprintf(szBuff,inBufLen,"%d.%d.%d.%d",(ver_int>>24) & 0xFF,(ver_int >> 16) & 0xFF,(ver_int >> 8) & 0xFF,(ver_int & 0xFF));
            return szBuff;
        }
        
        const uint32_t    xsysobject_t::string_to_version(const std::string & ver_str)
        {
            if(ver_str.empty())
                return 0;
            
            std::vector<std::string> parts;
            if(xstring_utl::split_string(ver_str, '.', parts) == 4)
            {
                uint32_t h0 = xstring_utl::touint32(parts[0]);
                uint32_t h1 = xstring_utl::touint32(parts[1]);
                uint32_t h2 = xstring_utl::touint32(parts[2]);
                uint32_t h3 = xstring_utl::touint32(parts[3]);
                if( (h0 > 255) || (h1 > 255) || (h2 > 255) || (h3 > 255) )
                {
                    xwarn("string_to_version,invalid version(%s)",ver_str.c_str());
                    return 0;
                }
                return ( (h0 << 24) | (h1 << 16) || (h2 << 8) || h3);
            }
            xwarn("string_to_version,invalid version(%s)",ver_str.c_str());
            return 0;
        }
    
        xsysobject_t::xsysobject_t()
        {
            m_obj_version = 0;
            m_raw_iobject = nullptr;
        }
    
        xsysobject_t::xsysobject_t(const int16_t _object_type) //_enum_xobject_type_ refer enum_xobject_type
            :xobject_t(_object_type)
        {
            m_obj_version = 0;
            m_raw_iobject = nullptr;
        }
    
        xsysobject_t::~xsysobject_t()
        {
            if(m_raw_iobject != nullptr)
            {
                m_raw_iobject->close(false);
                m_raw_iobject->release_ref();
            }
        }
    
        const uint64_t    xsysobject_t::get_time_now()
        {
            if(m_raw_iobject != nullptr)
                return m_raw_iobject->get_time_now();
            else
                return xtime_utl::time_now_ms();
        }
    
        const int32_t     xsysobject_t::get_thread_id() const //0 means no-bind to any thread yet
        {
            if(m_raw_iobject != nullptr)
                return m_raw_iobject->get_thread_id();
            else
                return 0;
        }
    
        xcontext_t*       xsysobject_t::get_context() const
        {
            if(m_raw_iobject != nullptr)
                return m_raw_iobject->get_context();
            else
                return &xcontext_t::instance();
        }
            
        //only allow for the uninited object
        bool  xsysobject_t::set_object_type(const int16_t sys_object_type)
        {
            if(get_obj_type() == enum_sys_object_type_undef)
            {
                set_type(sys_object_type);
                return true;
            }
            return false;
        }
    
        bool  xsysobject_t::set_object_key(const char* ptr_obj_key)
        {
            if(m_obj_key.empty() && (ptr_obj_key != nullptr))
            {
                m_obj_key = ptr_obj_key;
                return true;
            }
            return false;
        }
    
        bool  xsysobject_t::set_object_key(const std::string & obj_key)
        {
            if(m_obj_key.empty())
            {
                m_obj_key = obj_key;
                return true;
            }
            return false;
        }
    
        bool  xsysobject_t::set_object_version(const uint32_t obj_ver)
        {
            if(0 == m_obj_version)
            {
                m_obj_version = obj_ver;
                return true;
            }
            return false;
        }
    
        bool  xsysobject_t::is_valid(const uint32_t obj_ver) //check version
        {
            if( (0 == obj_ver) || (0 == m_obj_version) )//for any case
                return true;
            
            if(m_obj_version >= obj_ver)
                return true;
            
            xwarn("xsysobject_t::is_valid,failed for obj_ver(0x%x) vs m_obj_version(0x%x)",obj_ver,m_obj_version);
            return false;
        }
    
        bool   xsysobject_t::run(const int32_t at_thread_id)
        {
            class _localiobject : public xiobject_t
            {
            public:
                _localiobject(const int32_t at_thread_id)
                    :xiobject_t(xcontext_t::instance(),at_thread_id,enum_xobject_type_iobject)
                {
                }
                ~_localiobject()
                {
                    
                }
            };
            
            xassert(nullptr == m_raw_iobject);
            if(nullptr == m_raw_iobject)
            {
                m_raw_iobject = new _localiobject(at_thread_id);
            }
            return (m_raw_iobject != nullptr);
        }
        
        bool  xvsyslibrary::register_object(const char * object_key,xnew_sysobj_function_t creator_func_ptr)
        {
            xauto_lock<xspinlock_t> locker(m_lock);
            if(object_key != nullptr)
            {
                auto& it_ref_item = m_key_to_obj[std::string(object_key)];
                if(it_ref_item != nullptr)
                    return true;
                
                it_ref_item = creator_func_ptr;
                return true;
            }
            xassert(0);
            return false;
        }
    
        bool  xvsyslibrary::register_object(const int   object_type,xnew_sysobj_function_t creator_func_ptr)
        {
            xauto_lock<xspinlock_t> locker(m_lock);
            auto& it_ref_item = m_type_to_obj[object_type];
            if(it_ref_item != nullptr)
                return true;
            
            it_ref_item = creator_func_ptr;
            return true;
        }
    
        xsysobject_t*  xvsyslibrary::create_object(const int obj_type,const uint32_t obj_ver)
        {
            xauto_lock<xspinlock_t> locker(m_lock);
            
            auto it = m_type_to_obj.find(obj_type);
            if(it != m_type_to_obj.end())
                return (*it->second)(obj_ver);
            
            if(0 == obj_ver)
                xerror("xvsyslibrary::create_object,fail to found object of obj_type(%d) and obj_ver(0x%x)",obj_type,obj_ver);
            else
                xwarn("xvsyslibrary::create_object,fail to found object of obj_type(%d) and obj_ver(0x%x)",obj_type,obj_ver);
            return nullptr;
        }
     
        xsysobject_t*  xvsyslibrary::create_object(const char* ptr_obj_key,const uint32_t obj_ver)
        {
            if(nullptr == ptr_obj_key)
            {
                xassert(ptr_obj_key != nullptr);
                return nullptr;
            }
            
            const std::string obj_key(ptr_obj_key);
            return create_object(obj_key);
        }
    
        xsysobject_t*  xvsyslibrary::create_object(const std::string & obj_key,const uint32_t obj_ver)
        {
            xauto_lock<xspinlock_t> locker(m_lock);
            
            auto it = m_key_to_obj.find(obj_key);
            if(it != m_key_to_obj.end())
                return (*it->second)(obj_ver);
            
            if(0 == obj_ver)
                xerror("xvsyslibrary::create_object,fail to found object of obj_key(%s) and obj_ver(0x%x)",obj_key.c_str(),obj_ver);
            else
                xwarn("xvsyslibrary::create_object,fail to found object of obj_key(%s) and obj_ver(0x%x)",obj_key.c_str(),obj_ver);
            return nullptr;
        }
    
        //key_path must be formated as dot path like xx.yyy.zzz...
        const std::string  xvconfig_t::get_config(const std::string & key_path) const
        {
            xauto_lock<xspinlock_t> locker( (xspinlock_t&)m_lock);//force cast
            auto it = m_keys_map.find(key_path);
            if(it != m_keys_map.end())
                return it->second;
            
            return std::string();
        }
        bool               xvconfig_t::set_config(const std::string & key_path,const std::string & key_value)
        {
            xauto_lock<xspinlock_t> locker(m_lock);
            m_keys_map[key_path] = key_value;
            return true;
        }
    
        xvbootstrap_t::xvbootstrap_t()
        {
            set_object_type(xsysobject_t::enum_sys_object_type_bootstrap);
        }
    
        xvbootstrap_t::~xvbootstrap_t()
        {
        }
    
        //caller respond to cast (void*) to related  interface ptr
        void*  xvbootstrap_t::query_interface(const int32_t _enum_xobject_type_)
        {
            if(_enum_xobject_type_ == enum_sys_object_type_bootstrap)
                return this;
            
            return xsysobject_t::query_interface(_enum_xobject_type_);
        }
        
        xvmodule_t::xvmodule_t()
        {
            set_object_type(xsysobject_t::enum_sys_object_type_module);
        }
    
        xvmodule_t::~xvmodule_t()
        {
        };
        
        //caller respond to cast (void*) to related  interface ptr
        void*  xvmodule_t::query_interface(const int32_t _enum_xobject_type_)
        {
            if(_enum_xobject_type_ == enum_sys_object_type_module)
                return this;
            
            return xsysobject_t::query_interface(_enum_xobject_type_);
        }
        
        xvdaemon_t::xvdaemon_t()
        {
            set_object_type(xsysobject_t::enum_sys_object_type_daemon);
        }
        
        xvdaemon_t::~xvdaemon_t()
        {
        }
    
        //caller respond to cast (void*) to related  interface ptr
        void*  xvdaemon_t::query_interface(const int32_t _enum_xobject_type_)
        {
            if(_enum_xobject_type_ == enum_sys_object_type_daemon)
                return this;
            
            return xsysobject_t::query_interface(_enum_xobject_type_);
        }
        
        xvdriver_t::xvdriver_t()
        {
            set_object_type(xsysobject_t::enum_sys_object_type_driver);
        }
        
        xvdriver_t::~xvdriver_t()
        {
            
        }
        
        //caller respond to cast (void*) to related  interface ptr
        void*  xvdriver_t::query_interface(const int32_t _enum_xobject_type_)
        {
            if(_enum_xobject_type_ == enum_sys_object_type_driver)
                return this;
            
            return xsysobject_t::query_interface(_enum_xobject_type_);
        }

        xvsysinit_t::xvsysinit_t()
        {
            set_object_version(xvsysinit_t::get_register_version());
        }
    
        xvsysinit_t::~xvsysinit_t()
        {
            for(size_t it = 0; it < m_boot_objects.size(); ++it)
            {
                xsysobject_t * obj_ptr = m_boot_objects[it];
                if(obj_ptr != nullptr)
                {
                    obj_ptr->close();
                    obj_ptr->release_ref();
                }
            }
            m_boot_objects.clear();
        }
    
        int  xvsysinit_t::init(const xvconfig_t & config_obj)
        {
            //step#1 : version check
            const std::string system_version_str = config_obj.get_config("system.version");
            if(system_version_str.empty())
            {
                xerror("xvsysinit_t::init,empty sytem version of config(%s)",config_obj.dump().c_str());
                return enum_xerror_code_bad_version_code;
            }
            const uint32_t system_version = xsysobject_t::string_to_version(system_version_str);
            if(is_valid(system_version) == false)
            {
                xerror("xvsysinit_t::init,invalid system version(%s) of config(%s)",system_version_str.c_str(), config_obj.dump().c_str());
                return enum_xerror_code_bad_version_code;
            }
            
            //step#2 : load boot items at order
            const int boot_module_count = (int)xstring_utl::toint32(config_obj.get_config("system.boot.size"));
            for(int i = 0; i < boot_module_count; ++i)
            {
                const std::string config_path = "system.boot." + xstring_utl::tostring(i);
                const std::string object_key = config_obj.get_config(config_path + ".object_key");
                const uint32_t object_version = xsysobject_t::string_to_version(config_obj.get_config(config_path + ".object_version"));
                if(object_key.empty())
                {
                    xerror("xvsysinit_t::init,bad config(%s)",config_obj.dump().c_str());
                    return enum_xerror_code_bad_config;
                }
                
                xsysobject_t * boot_object = xvsyslibrary::instance().create_object(object_key,object_version);
                if(nullptr == boot_object)
                {
                    xerror("xvsysinit_t::init,failed to load system object of key(%s) and version(0x%x)",object_key.c_str(),object_version);
                    return enum_xerror_code_not_found;
                }
                
                if(boot_object->init(config_obj) != enum_xcode_successful)
                {
                    xerror("xvsysinit_t::init,failed to init system object of key(%s) and version(0x%x)",object_key.c_str(),object_version);
                    
                    boot_object->close();
                    boot_object->release_ref();
                    return enum_xerror_code_fail;
                }
                m_boot_objects.push_back(boot_object);
            }
            
            //step#3 : release unneed boot objects if have
            for(size_t it = 0; it < m_boot_objects.size(); ++it)
            {
                if(m_boot_objects[it]->is_close())//just clean closed object
                {
                    m_boot_objects[it]->release_ref();
                    m_boot_objects[it] = nullptr;
                }
            }
            
            return enum_xcode_successful;
        }
    
    }//end of namespace of base
}//end of namespace top
