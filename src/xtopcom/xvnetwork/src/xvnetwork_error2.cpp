// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvnetwork/xvnetwork_error2.h"
#include <string>

NS_BEG2(top, vnetwork)

static std::string
xvnetwork_errc_map(int const errc) noexcept {
    auto const ec = static_cast<xvnetwork_errc2_t>(errc);
    switch (ec) {
    case xvnetwork_errc2_t::success:
        return u8"success";
    case xvnetwork_errc2_t::vhost_not_run:
        return u8"vhost not run";
    case xvnetwork_errc2_t::cluster_address_not_match:
        return u8"cluster address not match";
    case xvnetwork_errc2_t::invalid_src_address:
        return u8"invalid src address";
    case xvnetwork_errc2_t::invalid_dst_address:
        return u8"invalid dst address";
    case xvnetwork_errc2_t::version_mismatch:
        return u8"version mismatch";
    case xvnetwork_errc2_t::not_supported:
        return u8"not supported";
    default:
        return u8"unknown error";
    }
};

class xtop_vnetwork_category2 final : public std::error_category {
public:
    const char *
    name() const noexcept override {
        return u8"[vnetwork]";
    }

    std::string
    message(int errc) const override {
        return xvnetwork_errc_map(errc);
    }
};
using xvnetwork_category2_t = xtop_vnetwork_category2;

std::error_code
make_error_code(xvnetwork_errc2_t const errc) {
    return std::error_code{static_cast<int>(errc), vnetwork_category2()};
}

std::error_condition
make_error_condition(xvnetwork_errc2_t const errc) {
    return std::error_condition{static_cast<int>(errc), vnetwork_category2()};
}

std::error_category const &
vnetwork_category2() {
    static xvnetwork_category2_t category{};
    return category;
}

NS_END2
