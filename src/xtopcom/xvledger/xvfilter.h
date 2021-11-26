// Copyright (c) 2018-Present Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>
#include <vector>
#include "xbase/xobject.h"

namespace top
{
    namespace base
    {
        enum enum_xfilter_handle_code
        {
            enum_xfilter_handle_code_bad_type       = -4, //invalid event types
            enum_xfilter_handle_code_closed         = -3, //filter has been closed
            enum_xfilter_handle_code_interrupt      = -2, //stop pass and handle event
            enum_xfilter_handle_code_error          = -1, //unknow error
            enum_xfilter_handle_code_success        = 0,  //success handle one
            enum_xfilter_handle_code_finish         = 1,  //finish everything and may
        };

        //filter-chain  [filter]<->[filter]<->[filter]<->[filter]<->[filter]
        //event-flow:   push_event_back->[filter]->[filter]->[filter]
        //event-flow:   [filter]<-[filter]<-[filter]<-push_event_front
        class xvfilter_t : public xobject_t
        {
        protected:
            typedef std::function< enum_xfilter_handle_code (const xvevent_t & event,xvfilter_t* last_filter) > xevent_handler;
            enum
            {
                enum_max_event_ids_count   = 64,
            };
        protected:
            xvfilter_t(xvfilter_t * front_filter);
            xvfilter_t(xvfilter_t * front_filter,xvfilter_t * back_filter);
            virtual ~xvfilter_t();
        private:
            xvfilter_t();
            xvfilter_t(const xvfilter_t &);
            xvfilter_t & operator = (const xvfilter_t &);
            
        public:
            //throw event from prev front to back
            enum_xfilter_handle_code    push_event_back(const xvevent_t & event,xvfilter_t* last_filter);
            //push event from back to front
            enum_xfilter_handle_code    push_event_front(const xvevent_t & event,xvfilter_t* last_filter);
            
            bool            reset_front_filter(xvfilter_t * front_filter);
            bool            reset_back_filter(xvfilter_t * front_filter);
            virtual bool    close(bool force_async = false) override; //must call close before release
            
        protected:
            xvfilter_t*     get_front() const {return m_front_filter;}
            xvfilter_t*     get_back()  const {return m_back_filter;}
            
            bool            register_event(const uint16_t full_event_type,const xevent_handler & api_function);
            inline xevent_handler**get_event_handlers() {return m_event_handlers;}
            
        private: //triggered by push_event_back or push_event_front
            virtual enum_xfilter_handle_code fire_event(const xvevent_t & event,xvfilter_t* last_filter);
            
        private:
            xvfilter_t *    m_front_filter;
            xvfilter_t *    m_back_filter;
            xevent_handler* m_event_handlers[enum_max_event_ids_count];
        };
    
        //convenient macro to register register_event
        #define BEGIN_DECLARE_EVENT_HANDLER() template<typename _T> void register_event_internal(_T * pThis){
            #define REGISTER_EVENT(event_id,entry) register_event((const uint8_t)event_id,std::bind(&_T::entry,pThis,std::placeholders::_1,std::placeholders::_2));
        #define END_DECLARE_EVENT_HANDLER() }
    
        #define INIT_EVENTS_HANDLER() register_event_internal(this)
    
        //[8bit:category][4bit:code][4bit:class]
        //Event_ID = [4bit:code][4bit:class]
        enum enum_xdbevent_def
        {
            //top level: category
            enum_xdbevent_category              = 0x0100, //DB
            
            //enum_xdbevent_code * enum_xdbevent_class < max enum_max_event_codes_count
            //first level: code
            enum_xdbevent_code_transfer         = 0x0010, //transfer src DB to dest DB

            enum_xdbevent_code_max              = 0x0040, //not over this max value
            
            //second level: class
            enum_xdbevent_class_unknown         = 0x0000, //unknow class
            enum_xdbevent_class_key_value       = 0x0001, //key-value
            enum_xdbevent_class_block_index     = 0x0002, //block index
            enum_xdbevent_class_block_object    = 0x0003, //block object
            enum_xdbevent_class_state_object    = 0x0004, //state object
            enum_xdbevent_class_account_meta    = 0x0005, //account meta
            enum_xdbevent_class_account_span    = 0x0006, //account span
            enum_xdbevent_class_transaction     = 0x0007, //txs
            
            enum_xdbevent_class_max             = 0x000F, //not over this max value
        };
        class xvdbstore_t; //forward delcare
        class xdbevent_t : public xvevent_t
        {
        public:
            xdbevent_t(const int _event_type,const std::string & db_key,const std::string & db_value,enum_xdbkey_type db_key_type,xvdbstore_t* db_store_ptr);
            virtual ~xdbevent_t();
        private:
            xdbevent_t();
            xdbevent_t(xdbevent_t &&);
            xdbevent_t(const xdbevent_t & obj);
            xdbevent_t& operator = (const xdbevent_t & obj);
            
