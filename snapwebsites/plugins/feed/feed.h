// Snap Websites Server -- feed management (RSS like feeds and aggregators)
// Copyright (C) 2013-2015  Made to Order Software Corp.
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

#include "../content/content.h"

namespace snap
{
namespace feed
{


enum name_t
{
    SNAP_NAME_FEED_ADMIN_SETTINGS,
    SNAP_NAME_FEED_AGE,
    SNAP_NAME_FEED_ATTACHMENT_TYPE,
    SNAP_NAME_FEED_DESCRIPTION,
    SNAP_NAME_FEED_EXTENSION,
    SNAP_NAME_FEED_MIMETYPE,
    SNAP_NAME_FEED_PAGE_LAYOUT,
    SNAP_NAME_FEED_TITLE,
    SNAP_NAME_FEED_TTL,
    SNAP_NAME_FEED_TYPE
};
char const *get_name(name_t name) __attribute__ ((const));


class feed_exception : public snap_exception
{
public:
    feed_exception(char const *        what_msg) : snap_exception("Feed", what_msg) {}
    feed_exception(std::string const & what_msg) : snap_exception("Feed", what_msg) {}
    feed_exception(QString const &     what_msg) : snap_exception("Feed", what_msg) {}
};



class feed : public plugins::plugin
{
public:
                        feed();
                        ~feed();

    static feed *       instance();
    virtual QString     description() const;
    virtual int64_t     do_update(int64_t last_updated);

    void                on_bootstrap(snap_child *snap);
    void                on_backend_process();
    void                on_generate_page_content(content::path_info_t& ipath, QDomElement& page, QDomElement& body, QString const& ctemplate);

private:
    void                content_update(int64_t variables_timestamp);
    void                generate_feeds();

    zpsnap_child_t      f_snap;
    QString             f_feed_parser_xsl;
};


} // namespace feed
} // namespace snap
// vim: ts=4 sw=4 et
