// Snap Websites Server -- form handling
// Copyright (C) 2011-2013  Made to Order Software Corp.
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
#ifndef SNAP_FROM_H
#define SNAP_FROM_H

#include "../sessions/sessions.h"
#include "../filter/filter.h"
#include <QtCassandra/QCassandraTable.h>
#include <QDomDocument>

namespace snap
{
namespace form
{

enum name_t {
    SNAP_NAME_FORM_FORM,
    SNAP_NAME_FORM_PATH,
    SNAP_NAME_FORM_RESOURCE,
    SNAP_NAME_FORM_SETTINGS,
    SNAP_NAME_FORM_SOURCE
};
char const *get_name(name_t const name) __attribute__ ((const));


class form_exception : public snap_exception
{
public:
    form_exception(char const *what_msg) : snap_exception("Form: " + std::string(what_msg)) {}
    form_exception(std::string const& what_msg) : snap_exception("Form: " + what_msg) {}
    form_exception(QString const& what_msg) : snap_exception("Form: " + what_msg.toStdString()) {}
};

class form_exception_invalid_form_xml : public form_exception
{
public:
    form_exception_invalid_form_xml(char const *what_msg) : form_exception(what_msg) {}
    form_exception_invalid_form_xml(std::string const& what_msg) : form_exception(what_msg) {}
    form_exception_invalid_form_xml(QString const& what_msg) : form_exception(what_msg.toStdString()) {}
};


class form_post
{
public:
    virtual ~form_post() {}

    virtual QDomDocument    on_get_xml_form(QString const& cpath) = 0;
    virtual void            on_process_post(QString const& cpath, sessions::sessions::session_info const& info) = 0;
};


class form : public plugins::plugin
{
public:
                            form();
                            ~form();

    static form *           instance();
    virtual QString         description() const;
    QSharedPointer<QtCassandra::QCassandraTable> get_form_table();

    void                    on_bootstrap(::snap::snap_child *snap);
    void                    on_init();
    void                    on_process_post(QString const& uri_path);
    void                    on_replace_token(filter::filter *f, QString const& cpath, QDomDocument& xml, filter::filter::token_info_t& token);

    SNAP_SIGNAL(tweak_form, (form *f, QString const& cpath, QDomDocument form_doc), (f, cpath, form_doc));
    SNAP_SIGNAL(form_element, (form *f), (f));
    SNAP_SIGNAL(validate_post_for_widget, (QString const& cpath, sessions::sessions::session_info& info, QDomElement const& widget, QString const& widget_name, QString const& widget_type, bool is_secret), (cpath, info, widget, widget_name, widget_type, is_secret));

    QDomDocument const      load_form(QString const& cpath, QString const& source, QString& error);
    QDomDocument            form_to_html(sessions::sessions::session_info& info, QDomDocument const& xml);
    void                    add_form_elements(QDomDocument& add);
    void                    add_form_elements(QString& filename);

    QString                 get_form_title(QString const& default_title) const;

    static QString          text_64max(QString const& text, bool is_secret);
    static QString          html_64max(QString const& html, bool is_secret);
    static int              count_text_lines(QString const& text);
    static int              count_html_lines(QString const& html);
    static bool             parse_width_height(QString const& size, int& width, int& height);

private:
    QString                     get_source(QString const& owner, QString const& cpath);

    zpsnap_child_t              f_snap;
    controlled_vars::fbool_t    f_form_initialized;
    QDomDocument                f_form_elements;
    QDomElement                 f_form_stylesheet;
    QString                     f_form_elements_string;
    QString                     f_form_title;
};

} // namespace form
} // namespace snap
#endif
// SNAP_FORM_H
// vim: ts=4 sw=4 et