        public://Event_ID = [4bit:code][4bit:class]
            static const int get_event_class(const int full_type)    {return (full_type & 0x0F);} //4bit
            static const int get_event_code(const int full_type)     {return ((full_type >> 4) & 0x0F);} //4bit
        public:
            const int        get_event_class()    const {return ( get_type() & 0x0F);} //4bit
            const int        get_event_code()     const {return ((get_type() >> 4) & 0x0F);} //4bit
            
            inline const std::string &  get_db_key()   const {return m_db_key;}
            inline const std::string &  get_db_value() const {return m_db_value;}
            inline enum_xdbkey_type     get_db_key_type() const{return m_db_key_type;}
            inline xvdbstore_t*         get_db_store() const {return m_db_store;}
        private:
            std::string         m_db_key;
            std::string         m_db_value;
            enum_xdbkey_type    m_db_key_type;
            xvdbstore_t*        m_db_store; //note:just copy ptr without reference control
        };
    
        class xdbfilter_t : public xvfilter_t
        {
        protected:
            xdbfilter_t(xdbfilter_t * front_filter);
            xdbfilter_t(xdbfilter_t * front_filter,xdbfilter_t * back_filter);
            virtual ~xdbfilter_t();
        private:
            xdbfilter_t();
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
            xkeyvfilter_t(xdbfilter_t * front_filter);
            xkeyvfilter_t(xdbfilter_t * front_filter,xdbfilter_t * back_filter);
            virtual ~xkeyvfilter_t();
        private:
            xkeyvfilter_t();
            xkeyvfilter_t(xkeyvfilter_t &&);
            xkeyvfilter_t(const xkeyvfilter_t &);
            xkeyvfilter_t & operator = (const xkeyvfilter_t &);
            
        protected:
            virtual enum_xfilter_handle_code on_keyvalue_transfer(const xvevent_t & event,xvfilter_t* last_filter);
            
        private:
            BEGIN_DECLARE_EVENT_HANDLER()
                REGISTER_EVENT(enum_xdbevent_category | enum_xdbevent_class_key_value | enum_xdbevent_code_transfer,on_keyvalue_transfer)
            END_DECLARE_EVENT_HANDLER()
        };
    
        //handle block
        class xbksfilter_t : public xdbfilter_t
        {
        protected:
            xbksfilter_t(xdbfilter_t * front_filter);
            xbksfilter_t(xdbfilter_t * front_filter,xdbfilter_t * back_filter);
            virtual ~xbksfilter_t();
        private:
            xbksfilter_t();
            xbksfilter_t(xbksfilter_t &&);
            xbksfilter_t(const xbksfilter_t &);
            xbksfilter_t & operator = (const xbksfilter_t &);
            
        protected:
            virtual enum_xfilter_handle_code on_keyvalue_transfer(const xvevent_t & event,xvfilter_t* last_filter);
            virtual enum_xfilter_handle_code on_block_index_transfer(const xvevent_t & event,xvfilter_t* last_filter);
            virtual enum_xfilter_handle_code on_block_object_transfer(const xvevent_t & event,xvfilter_t* last_filter);
            
        private:
            BEGIN_DECLARE_EVENT_HANDLER()
                REGISTER_EVENT(enum_xdbevent_category | enum_xdbevent_class_key_value | enum_xdbevent_code_transfer,on_keyvalue_transfer)
            
                REGISTER_EVENT(enum_xdbevent_category | enum_xdbevent_class_block_index | enum_xdbevent_code_transfer,on_block_index_transfer)
 
                REGISTER_EVENT(enum_xdbevent_category | enum_xdbevent_class_block_object | enum_xdbevent_code_transfer,on_block_object_transfer)
            END_DECLARE_EVENT_HANDLER()
        };
    
        //generaly handle any transaction
        class xtxsfilter_t : public xdbfilter_t
        {
        protected:
            xtxsfilter_t(xdbfilter_t * front_filter);
            xtxsfilter_t(xdbfilter_t * front_filter,xdbfilter_t * back_filter);
            virtual ~xtxsfilter_t();
        private:
            xtxsfilter_t();
            xtxsfilter_t(xtxsfilter_t &&);
            xtxsfilter_t(const xtxsfilter_t &);
            xtxsfilter_t & operator = (const xtxsfilter_t &);
            
        protected:
            virtual enum_xfilter_handle_code on_keyvalue_transfer(const xvevent_t & event,xvfilter_t* last_filter);
            virtual enum_xfilter_handle_code on_tx_transfer(const xvevent_t & event,xvfilter_t* last_filter);
            
        private:
            BEGIN_DECLARE_EVENT_HANDLER()
                REGISTER_EVENT(enum_xdbevent_category | enum_xdbevent_class_key_value | enum_xdbevent_code_transfer,on_keyvalue_transfer)
 
                REGISTER_EVENT(enum_xdbevent_category | enum_xdbevent_class_transaction | enum_xdbevent_code_transfer,on_tx_transfer)
            END_DECLARE_EVENT_HANDLER()
        };

    }//end of namespace of base
}//end of namespace top
