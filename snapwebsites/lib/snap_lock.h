// Snap Lock -- class used to have inter-process locks in a Snap! cluster
// Copyright (C) 2016  Made to Order Software Corp.
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

#include "snap_communicator.h"

namespace snap
{

class snap_lock_exception : public snap_exception
{
public:
    explicit snap_lock_exception(char const *        what_msg) : snap_exception("snap_lock", what_msg) {}
    explicit snap_lock_exception(std::string const & what_msg) : snap_exception("snap_lock", what_msg) {}
    explicit snap_lock_exception(QString const &     what_msg) : snap_exception("snap_lock", what_msg) {}
};

class snap_lock_failed_exception : public snap_lock_exception
{
public:
    explicit snap_lock_failed_exception(char const *        what_msg) : snap_lock_exception(what_msg) {}
    explicit snap_lock_failed_exception(std::string const & what_msg) : snap_lock_exception(what_msg) {}
    explicit snap_lock_failed_exception(QString const &     what_msg) : snap_lock_exception(what_msg) {}
};


// the lock internal implementation
class lock_connection;


class snap_lock
{
public:
    typedef std::shared_ptr<snap_lock>      pointer_t;

    static int const    SNAP_LOCK_DEFAULT_TIMEOUT = 5; // in seconds

                        snap_lock(QString const & object_name, int timeout = -1);

    static void         initialize_timeout(int timeout);
    static int          current_timeout();
    static void         initialize_snapcommunicator(
                              std::string const & addr
                            , int port
                            , tcp_client_server::bio_client::mode_t mode = tcp_client_server::bio_client::mode_t::MODE_PLAIN);

    void                unlock();
    time_t              get_timeout_date() const;

private:
    std::shared_ptr<lock_connection>    f_lock_connection;
};




} // namespace snap
// vim: ts=4 sw=4 et
