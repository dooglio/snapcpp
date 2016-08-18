// Snap Websites Server -- snap websites server
// Copyright (C) 2011-2016  Made to Order Software Corp.
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

#include "version.h"
#include "plugins.h"
#include "snap_child.h"
#include "snap_communicator.h"
#include "snap_config.h"
#include "snap_expr.h"
#include "snap_listen_thread.h"

#include <controlled_vars/controlled_vars_limited_need_enum_init.h>

#include <advgetopt/advgetopt.h>

#include <QTranslator>

/** \file
 * \brief Main header file of the libsnapwebsites library.
 *
 * This header file defines the server class which is the main
 * class in the snapserver daemon. It executes the run() loop.
 *
 * See the .cpp file for more details.
 */

namespace snap
{

enum class name_t
{
    // low level names
    SNAP_NAME_SERVER,           // The name of the Snap! Server
    SNAP_NAME_CONTEXT,          // Cassandra Keyspace
    SNAP_NAME_INDEX,            // Row used for the domains & websites index
    SNAP_NAME_DOMAINS,          // Cassandra Table used for domains
    SNAP_NAME_WEBSITES,         // Cassandra Table used for websites
    SNAP_NAME_SITES,            // Cassandra Table used for sites (one site per row)
    SNAP_NAME_BACKEND,          // Cassandra Table used to know where we are with backends
    SNAP_NAME_LOCKTABLE,        // Cassandra Table used to serialize database access

    // names used by core (server & snap_child)
    SNAP_NAME_CORE_ADMINISTRATOR_EMAIL,
    SNAP_NAME_CORE_CONTENT_TYPE_HEADER,
    SNAP_NAME_CORE_COOKIE_DOMAIN,
    SNAP_NAME_CORE_HTTP_ACCEPT_LANGUAGE,
    SNAP_NAME_CORE_HTTP_USER_AGENT,
    SNAP_NAME_CORE_LAST_DYNAMIC_UPDATE,
    SNAP_NAME_CORE_LAST_UPDATED,
    SNAP_NAME_CORE_LOCATION_HEADER,
    SNAP_NAME_CORE_ORIGINAL_RULES,
    SNAP_NAME_CORE_PARAM_DEFAULT_PLUGINS,
    SNAP_NAME_CORE_PARAM_PLUGINS,
    SNAP_NAME_CORE_PARAM_PLUGINS_PATH,
    SNAP_NAME_CORE_PARAM_TABLE_SCHEMA_PATH,
    SNAP_NAME_CORE_PLUGINS,
    SNAP_NAME_CORE_PLUGIN_THRESHOLD,
    SNAP_NAME_CORE_REDIRECT,
    SNAP_NAME_CORE_REMOTE_ADDR,
    SNAP_NAME_CORE_REQUEST_METHOD,
    SNAP_NAME_CORE_REQUEST_URI,
    SNAP_NAME_CORE_RETRY_AFTER_HEADER,
    SNAP_NAME_CORE_RULES,
    SNAP_NAME_CORE_SERVER_PROTOCOL,
    SNAP_NAME_CORE_SITE_LONG_NAME,
    SNAP_NAME_CORE_SITE_NAME,
    SNAP_NAME_CORE_SITE_READY,
    SNAP_NAME_CORE_SITE_SHORT_NAME,
    SNAP_NAME_CORE_SNAPBACKEND,
    SNAP_NAME_CORE_STATUS_HEADER,
    SNAP_NAME_CORE_USER_COOKIE_NAME,
    SNAP_NAME_CORE_X_POWERED_BY_HEADER
};
char const * get_name(name_t name) __attribute__ ((const));



class snapwebsites_exception : public snap_exception
{
public:
    snapwebsites_exception(char const *        whatmsg) : snap_exception("snapwebsites", whatmsg) {}
    snapwebsites_exception(std::string const & whatmsg) : snap_exception("snapwebsites", whatmsg) {}
    snapwebsites_exception(QString const &     whatmsg) : snap_exception("snapwebsites", whatmsg) {}
};

class snapwebsites_exception_invalid_parameters : public snapwebsites_exception
{
public:
    snapwebsites_exception_invalid_parameters(char const *        whatmsg) : snapwebsites_exception(whatmsg) {}
    snapwebsites_exception_invalid_parameters(std::string const & whatmsg) : snapwebsites_exception(whatmsg) {}
    snapwebsites_exception_invalid_parameters(QString const &     whatmsg) : snapwebsites_exception(whatmsg) {}
};

class snapwebsites_exception_parameter_no_available : public snapwebsites_exception
{
public:
    snapwebsites_exception_parameter_no_available(char const *        whatmsg) : snapwebsites_exception(whatmsg) {}
    snapwebsites_exception_parameter_no_available(std::string const & whatmsg) : snapwebsites_exception(whatmsg) {}
    snapwebsites_exception_parameter_no_available(QString const &     whatmsg) : snapwebsites_exception(whatmsg) {}
};

class snapwebsites_exception_io_error : public snapwebsites_exception
{
public:
    snapwebsites_exception_io_error(char const *        whatmsg) : snapwebsites_exception(whatmsg) {}
    snapwebsites_exception_io_error(std::string const & whatmsg) : snapwebsites_exception(whatmsg) {}
    snapwebsites_exception_io_error(QString const &     whatmsg) : snapwebsites_exception(whatmsg) {}
};




class permission_error_callback
{
public:
    class error_by_mime_type
    {
    public:
        virtual                 ~error_by_mime_type() {}

