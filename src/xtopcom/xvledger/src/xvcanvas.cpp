// Copyright (c) 2018-2020 Telos Foundation & contributors
// taylor.wei@topnetwork.org
// Licensed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <cinttypes>
#include "xbase/xutl.h"
#include "xbase/xcontext.h"
#include "../xvcanvas.h"
 
namespace top
{
    namespace base
    {
        const int  xvcanvas_t::compile(const std::deque<xvmethod_t> & input_records,const int compile_options,xstream_t & output_stream)
        {
            output_stream << (int32_t)input_records.size();
            if(enum_compile_optimization_none == (compile_options & enum_compile_optimization_mask) ) //dont ask any optimization
            {
                for(auto & record : input_records)
                {
                    const std::string & full_path = record.get_method_uri();
                    xassert(full_path.empty() == false);
                    record.serialize_to(output_stream);
                }
            }
            else //ask some optimization
            {
                //split every method to [base_path]/[unitname]
                std::string last_base_path; //init as empty
                std::string last_unit_name; //init as empty
                for(auto & record : input_records)
                {
                    const std::string & full_path = record.get_method_uri();
                    const std::string::size_type lastdot = full_path.find_last_of('/');
                    if(lastdot != std::string::npos) //found
                    {
                        const std::string cur_base_path = full_path.substr(0,lastdot);//[)
                        const std::string cur_unit_name = full_path.substr(lastdot + 1); //skip dot and keep left part
                        xassert(cur_base_path.empty() == false);
                        xassert(cur_unit_name.empty() == false);
                        if( last_base_path.empty() || (last_base_path != cur_base_path) ) //new base path
                        {
                            record.serialize_to(output_stream);
                            last_base_path = cur_base_path; //assign to current base
                            last_unit_name = cur_unit_name; //assign to current unit
                        }
                        else if(last_unit_name.empty() || (last_unit_name != cur_unit_name) )
                        {
                            const std::string compressed_uri = "/" + cur_unit_name;//remove duplicated base path to save space
                            xvmethod_t clone_record(record,compressed_uri);
                            clone_record.serialize_to(output_stream);
                            
                            last_base_path = cur_base_path; //assign to current base
                            last_unit_name = cur_unit_name; //assign to current unit
                        }
                        else //same base_path and unit name as last instruction
                        {
                            std::string empty_uri; //so use empty uri to save space
                            xvmethod_t clone_record(record,empty_uri);
                            clone_record.serialize_to(output_stream);
                        }
                    }
                    else //found root record
                    {
                        last_base_path.clear();//clear first
                        last_unit_name.clear();//clear first
                        last_base_path = full_path;
                        record.serialize_to(output_stream);
                    }
                }
            }
            return enum_xcode_successful;
        }
        
        const int  xvcanvas_t::decompile(xstream_t & input_stream,std::deque<xvmethod_t> & output_records)
        {
            //split every method to [base_path].[unitname]
            std::string last_base_path; //init as empty
            std::string last_unit_name; //init as empty
            output_records.clear(); //clear first
            
            int32_t total_records = 0;
            input_stream >> total_records;
            for(int32_t i = 0; i < total_records; ++i)
            {
                xvmethod_t record;
                if(record.serialize_from(input_stream) > 0)
                {
                    const std::string & org_full_path = record.get_method_uri();
                    if(org_full_path.empty()) //use last base path and unit name
                    {
                        const std::string cur_base_path = last_base_path;
                        const std::string cur_unit_name = last_unit_name;
                        if(cur_base_path.empty() || cur_unit_name.empty())
                        {
                            xassert(0);
                            output_records.clear();
                            return enum_xerror_code_bad_data;
                        }
                        const std::string restored_full_uri = cur_base_path + "/" + cur_unit_name;
                        xvmethod_t clone_record(record,restored_full_uri);
                        output_records.emplace_back(clone_record);
                    }
                    else
                    {
                        const std::string::size_type lastdot = org_full_path.find_last_of('/');
                        if(lastdot != std::string::npos) //found
                        {
                            std::string cur_base_path = org_full_path.substr(0,lastdot);//[)
                            std::string cur_unit_name = org_full_path.substr(lastdot + 1); //skip dot and left part
                            
                            if(cur_base_path.empty())
                                cur_base_path = last_base_path;
                            if(cur_unit_name.empty())
                                cur_unit_name = last_unit_name;
                            
                            if(cur_base_path.empty() || cur_unit_name.empty())
                            {
                                xassert(0);
                                output_records.clear();
                                return enum_xerror_code_bad_data;
                            }
                            const std::string restored_full_uri = cur_base_path + "/" + cur_unit_name;
                            if(restored_full_uri != org_full_path) //it is a compressed record
                            {
                                xvmethod_t clone_record(record,restored_full_uri);
                                output_records.emplace_back(clone_record);
                            }
                            else //it is a full record
                            {
                                output_records.emplace_back(record);
                            }
                            last_base_path = cur_base_path;
                            last_unit_name = cur_unit_name;
                        }
                        else //found root record
                        {
                            output_records.emplace_back(record);
                            last_base_path = org_full_path;
                            last_unit_name.clear();
                        }
                    }
                }
                else
                {
                    xassert(0);
                    output_records.clear();
                    return enum_xerror_code_bad_data;
                }
            }
            return enum_xcode_successful;
        }
        
