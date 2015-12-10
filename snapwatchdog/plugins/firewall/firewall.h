// Snap Websites Server -- watchdog apache
// Copyright (C) 2013-2014  Made to Order Software Corp.
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

//#include "../users/users.h"
//#include "../server_access/server_access.h"

namespace snap
{
namespace apache
{

enum class name_t
{
    SNAP_NAME_WATCHDOG_APACHE_NAME
};
char const * get_name(name_t name) __attribute__ ((const));


//class apache_exception : public snap_exception
//{
//public:
//    apache_exception(char const *        what_msg) : snap_exception("apache", what_msg) {}
//    apache_exception(std::string const & what_msg) : snap_exception("apache", what_msg) {}
//    apache_exception(QString const &     what_msg) : snap_exception("apache", what_msg) {}
//};
//
//class apache_exception_invalid_argument : public apache_exception
//{
//public:
//    apache_exception_invalid_argument(char const *        what_msg) : apache_exception(what_msg) {}
//    apache_exception_invalid_argument(std::string const & what_msg) : apache_exception(what_msg) {}
//    apache_exception_invalid_argument(QString const &     what_msg) : apache_exception(what_msg) {}
//};





class apache
        : public plugins::plugin
{
public:
                        apache();
                        ~apache();

    // plugins::plugin implementation
    static apache *     instance();
    virtual QString     description() const;
    virtual QString     dependencies() const;
    virtual int64_t     do_update(int64_t last_updated);
    virtual void        bootstrap(snap_child * snap);

    // server signals
    void                on_process_watch();

private:
    zpsnap_child_t      f_snap;
};

} // namespace apache
} // namespace snap
// vim: ts=4 sw=4 et