        virtual void            on_handle_error_by_mime_type(snap_child::http_code_t err_code, QString const & err_name, QString const & err_description, QString const & path) = 0;
    };

    virtual                 ~permission_error_callback() {}

    virtual void            on_error(snap_child::http_code_t const err_code, QString const & err_name, QString const & err_description, QString const & err_details, bool const err_by_mime_type) = 0;
    virtual void            on_redirect(QString const & err_name, QString const & err_description, QString const & err_details, bool err_security, QString const & path, snap_child::http_code_t const http_code) = 0;
};

// a simple specialization of the permission_error_callback that quiet
// the errors so they don't get in the way (quiet as in: the end users don't
// see them; it's going to be logged anyway)
class quiet_error_callback
        : public permission_error_callback
{
public:
                    quiet_error_callback(snap_child * snap, bool log);

    virtual void    on_error(snap_child::http_code_t const err_code, QString const & err_name, QString const& err_description, QString const & err_details, bool const err_by_mime_type);
    virtual void    on_redirect(QString const & err_name, QString const & err_description, QString const & err_details, bool err_security, QString const & path, snap_child::http_code_t const http_code);

    void            clear_error();
    bool            has_error() const;

private:
    zpsnap_child_t              f_snap;
    controlled_vars::mlbool_t   f_log;
    controlled_vars::flbool_t   f_error;
};


// implementation specific class
class listener_impl;


class server
        : public plugins::plugin
{
public:
    typedef std::shared_ptr<server>             pointer_t;

    // TODO: remove once snapcommunicator is used
    typedef QSharedPointer<udp_client_server::udp_server>   udp_server_t;

    typedef uint32_t                            config_flags_t;

    static config_flags_t const                 SNAP_SERVER_CONFIG_OPTIONAL_SERVER_NAME = 0x01;

    class backend_action
    {
    public:
        virtual                 ~backend_action() {}
        virtual void            on_backend_action(QString const & action) = 0;
    };

    class backend_action_set
    {
    public:
        void                    add_action(QString const & action, plugins::plugin * p);
        bool                    has_action(QString const & action) const;
        void                    execute_action(QString const & action);
        QString                 get_plugin_name(QString const & action);
        void                    display();

    private:
        typedef QMap<QString, backend_action *> actions_map_t;
        actions_map_t           f_actions;
    };

    class accessible_flag_t
    {
    public:
                        accessible_flag_t()
                            //: f_accessible(false) -- auto-init
                            //, f_secure(false) -- auto-init
                        {
                        }

        bool            is_accessible() const { return f_accessible && !f_secure; }
        void            mark_as_accessible() { f_accessible = true; }
        void            mark_as_secure() { f_secure = true; }

    private:
        // prevent copies or a user could reset the flag!
                        accessible_flag_t(accessible_flag_t const & rhs) = delete;
                        accessible_flag_t & operator = (accessible_flag_t const & rhs) = delete;

        controlled_vars::fbool_t    f_accessible;
        controlled_vars::fbool_t    f_secure;
    };

    static pointer_t    instance();
    virtual             ~server();

    [[noreturn]] static void exit( int const code );

    static char const * version();
    static int          version_major();
    static int          version_minor();
    static int          version_patch();
    virtual void        show_version();

    static std::string const get_server_name();

    // plugins::plugin implementation
    virtual QString     icon() const;
    virtual QString     description() const;
    virtual QString     dependencies() const;
    virtual void        bootstrap(snap_child * snap);
    virtual int64_t     do_update(int64_t last_updated);

    [[noreturn]] void   usage();
    void                setup_as_backend();
    bool                is_debug() const { return f_debug; }
    bool                is_foreground() const { return f_foreground; }
    bool                is_backend() const { return f_backend; }
    bool                is_logging_server() const { return f_using_logging_server; }
    static size_t       thread_count();
    void                set_config_filename(std::string const & filename);
    void                config( int argc, char * argv[] );
    void                set_translation(QString const xml_data);
    QString             get_parameter(QString const & param_name) const;
    void                set_parameter( QString const & param_name, QString const & value );
    void                prepare_qtapp( int argc, char * argv[] );
    bool                check_cassandra(QString const & mandatory_table);
    QtCassandra::QCassandraTable::pointer_t create_table(QtCassandra::QCassandraContext::pointer_t context, QString table_name, QString comment);
    void                detach();
    void                listen();
    void                backend();
    int                 snapdbproxy_port() const { return f_snapdbproxy_port; }
    QString const &     snapdbproxy_addr() const { return f_snapdbproxy_addr; }
    void                capture_zombies(pid_t child_pid);
    void                process_message(snap_communicator_message const & message);

    unsigned long       connections_count();

    std::string         servername() const;

    void                udp_ping_server( QString const & service, QString const & uri );
    void                udp_rusage(QString const & process_name);
    static void         block_ip( QString const & ip, QString const & period = "" );

#ifdef SNAP_NO_FORK
    bool nofork() const;
#endif

    SNAP_SIGNAL_WITH_MODE(init, (), (), NEITHER);
    SNAP_SIGNAL_WITH_MODE(update, (int64_t last_updated), (last_updated), NEITHER);
    SNAP_SIGNAL_WITH_MODE(process_cookies, (), (), NEITHER);
    SNAP_SIGNAL_WITH_MODE(attach_to_session, (), (), NEITHER);
    SNAP_SIGNAL_WITH_MODE(detach_from_session, (), (), NEITHER);
    SNAP_SIGNAL_WITH_MODE(define_locales, (QString & locales), (locales), NEITHER);
    SNAP_SIGNAL_WITH_MODE(process_post, (QString const & url), (url), NEITHER);
    SNAP_SIGNAL_WITH_MODE(execute, (QString const & url), (url), NEITHER);
    SNAP_SIGNAL_WITH_MODE(register_backend_cron, (backend_action_set & actions), (actions), NEITHER);
    SNAP_SIGNAL_WITH_MODE(register_backend_action, (backend_action_set & actions), (actions), NEITHER);
    SNAP_SIGNAL_WITH_MODE(backend_process, (), (), NEITHER);
    SNAP_SIGNAL_WITH_MODE(save_content, (), (), NEITHER);
    SNAP_SIGNAL_WITH_MODE(xss_filter, (QDomNode & node,
                             QString const & accepted_tags,
                             QString const & accepted_attributes),
                            (node, accepted_tags, accepted_attributes), NEITHER);
    SNAP_SIGNAL_WITH_MODE(improve_signature, (QString const & path, QDomDocument doc, QDomElement & signature_tag), (path, doc, signature_tag), NEITHER);
    SNAP_SIGNAL(load_file, (snap_child::post_file_t & file, bool & found), (file, found));
    SNAP_SIGNAL_WITH_MODE(table_is_accessible, (QString const & table, accessible_flag_t & accessible), (table, accessible), NEITHER);
    SNAP_SIGNAL_WITH_MODE(add_snap_expr_functions, (snap_expr::functions_t & functions), (functions), NEITHER);
    SNAP_SIGNAL_WITH_MODE(output_result, (QString const & uri_path, QByteArray & output), (uri_path, output), NEITHER);

protected:
                                server();
    static server::pointer_t    get_instance();
    static server::pointer_t    set_instance(pointer_t server);

    // See TODO in server::prepare_cassandra()
    QString                                 f_snapdbproxy_addr;     // NO DEFAULT, if isEmpty() then we are not connected / cannot connect to snapdbproxy
    int32_t                                 f_snapdbproxy_port = 0;
    snap_config                             f_parameters;

private:
    typedef std::shared_ptr<advgetopt::getopt>    getopt_ptr_t;

    friend class listener_impl;

    static void                             sighandler( int sig );

    void                                    process_connection(int socket);
    void                                    stop_thread_func();

    static pointer_t                        g_instance;

    QTranslator                             f_translator;
    QByteArray                              f_translation_xml;

    std::string                             f_servername;
    controlled_vars::fbool_t                f_debug;
    controlled_vars::fbool_t                f_foreground;
    controlled_vars::fbool_t                f_backend;
    controlled_vars::fbool_t                f_using_logging_server;
    QMap<QString, bool>                     f_created_table;

    controlled_vars::zuint64_t              f_connections_count;
    snap_child_vector_t                     f_children_running;
    snap_child_vector_t                     f_children_waiting;

    getopt_ptr_t                            f_opt;

    std::unique_ptr<snap_listen_thread>     f_listen_runner;
    std::unique_ptr<snap_thread>            f_listen_thread;

#ifdef SNAP_NO_FORK
    controlled_vars::zbool_t                f_nofork;
#endif
};

} // namespace snap
// vim: ts=4 sw=4 et