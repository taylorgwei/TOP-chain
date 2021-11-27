// Copyright (c) 2018-Present Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xvledger/xvfilter.h"

namespace top
{
    namespace base
    {
        class xvdbstore_t; //forward delcare
        class xdbevent_t : public xvevent_t
        {
        public:
            xdbevent_t(xvdbstore_t* db_store_ptr,enum_xdbevent_code code);
            xdbevent_t(const std::string & db_key,const std::string & db_value,enum_xdbkey_type db_key_type,xvdbstore_t* db_store_ptr,enum_xdbevent_code code);
            virtual ~xdbevent_t();
        private:
            xdbevent_t();
            xdbevent_t(xdbevent_t &&);
            xdbevent_t(const xdbevent_t & obj);
            xdbevent_t& operator = (const xdbevent_t & obj);
            
        public://Event_ID = [4bit:code][4bit:class]
            static const int get_event_code(const int full_type) {return ((full_type >> 4) & 0x0F);}//4bit
            const int        get_event_code()     const {return ((get_type() >> 4) & 0x0F);} //4bit
            
            inline const std::string &  get_db_key()   const {return m_db_key;}
            inline const std::string &  get_db_value() const {return m_db_value;}
            inline enum_xdbkey_type     get_db_key_type() const{return m_db_key_type;}
            inline xvdbstore_t*         get_db_store() const {return m_db_store;}
            
        public: //for performance,let xdb operate db key & value directly
            inline std::string &        get_set_db_key()    {return m_db_key;}
            inline std::string &        get_set_db_value()  {return m_db_value;}
            inline enum_xdbkey_type&    get_set_db_type()   {return m_db_key_type;}
        private:
            std::string         m_db_key;
            std::string         m_db_value;
            enum_xdbkey_type    m_db_key_type;
            xvdbstore_t*        m_db_store; //note:just copy ptr without reference control
        };
    
        class xdbfilter_t : public xvfilter_t
        {
        protected:
            xdbfilter_t();
            xdbfilter_t(xdbfilter_t * front_filter);
            xdbfilter_t(xdbfilter_t * front_filter,xdbfilter_t * back_filter);
            virtual ~xdbfilter_t();
        private:
            xdbfilter_t(xdbfilter_t &&);
            xdbfilter_t(const xdbfilter_t &);
            xdbfilter_t & operator = (const xdbfilter_t &);
            
        private: //triggered by push_event_back or push_event_front
            virtual enum_xfilter_handle_code fire_event(const xvevent_t & event,xvfilter_t* last_filter) override;
            using xvfilter_t::get_event_handlers;
        };
    
        //generaly handle any key-value of DB
        class xkeyvfilter_t : public xdbfilter_t
        {
        protected:
            xkeyvfilter_t();
            xkeyvfilter_t(xdbfilter_t * front_filter);
            xkeyvfilter_t(xdbfilter_t * front_filter,xdbfilter_t * back_filter);
            virtual ~xkeyvfilter_t();
        private:
            xkeyvfilter_t(xkeyvfilter_t &&);
            xkeyvfilter_t(const xkeyvfilter_t &);
            xkeyvfilter_t & operator = (const xkeyvfilter_t &);
            
        protected:
            virtual enum_xfilter_handle_code  transfer_keyvalue(xdbevent_t & event,xvfilter_t* last_filter);
            
        private:
            enum_xfilter_handle_code on_keyvalue_transfer(const xvevent_t & event,xvfilter_t* last_filter);
        private:
            BEGIN_DECLARE_EVENT_HANDLER()
                REGISTER_EVENT(enum_xevent_category_db | enum_xdbevent_code_transfer | enum_xdbkey_type_keyvalue,on_keyvalue_transfer)
            END_DECLARE_EVENT_HANDLER()
        };
    
        //handle block
        class xbksfilter_t : public xdbfilter_t
        {
        protected:
            xbksfilter_t();
            xbksfilter_t(xdbfilter_t * front_filter);
            xbksfilter_t(xdbfilter_t * front_filter,xdbfilter_t * back_filter);
            virtual ~xbksfilter_t();
        private:
            xbksfilter_t(xbksfilter_t &&);
            xbksfilter_t(const xbksfilter_t &);
            xbksfilter_t & operator = (const xbksfilter_t &);
            
        protected:
            virtual enum_xfilter_handle_code    transfer_keyvalue(xdbevent_t & event,xvfilter_t* last_filter);
            virtual enum_xfilter_handle_code    transfer_block_index(xdbevent_t & event,xvfilter_t* last_filter);
            virtual enum_xfilter_handle_code    transfer_block_object(xdbevent_t & event,xvfilter_t* last_filter);
            
        private:
            enum_xfilter_handle_code on_keyvalue_transfer(const xvevent_t & event,xvfilter_t* last_filter);
            enum_xfilter_handle_code on_block_index_transfer(const xvevent_t & event,xvfilter_t* last_filter);
            enum_xfilter_handle_code on_block_object_transfer(const xvevent_t & event,xvfilter_t* last_filter);
        private:
            BEGIN_DECLARE_EVENT_HANDLER()
                REGISTER_EVENT(enum_xevent_category_db | enum_xdbevent_code_transfer | enum_xdbkey_type_keyvalue,on_keyvalue_transfer)
            
                REGISTER_EVENT(enum_xevent_category_db | enum_xdbevent_code_transfer | enum_xdbkey_type_block_index,on_block_index_transfer)
 
                REGISTER_EVENT(enum_xevent_category_db | enum_xdbevent_code_transfer | enum_xdbkey_type_block_object,on_block_object_transfer)
            END_DECLARE_EVENT_HANDLER()
        };
    
        //generaly handle any transaction
        class xtxsfilter_t : public xdbfilter_t
        {
        protected:
            xtxsfilter_t();
            xtxsfilter_t(xdbfilter_t * front_filter);
            xtxsfilter_t(xdbfilter_t * front_filter,xdbfilter_t * back_filter);
            virtual ~xtxsfilter_t();
        private:
            xtxsfilter_t(xtxsfilter_t &&);
            xtxsfilter_t(const xtxsfilter_t &);
            xtxsfilter_t & operator = (const xtxsfilter_t &);
           
        protected:
            virtual enum_xfilter_handle_code    transfer_keyvalue(xdbevent_t & event,xvfilter_t* last_filter);
            virtual enum_xfilter_handle_code    transfer_tx(xdbevent_t & event,xvfilter_t* last_filter);
            
        private:
            enum_xfilter_handle_code on_keyvalue_transfer(const xvevent_t & event,xvfilter_t* last_filter);
            enum_xfilter_handle_code on_tx_transfer(const xvevent_t & event,xvfilter_t* last_filter);
        private:
            BEGIN_DECLARE_EVENT_HANDLER()
                REGISTER_EVENT(enum_xevent_category_db | enum_xdbevent_code_transfer | enum_xdbkey_type_keyvalue,on_keyvalue_transfer)
 
                REGISTER_EVENT(enum_xevent_category_db | enum_xdbevent_code_transfer | enum_xdbkey_type_transaction ,on_tx_transfer)
            END_DECLARE_EVENT_HANDLER()
        };

    }//end of namespace of base
}//end of namespace top
