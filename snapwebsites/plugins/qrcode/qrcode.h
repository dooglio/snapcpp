// Snap Websites Server -- QR Code generator
// Copyright (C) 2014-2015  Made to Order Software Corp.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#pragma once

#include "../path/path.h"


namespace snap
{
namespace qrcode
{


enum name_t
{
    SNAP_NAME_QRCODE_DEFAULT_SCALE,
    SNAP_NAME_QRCODE_DEFAULT_EDGE,
    SNAP_NAME_QRCODE_PRIVATE_ENABLE
};
char const *get_name(name_t name) __attribute__ ((const));


class qrcode_exception : public snap_exception
{
public:
    qrcode_exception(char const *        what_msg) : snap_exception("qrcode", what_msg) {}
    qrcode_exception(std::string const & what_msg) : snap_exception("qrcode", what_msg) {}
    qrcode_exception(QString const &     what_msg) : snap_exception("qrcode", what_msg) {}
};







class qrcode : public plugins::plugin
             , public path::path_execute
{
public:
                        qrcode();
                        ~qrcode();

    static qrcode *     instance();
    virtual QString     description() const;
    virtual int64_t     do_update(int64_t last_updated);

    void                on_bootstrap(snap_child *snap);
    virtual bool        on_path_execute(content::path_info_t& ipath);
    void                on_can_handle_dynamic_path(content::path_info_t& ipath, path::dynamic_plugin_t& plugin_info);

private:
    void                content_update(int64_t variables_timestamp);

    zpsnap_child_t      f_snap;
};


} // namespace qrcode
} // namespace snap
// vim: ts=4 sw=4 et
