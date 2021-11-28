// Copyright (c) 2018-Present Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once
#include "../xvmigrate.h"
#include "xvledger/xvdbstore.h"
#include "xdb/xdb_face.h"

namespace top
{
    namespace base
    {
        class xvfilter_t;
        class xvblock_t;
        class xdb_face_t;
    
        // wrapper for the block/tx storing in xdb
        class xmigratedb_t : public xvdbstore_t
        {
        public:
            xmigratedb_t(const std::string & db_path);
        protected:
            virtual ~xmigratedb_t();
        private:
            xmigratedb_t();
            xmigratedb_t(xmigratedb_t &&);
            xmigratedb_t(const xmigratedb_t &);
            xmigratedb_t & operator = (const xmigratedb_t &);
            
        public://key-value manage
            virtual bool                set_value(const std::string & key, const std::string& value) override;
            virtual bool                delete_value(const std::string & key) override;
            virtual const std::string   get_value(const std::string & key) const override;
            virtual bool                delete_values(std::vector<std::string> & to_deleted_keys) override;
            
        public:
            //prefix must start from first char of key
            virtual bool             read_range(const std::string& prefix, std::vector<std::string>& values) override;
            //note:begin_key and end_key must has same style(first char of key)
            virtual bool             delete_range(const std::string & begin_key,const std::string & end_key) override;
            //key must be readonly(never update after PUT),otherwise the behavior is undefined
            virtual bool             single_delete(const std::string & target_key) override;
            
            //iterator each key of prefix.note: go throuh whole db if prefix is empty
            bool   read_range(const std::string& prefix,db::xdb_iterator_callback callback,void * cookie);
            
        private://API for block object manage,useless while migrating
            virtual bool             set_vblock(const std::string & store_path,base::xvblock_t* block) override{return false;}
            virtual bool             set_vblock_header(const std::string & store_path,base::xvblock_t* block) override{return false;}
            virtual base::xvblock_t* get_vblock(const std::string & store_path,const std::string & account, const uint64_t height) const override{return nullptr;}
            virtual base::xvblock_t* get_vblock_header(const std::string & store_path,const std::string & account,const uint64_t height) const override{return nullptr;}
            virtual bool             get_vblock_input(const std::string & store_path,base::xvblock_t* for_block)  const override{return false;}
            virtual bool             get_vblock_output(const std::string & store_path,base::xvblock_t* for_block) const override{return false;}
        public:
            virtual std::string      get_store_path() const  override {return m_store_path;}
 
        private:
            std::string              m_store_path;
            std::shared_ptr<db::xdb_face_t> m_db_face_ptr;
        };
    
        class xdbmigrate_t : public xvmigrate_t
        {
        public:
            static const char* get_register_key(){return "/init/migrate/db";}
            static bool  db_scan_callback(const std::string& key, const std::string& value,void*cookie);
        protected:
            xdbmigrate_t();
            virtual ~xdbmigrate_t();
        private:
            xdbmigrate_t(xdbmigrate_t &&);
            xdbmigrate_t(const xdbmigrate_t &);
            xdbmigrate_t & operator = (const xdbmigrate_t &);
        public:
            virtual bool  is_valid(const uint32_t obj_ver)  override;//check version
            virtual int   init(const xvconfig_t & config)   override;//init config
            virtual bool  start(const int32_t at_thread_id) override;//start system object
        protected:
            virtual bool  run(const int32_t cur_thread_id,const uint64_t timenow_ms) override;//process
            bool  db_scan_callback(const std::string& key, const std::string& value);
        private:
            xmigratedb_t*             m_src_store_ptr;//source db
            xmigratedb_t*             m_dst_store_ptr;//target db
            std::vector<xvfilter_t*>  m_filter_objects;
        };
    
        template<uint32_t migrate_version>
        class xdbmigrate_ver : public xdbmigrate_t,public xsyscreator<xdbmigrate_ver<migrate_version> >
        {
            friend class xsyscreator<xdbmigrate_ver<migrate_version> >;
        public:
            static const uint32_t get_register_version(){ return migrate_version;}
        protected:
            xdbmigrate_ver()
            {
                xsysobject_t::set_object_version(get_register_version());
            }
            virtual ~xdbmigrate_ver(){}
        private:
            xdbmigrate_ver(xdbmigrate_ver &&);
            xdbmigrate_ver(const xdbmigrate_ver &);
            xdbmigrate_ver & operator = (const xdbmigrate_ver &);
        };

        #define DECLARE_DB_MIGRATE(version) \
            class xdbmigrate_ver##version : public xdbmigrate_ver<version> \
            {\
            public:\
                xdbmigrate_ver##version(){};\
                virtual ~xdbmigrate_ver##version(){};\
            };\
           
        //***************************DECLARE OBJECTS WITH VERSION***************************//
        DECLARE_DB_MIGRATE(1);

    
    }//end of namespace of base
}//end of namespace top
