// Copyright (c) 2017-2021 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xstate/xerror/xerror.h"

namespace top {
namespace state {
namespace error {

static char const * errc_to_message(int const errc) noexcept {
    auto ec = static_cast<error::xerrc_t>(errc);
    switch (ec) {
    case xerrc_t::ok:
        return "successful";

    case xerrc_t::token_insufficient:
        return "token insufficient";

    case xerrc_t::invalid_property_type:
        return "invalid property type";

    case xerrc_t::load_property_failed:
        return "load property fail";

    case xerrc_t::property_access_denied:
        return "property access denied";

    case xerrc_t::property_not_exist:
        return "property not exist";

    case xerrc_t::property_value_out_of_range:
        return "property value out of range";

    case xerrc_t::property_name_too_long:
        return "property name too long";

    case xerrc_t::create_property_failed:
        return "create property failed";

    default:
        return "unknown error";
    }
}

class xtop_state_category : public std::error_category {
public:
    const char * name() const noexcept override {
        return "state";
    }

    std::string message(int errc) const override {
        return errc_to_message(errc);
    }
};
using xstate_category_t = xtop_state_category;

std::error_code make_error_code(xerrc_t errc) noexcept {
    return std::error_code(static_cast<int>(errc), state_category());
}

std::error_condition make_error_condition(xerrc_t errc) noexcept {
    return std::error_condition(static_cast<int>(errc), state_category());
}

std::error_category const & state_category() {
    static xstate_category_t category;
    return category;
}

}
}
}

namespace std {

#if !defined(XCXX14_OR_ABOVE)

size_t hash<top::state::error::xerrc_t>::operator()(top::state::error::xerrc_t errc) const noexcept {
    return static_cast<size_t>(static_cast<std::underlying_type<top::state::error::xerrc_t>::type>(errc));
}

#endif

}