        //raw_instruction code -> optimization -> original_length -> compressed
        const int  xvcanvas_t::encode_to(const std::deque<xvmethod_t> & input_records,const int encode_options,xstream_t & output_bin)
        {
            try{
                
                base::xautostream_t<1024> _raw_stream(xcontext_t::instance());
                compile(input_records,encode_options,_raw_stream);
                return xstream_t::compress_to_stream(_raw_stream, _raw_stream.size(),output_bin);
                
            } catch (int error_code){
                xerror("xvcanvas_t::encode_to,throw exception with error:%d",error_code);
                return enum_xerror_code_errno;
            }
            xerror("xvcanvas_t::encode_to,throw unknow exception");
            return enum_xerror_code_fail;
        }
        
        const int  xvcanvas_t::encode_to(const std::deque<xvmethod_t> & input_records,const int encode_options,std::string & output_bin)
        {
            try{
                
                base::xautostream_t<1024> _raw_stream(xcontext_t::instance());
                compile(input_records,encode_options,_raw_stream);
                return xstream_t::compress_to_string(_raw_stream,_raw_stream.size(),output_bin);
                
            } catch (int error_code){
                xerror("xvcanvas_t::encode_to,throw exception with error:%d",error_code);
                return enum_xerror_code_errno;
            }
            xerror("xvcanvas_t::encode_to,throw unknow exception");
            return enum_xerror_code_fail;
        }
        
        const int  xvcanvas_t::decode_from(xstream_t & input_bin,const uint32_t bin_size,std::deque<xvmethod_t> & output_records)
        {
            if( (input_bin.size() == 0) || (bin_size == 0) || ((uint32_t)input_bin.size() < bin_size) )
            {
                xerror("xvcanvas_t::decode_from,invalid stream of size(%d) vs bin_log_size(%u)",(int)input_bin.size(),bin_size);
                return enum_xerror_code_no_data;
            }
            
            try{

                xautostream_t<1024> uncompressed_stream(xcontext_t::instance()); //1K is big enough for most packet
                const int decompress_result = xstream_t::decompress_from_stream(input_bin,bin_size,uncompressed_stream);
                if(decompress_result > 0)
                    return decompile(uncompressed_stream,output_records);
                
                xerror("xvcanvas_t::decode_from,decompress_from_stream failed as err(%d)",decompress_result);
                return decompress_result;
                
            } catch (int error_code){
                xerror("xvcanvas_t::decode_from,throw exception with error:%d",error_code);
                return enum_xerror_code_errno;
            }
            xerror("xvcanvas_t::decode_from,throw unknow exception");
            return enum_xerror_code_fail;
        }
        
        const int  xvcanvas_t::decode_from(const std::string & input_bin,std::deque<xvmethod_t> & output_records)
        {
            if(input_bin.empty())
            {
                xerror("xvcanvas_t::decode_from,empty input_bin");
                return enum_xerror_code_no_data;
            }
            
            try{
                xautostream_t<1024> uncompressed_stream(xcontext_t::instance()); //1K is big enough for most packet
                const int decompress_result = xstream_t::decompress_from_string(input_bin,uncompressed_stream);
                if(decompress_result > 0)
                    return decompile(uncompressed_stream,output_records);
                
                xerror("xvcanvas_t::decode_from,decompress_from_string failed as err(%d)",decompress_result);
                return decompress_result;
                
            } catch (int error_code){
                xerror("xvcanvas_t::decode_from,throw exception with error:%d",error_code);
                return enum_xerror_code_errno;
            }
            xerror("xvcanvas_t::decode_from,throw unknow exception");
            return enum_xerror_code_fail;
        }
        
        xvcanvas_t::xvcanvas_t()
        {
        }
        
        xvcanvas_t::xvcanvas_t(const std::string & bin_log)
        {
            xvcanvas_t::decode_from(bin_log,m_records);
        }
        
        xvcanvas_t::~xvcanvas_t()
        {
            m_lock.lock();
            m_records.clear();
            m_lock.unlock();
        }
        
        bool   xvcanvas_t::record(const xvmethod_t & op) //record instruction
        {
            std::lock_guard<std::recursive_mutex> locker(m_lock);
            m_records.emplace_back(op);
            return true;
        }
    
        const int  xvcanvas_t::encode(xstream_t & output_bin,const int compile_options)
        {
            std::lock_guard<std::recursive_mutex> locker(m_lock);
            return encode_to(m_records,compile_options,output_bin);
        }
    
        const int  xvcanvas_t::encode(std::string & output_bin,const int compile_options)
        {
            std::lock_guard<std::recursive_mutex> locker(m_lock);
            return encode_to(m_records,compile_options,output_bin);
        }
        
    };//end of namespace of base
};//end of namespace of top
