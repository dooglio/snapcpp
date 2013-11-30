// Snap Websites Server -- queue emails for the backend to send
// Copyright (C) 2013  Made to Order Software Corp.
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
#ifndef SNAP_SENDMAIL_H
#define SNAP_SENDMAIL_H

#include "snapwebsites.h"
#include "plugins.h"
#include "snap_child.h"
#include "../layout/layout.h"
#include "../filter/filter.h"
#include <libtld/tld.h>
#include <QtSerialization/QSerializationReader.h>
#include <controlled_vars/controlled_vars_need_init.h>
#include <QMap>
#include <QVector>
#include <QByteArray>

namespace snap
{
namespace sendmail
{

class sendmail_exception : public snap_exception
{
public:
    sendmail_exception(const std::string& what_msg) : snap_exception(what_msg) {}
};

class sendmail_exception_no_magic : public sendmail_exception
{
public:
    sendmail_exception_no_magic(const std::string& what_msg) : sendmail_exception(what_msg) {}
};

class sendmail_exception_invalid_argument : public sendmail_exception
{
public:
    sendmail_exception_invalid_argument(const std::string& what_msg) : sendmail_exception(what_msg) {}
};


enum name_t
{
    SNAP_NAME_SENDMAIL,
    SNAP_NAME_SENDMAIL_CONTENT_TYPE,
    SNAP_NAME_SENDMAIL_EMAIL,
    SNAP_NAME_SENDMAIL_EMAILS_TABLE,
    SNAP_NAME_SENDMAIL_FREQUENCY,
    SNAP_NAME_SENDMAIL_FREQUENCY_DAILY,
    SNAP_NAME_SENDMAIL_FREQUENCY_IMMEDIATE,
    SNAP_NAME_SENDMAIL_FREQUENCY_MONTHLY,
    SNAP_NAME_SENDMAIL_FREQUENCY_WEEKLY,
    SNAP_NAME_SENDMAIL_FROM,
    SNAP_NAME_SENDMAIL_IMPORTANT,
    SNAP_NAME_SENDMAIL_INDEX,
    SNAP_NAME_SENDMAIL_LISTS,
    SNAP_NAME_SENDMAIL_NEW,
    SNAP_NAME_SENDMAIL_PING,
    SNAP_NAME_SENDMAIL_SENDING_STATUS,
    SNAP_NAME_SENDMAIL_STATUS,
    SNAP_NAME_SENDMAIL_STATUS_DELETED,
    SNAP_NAME_SENDMAIL_STATUS_LOADING,
    SNAP_NAME_SENDMAIL_STATUS_NEW,
    SNAP_NAME_SENDMAIL_STATUS_READ,
    SNAP_NAME_SENDMAIL_STATUS_SENDING,
    SNAP_NAME_SENDMAIL_STATUS_SENT,
    SNAP_NAME_SENDMAIL_STATUS_SPAM,
    SNAP_NAME_SENDMAIL_STOP,
    SNAP_NAME_SENDMAIL_SUBJECT,
    SNAP_NAME_SENDMAIL_TO,
    SNAP_NAME_SENDMAIL_X_MSMAIL_PRIORITY,
    SNAP_NAME_SENDMAIL_X_PRIORITY
};
const char *get_name(name_t name);


class sendmail : public plugins::plugin, public snap::server::backend_action, public layout::layout_content
{
public:
    class email : public QtSerialization::QSerializationObject
    {
    public:
        static const int EMAIL_MAJOR_VERSION = 1;
        static const int EMAIL_MINOR_VERSION = 0;

        typedef QMap<QString, QString> header_map_t;

        enum email_priority_t
        {
            EMAIL_PRIORITY_BULK = 1,
            EMAIL_PRIORITY_LOW,
            EMAIL_PRIORITY_NORMAL,
            EMAIL_PRIORITY_HIGH,
            EMAIL_PRIORITY_URGENT
        };

        class email_attachment : public QtSerialization::QSerializationObject
        {
        public:
            email_attachment();
            virtual ~email_attachment();

            void set_data(const QByteArray& data, QString mime_type);
            QByteArray get_data() const;
            void add_header(const QString& name, const QString& value);
            QString get_header(const QString& name) const;
            const header_map_t& get_all_headers() const;

            // internal functions used to save the data serialized
            void unserialize(QtSerialization::QReader& r);
            virtual void readTag(const QString& name, QtSerialization::QReader& r);
            void serialize(QtSerialization::QWriter& w) const;

        private:
            header_map_t        f_header;
            QByteArray          f_data;
        };
        typedef QVector<email_attachment> attachment_vector_t;

        email();
        virtual ~email();

        void set_from(const QString& from);
        void set_cumulative(const QString& object);
        void set_site_key(const QString& site_key);
        const QString& get_site_key() const;
        void set_email_path(const QString& email_path);
        const QString& get_email_path() const;
        void set_email_key(const QString& site_key);
        const QString& get_email_key() const;
        time_t get_time() const;
        void set_priority(email_priority_t priority = EMAIL_PRIORITY_NORMAL);
        void set_subject(const QString& subject);
        void add_header(const QString& name, const QString& value);
        QString get_header(const QString& name) const;
        const header_map_t& get_all_headers() const;
        void set_body_attachment(const email_attachment& data);
        void add_attachment(const email_attachment& data);
        int get_attachment_count() const;
        email_attachment& get_attachment(int index) const;
        void add_parameter(const QString& name, const QString& value);
        QString get_parameter(const QString& name) const;
        const header_map_t& get_all_parameters() const;

        // internal functions used to save the data serialized
        void unserialize(const QString& data);
        virtual void readTag(const QString& name, QtSerialization::QReader& r);
        QString serialize() const;

    private:
        QString                     f_cumulative;
        QString                     f_site_key;
        QString                     f_email_path;
        QString                     f_email_key; // set on post_email()
        controlled_vars::mint64_t   f_time;
        header_map_t                f_header;
        attachment_vector_t         f_attachment;
        header_map_t                f_parameter;
    };

    sendmail();
    ~sendmail();

    static sendmail *   instance();
    virtual QString     description() const;
    virtual int64_t     do_update(int64_t last_updated);
    QSharedPointer<QtCassandra::QCassandraTable> get_emails_table();

    void                on_bootstrap(snap_child *snap);
    void                on_register_backend_action(snap::server::backend_action_map_t& actions);
    virtual void        on_backend_action(const QString& action);
	virtual void		on_generate_main_content(layout::layout *l, const QString& path, QDomElement& page, QDomElement& body);
	//void				on_generate_page_content(layout::layout *l, const QString& path, QDomElement& page, QDomElement& body);
    void                on_replace_token(filter::filter *f, QDomDocument& xml, filter::filter::token_info_t& token);

    void                post_email(const email& e);

    SNAP_SIGNAL(filter_email, (email& e), (e));

private:
    void initial_update(int64_t variables_timestamp);
    void content_update(int64_t variables_timestamp);
    void process_emails();
    void attach_email(const email& e);
    void attach_user_email(const email& e);
    void run_emails();
    void sendemail(const QString& key, const QString& unique_key);

    zpsnap_child_t      f_snap;
    email               f_email; // email being processed
};

} // namespace sendmail
} // namespace snap
#endif
// SNAP_SENDMAIL_H
// vim: ts=4 sw=4 et
