// Copyright (c) 2018-Present Telos Foundation & contributors
// taylor.wei@topnetwork.org
// Licensed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <cinttypes>
#include "xbkstoreutl.h"

namespace top
{
    namespace store
    {
        xblockevent_t::xblockevent_t(enum_blockstore_event type,base::xvbindex_t* index,base::xvactplugin_t* plugin,const base::xblockmeta_t& meta)
        : _meta_info(meta)
        {
            _event_type     = type;
            _target_index   = index;
            _target_plugin  = plugin;
            if(_target_index != NULL)
                _target_index->add_ref();
            
            if(_target_plugin != NULL)
                _target_plugin->add_ref();
        }
        xblockevent_t::xblockevent_t(xblockevent_t && obj)
        :_meta_info(obj._meta_info)
        {
            _event_type     = obj._event_type;
            _target_index   = obj._target_index;
            _target_plugin  = obj._target_plugin;
            obj._target_index   = NULL;
            obj._target_plugin  = NULL;
        }
        xblockevent_t::xblockevent_t(const xblockevent_t & obj)
        :_meta_info(obj._meta_info)
        {
            _event_type     = obj._event_type;
            _target_index   = obj._target_index;
            _target_plugin  = obj._target_plugin;
            if(_target_index != NULL)
                _target_index->add_ref();
            if(_target_plugin != NULL)
                _target_plugin->add_ref();
        }
        xblockevent_t & xblockevent_t::operator = (const xblockevent_t & obj)
        {
            base::xvbindex_t* old_index     = _target_index;
            base::xvactplugin_t* old_plugin = _target_plugin;
            
            _event_type     = obj._event_type;
            _target_index   = obj._target_index;
            _target_plugin  = obj._target_plugin;
            _meta_info      = obj._meta_info;
            if(_target_index != NULL)
                _target_index->add_ref();
            if(_target_plugin != NULL)
                _target_plugin->add_ref();
            
            if(old_index != NULL)
                old_index->release_ref();
            
            if(old_plugin != NULL)
                old_plugin->release_ref();
            
            return *this;
        }
        xblockevent_t::~xblockevent_t()
        {
            if(_target_index != NULL)
                _target_index->release_ref();
            if(_target_plugin != NULL)
                _target_plugin->release_ref();
        }
    }
}
