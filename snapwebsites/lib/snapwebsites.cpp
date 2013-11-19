// Snap Websites Server -- snap websites server
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

#include "snapwebsites.h"
#include "plugins.h"
#include "signal.h"
#include "log.h"
#include "not_reached.h"
#include "tcp_client_server.h"
#include <iostream>
#include <memory>
#include <QStringList>
#include <QFile>
#include <QDirIterator>
#include <QHostAddress>
#include <QCoreApplication>
#include <syslog.h>
#include <errno.h>
#include <signal.h>

namespace snap
{

std::shared_ptr<QCoreApplication> g_application;


/** \brief Get a fixed name.
 *
 * The Snap! Server makes use of a certain number of fixed names
 * which instead of being defined in macros are defined here as
 * static strings. To retrieve one of the strings, call the function
 * with the appropriate index.
 *
 * \param[in] name  The name to retrieve.
 *
 * \return A pointer to the name.
 */
const char *get_name(name_t name)
{
    switch(name) {
    case SNAP_NAME_SERVER:
        return "Snap! Server";

    case SNAP_NAME_CONTEXT:
        return "snap_websites";

    case SNAP_NAME_INDEX: // name used for the domains and websites indexes
        return "*index*"; // this is a row name inside the domains/websites tables

    case SNAP_NAME_DOMAINS: // domain/sub-domain canonalization
        return "domains";

    case SNAP_NAME_WEBSITES: // remaining of URL canonalization
        return "websites";

    case SNAP_NAME_SITES: // website global settings
        return "sites";

    case SNAP_NAME_CORE_LAST_UPDATED:
        return "core::last_updated";

    case SNAP_NAME_CORE_SITE_NAME:
        return "core::site_name";

    case SNAP_NAME_CORE_SITE_SHORT_NAME:
        return "core::site_short_name";

    case SNAP_NAME_CORE_SITE_LONG_NAME:
        return "core::site_long_name";

    case SNAP_NAME_CORE_PLUGINS:
        return "core::plugins";

    case SNAP_NAME_CORE_REDIRECT:
        return "core::redirect";

    case SNAP_NAME_CORE_RULES:
        return "core::rules";

    case SNAP_NAME_CORE_ORIGINAL_RULES:
        return "core::original_rules";

    case SNAP_NAME_CORE_PLUGIN_THRESHOLD:
        return "core::plugin_threshold";

    case SNAP_NAME_CORE_COOKIE_DOMAIN:
        return "core::cookie_domain";

    default:
        // invalid index
        throw snap_exception();

    }
    NOTREACHED();
}

/** \brief Server instance.
 *
 * The g_instance variable holds the current server instance.
 */
server *server::g_instance = NULL;

/** \brief Return the server version.
 *
 * This function can be used to verify that the server version is
 * compatible with your plugin or to display the version.
 *
 * To compare versions, however, it is suggested that you make
 * use of the version_major(), version_minor(), and version_patch()
 * instead.
 *
 * \return A pointer to a constant string representing the server version.
 */
const char *server::version()
{
    return SNAPWEBSITES_VERSION_STRING;
}

/** \brief Return the server major version.
 *
 * This function returns the major version of the server. This can be used
 * to verify that you have the correct version of the server to run your
 * plugin.
 *
 * This is a positive number.
 *
 * \return The server major version as an integer.
 */
int server::version_major()
{
    return SNAPWEBSITES_VERSION_MAJOR;
}

/** \brief Return the server minor version.
 *
 * This function returns the minor version of the server. This can be used
 * to verify that you have the correct version of the server to run your
 * plugin.
 *
 * This is a positive number.
 *
 * \return The server minor version as an integer.
 */
int server::version_minor()
{
    return SNAPWEBSITES_VERSION_MINOR;
}

/** \brief Return the server patch version.
 *
 * This function returns the patch version of the server. This can be used
 * to verify that you have the correct version of the server to run your
 * plugin.
 *
 * This is a positive number.
 *
 * \return The server patch version as an integer.
 */
int server::version_patch()
{
    return SNAPWEBSITES_VERSION_PATCH;
}

/** \brief Get the server instance.
 *
 * The main central hub is the server object.
 *
 * Like all the plugins, there can be only one server instance.
 * Because of that, it is made a singleton which means whichever
 * plugin that first needs the server can get a pointer to it at
 * any time.
 *
 * \note
 * This function is not thread safe.
 *
 * \return A pointer to the server.
 */
server *server::instance()
{
    if(g_instance == NULL) {
        g_instance = new server;
    }
    return g_instance;
}

/** \brief Return the description of this plugin.
 *
 * This function returns the English description of this plugin.
 * The system presents that description when the user is offered to
 * install or uninstall a plugin on his website. Translation may be
 * available in the database.
 *
 * \return The description in a QString.
 */
QString server::description() const
{
    return "The server plugin is hard coded in the base of the system."
        " It handles the incoming and outgoing network connections."
        " The server handles a number of messages that are global.";
}

/** \brief Update the server, the function is mandatory.
 *
 * This function is here because it is a pure virtual in the plug in. At this
 * time it does nothing and it probably will never have actual updates.
 *
 * \param[in] last_updated  The UTC Unix date when this plugin was last updated (in micro seconds).
 *
 * \return The UTC Unix date of the last update of this plugin.
 */
int64_t server::do_update(int64_t /*last_updated*/)
{
    SNAP_PLUGIN_UPDATE_INIT();
    SNAP_PLUGIN_UPDATE_EXIT();
}


/** \brief Initialize the server.
 *
 * This function initializes the server.
 */
server::server()
    : f_argv(0)
{
    // default parameters -- we may want to have a separate function and
    //                       maybe some clear separate variables?
    f_parameters["listen"] = "0.0.0.0:4004";
    f_parameters["plugins"] = "/usr/lib/snapwebsites/plugins";
    f_parameters["qs_path"] = "q";
    f_parameters["server_name"] = "";
}

/** \brief Clean up the server.
 *
 * Since the server is a singleon, it never gets deleted while running.
 * Since we use a bare pointer, it should never go out of scope, thus
 * this function should never be called.
 */
server::~server()
{
}

/** \brief Print out usage information to start the server.
 *
 * This function prints out a usage message that describes the arguments
 * that the server accepts on the command line.
 *
 * The function calls exit(1) and never returns.
 */
void server::usage()
{
    const char *server_name;
    if(f_argv == NULL) {
        server_name = "snapserver";
    }
    else {
        server_name = f_argv[0];
    }

    fprintf(stderr, "Usage: %s -<arg> ...\n"
                    "Where -<arg> is one or more of the following:\n"
                    "  -c|--config <config>   define the name of the configuration file (default \"/etc/snapwebsites/snapserver.conf\")\n"
                    "  -d|--debug             run in debug mode, and do not start in the background\n"
                    "  -h|--help              display this help\n"
                    "when run as the backend, you can specify the URI of the site to process.\n"
                    , server_name);

    exit(1);
}

/** \brief Mark the server object as a backend tool instead.
 *
 * This function is called by the backend tool to mark the server
 * as a command line tool rather than a server. In general, this
 * is ignored, but there are a few cases where it is checked to
 * make sure that everything works as expected.
 *
 * The function can be called as many times as necessary.
 */
void server::setup_as_backend()
{
    f_backend = true;
}

/** \fn server::is_backend() const;
 * \brief Check whether the server is setup as a backend.
 *
 * This function returns false unless the setup_as_backend()
 * funciton was called.
 *
 * \return true if this is a server, false if this is used as a command line tool
 */

/** \brief Configure the server.
 *
 * This function parses the command line arguments and reads the
 * configuration file.
 *
 * By default, the configuration file is defined as:
 *
 * \code
 * /etc/snapwebsites/snapserver.conf
 * \endcode
 *
 * The user may use the --config argument to use a different file.
 *
 * The function does not return if any of the arguments generate an
 * error or if the configuration file has an invalid parameter.
 *
 * \note
 * In this function we still use syslog() to log errors because the
 * logger is initialized at the end of the function once we got
 * all the necessary information to initialize the logger. Later we
 * may want to record the configuration file errors and log them
 * if we can still properly initialize the logger.
 *
 * \param[in] argc  The number of arguments in argv.
 * \param[in] argv  The array of argument strings.
 */
void server::config(int argc, char *argv[])
{
    if(!g_application)
    {
        g_application.reset( new QCoreApplication(argc, argv) );
    }

    // save the command line arguments
    f_argc = argc;
    f_argv = const_cast<const char **>(argv);

    // initialize the syslog() interface
    openlog("snapserver", LOG_NDELAY | LOG_PID, LOG_DAEMON);

    // parse the command line arguments
    controlled_vars::zbool_t help;
    for(int i(1); i < argc; ++i)
    {
        if(argv[i][0] == '-')
        {
            if(strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--action") == 0)
            {
                // this is expected to be followed by the action name
                if(i + 1 >= argc)
                {
                    syslog(LOG_CRIT, "-a must be followed by the name of an action, server not started. (in server::config())");
                    help = true;
                    break;
                }
                ++i;
                if(f_backend)
                {
                    if(f_parameters.find("__BACKEND_ACTION") == f_parameters.end())
                    {
                        f_parameters["__BACKEND_ACTION"] = argv[i];
                    }
                    else
                    {
                        syslog(LOG_CRIT, "unexpected parameter \"%s %s\", at most one action can be specified, backend not started. (in server::config())", argv[i - 1], argv[i]);
                        help = true;
                    }
                }
                else
                {
                    syslog(LOG_CRIT, "unexpected command line option \"%s\", server not started. (in server::config())", argv[i - 1]);
                    help = true;
                }
            }
            else if(strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0)
            {
                if(i + 1 >= argc)
                {
                    syslog(LOG_CRIT, "-c must be followed by the name of the configuration file, server not started. (in server::config())");
                    help = true;
                    break;
                }
                ++i;
                // TBD: should we refuse filenames that start with '-'?
                // (i.e. possibly another option)
                f_config = argv[i];
            }
            else if(strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--debug") == 0)
            {
                f_debug = true;
            }
            else if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
            {
                help = true;
            }
            //else if(strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--resources") == 0)
            //{
            //    // TODO: this does not show us the list of resources available
            //    //       in plugins... unfortunately. So that's useless at this
            //    //       point (and you need to have a site key to load the
            //    //       expected plugins...)
            //    printf("List of available resources in the server:\n");
            //    QCoreApplication app(argc, argv);
            //    QDirIterator it(":", QDirIterator::Subdirectories);
            //    while(it.hasNext()) {
            //        qDebug() << "" << it.next();
            //    }
            //    exit(1);
            //}
            else
            {
                syslog(LOG_CRIT, "unknown option \"%s\", server not started. (in server::config())", argv[i]);
                help = true;
            }
        }
        else
        {
            if(f_backend)
            {
                if(f_parameters.find("__BACKEND_URI") == f_parameters.end())
                {
                    f_parameters["__BACKEND_URI"] = argv[i];
                }
                else
                {
                    syslog(LOG_CRIT, "unexpected parameter \"%s\", at most one URI can be specified, backend not started. (in server::config())", argv[i]);
                    help = true;
                }
            }
            else
            {
                syslog(LOG_CRIT, "unexpected parameter \"%s\", server not started. (in server::config())", argv[i]);
                help = true;
            }
        }
    }
    if(help)
    {
        // if the user asked to not detach, then print the usage
        if(f_debug)
        {
            usage();
        }
        exit(1);
    }

    // read the configuration file now
    QFile c;
    if(f_config.length() == 0)
    {
        // empty string means the user did not specify a configuration file
        // and in this case it is optional
        c.setFileName("/etc/snapwebsites/snapserver.conf");
        c.open(QIODevice::ReadOnly);
        if(c.isOpen())
        {
            f_config = "/etc/snapwebsites/snapserver.conf";
        }
    }
    else
    {
        c.setFileName(f_config);
        c.open(QIODevice::ReadOnly);
        if(!c.isOpen())
        {
            if(f_debug)
            {
                fprintf(stderr, "cannot read configuration file \"%s\".\n",
                                                    f_config.toUtf8().data());
            }
            syslog(LOG_CRIT, "cannot read configuration file \"%s\", server not started. (in server::config())",
                                                    f_config.toUtf8().data());
            exit(1);
        }
    }
    if(c.isOpen()) // if no configuration exists, isOpen() returns false
    {
        char buf[256];
        for(int line(1); c.readLine(buf, sizeof(buf)) > 0; ++line)
        {
            // make sure the last byte is '\0'
            buf[sizeof(buf) - 1] = '\0';
            int len = strlen(buf);
            if(len == 0 || (buf[len - 1] != '\n' && buf[len - 1] != '\r'))
            {
                if(f_debug)
                {
                    fprintf(stderr, "line %d in \"%s\" is too long.\n",
                                        line, f_config.toUtf8().data());
                }
                syslog(LOG_CRIT, "line %d in \"%s\" is too long, server not started. (in server::config())",
                                        line, f_config.toUtf8().data());
                exit(1);
            }
            buf[len - 1] = '\0';
            --len;
            while(len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
            {
                --len;
                buf[len] = '\0';
            }
            if(len == 0)
            {
                // comment or empty line
                continue;
            }
            char *n(buf);
            while(isspace(*n))
            {
                ++n;
            }
            if(*n == '#' || *n == '\0')
            {
                // comment or empty line
                continue;
            }
            char *v(n);
            while(*v != '=' && *v != '\0')
            {
                ++v;
            }
            if(*v != '=')
            {
                if(f_debug)
                {
                    fprintf(stderr, "invalid variable on line %d in \"%s\", no equal sign found..\n",
                                                line, f_config.toUtf8().data());
                }
                syslog(LOG_CRIT, "invalid variable on line %d in \"%s\", no equal sign found, server not started. (in server::config())\n",
                                                line, f_config.toUtf8().data());
                exit(1);
            }
            char *e;
            for(e = v; e > n && isspace(e[-1]); --e);
            *e = '\0';
            do
            {
                ++v;
            }
            while(isspace(*v));
            for(e = v + strlen(v); e > v && isspace(e[-1]); --e);
            *e = '\0';
            if(v != e && ((v[0] == '\'' && e[-1] == '\'') || (v[0] == '"' && e[-1] == '"')))
            {
                // remove single or double quotes
                v++;
                e[-1] = '\0';
            }
            f_parameters[n] = v;
        }
    }

    // the name of the server is mandatory, use hostname by default
    if(f_parameters["server_name"] == "")
    {
        char host[HOST_NAME_MAX + 1];
        if(gethostname(host, sizeof(host)) != 0)
        {
            if(f_debug)
            {
                fprintf(stderr, "hostname is not available as the server name.\n");
            }
            syslog(LOG_CRIT, "hostname is not available as the server name, server not started. (in server::config())\n");
            exit(1);
        }
        f_parameters["server_name"] = host;
    }

    // finally we can initialize the log system
    logging::configure(f_parameters["log_config"]);

    // I really prevent to have an immediate message to let users know that
    // the server is starting; however, until we detach() ourselves the
    // process identifier is going to be wrong.
    SNAP_LOG_INFO("Snap v" SNAPWEBSITES_VERSION_STRING " on \"" + f_parameters["server_name"] + "\" started.");
}

/** \brief Retrieve one of the configuration file parameters.
 *
 * This function returns the value of a named parameter. The
 * parameter is defined in the configuration file, it may also
 * be given a default value when the server is initialized.
 *
 * The following are the parameters currently supported by
 * the core system. Additional parameters may be defined by
 * plugins. Remember that parameters defined in the
 * configuration file are common to ALL the websites and at
 * this point plugins do not have direct access to the
 * get_parameter() function (look at the get_site_parameter()
 * function in the snap_child class as a better alternative
 * for plugins.)
 *
 * \li cassandra_host -- the IP address or server name to Cassandra; default is localhost
 * \li cassandra_port -- the port to use to connect to Cassandra; default is 9160
 * \li data_path -- path to the directory holding the system data (images, js, css, counters, etc.)
 * \li default_plugins -- list of default plugins to initialize a new website
 * \li listen -- address:port to listen to (default 0.0.0.0:4004)
 * \li plugins -- path to the list of plugins
 * \li qs_path -- the variable holding the path in the URL; defaults to "q"
 * \li max_pending_connections -- the number of connections that can wait in
 *     the server queue, there is Snap default (i.e. the Qt TCP server default
 *     is used if undefined, which in most cases means the system of 5.)
 * \li server_name -- the name of the server, defaults to gethostname()
 * \li timeout_wait_children -- the amount of time to wait before checking on
 *     the existing children; cannot be less than 100ms; defaults to 5,000ms
 *
 * \param[in] param_name  The name of the parameter to retrieve.
 *
 * \return The value of the specified parameter.
 */
QString server::get_parameter(const QString& param_name)
{
    return f_parameters[param_name];
}

/** \brief Prepare the Cassandra database.
 *
 * This function ensures that the Cassandra database includes the default
 * context and tables (domain, website, contents.)
 *
 * This is called once each time the server is started. It doesn't matter
 * too much as it is quite fast. Only the core tables are checked. Plug-ins
 * can create new tables on the fly so it doesn't matter as much. We may
 * later provide a way for plugins to create different contexts but at
 * this point we expect all of them to only make use of the Core provided
 * context.
 */
void server::prepare_cassandra()
{
    // This function connects to the Cassandra database, but it doesn't
    // keep the connection. We are the server and the connection would
    // not be shared properly between all the children.
    f_cassandra_host = get_parameter("cassandra_host");
    if(f_cassandra_host.isEmpty())
    {
        f_cassandra_host = "localhost";
    }
    QString port_str(get_parameter("cassandra_port"));
    if(port_str.isEmpty())
    {
        port_str = "9160";
    }
    bool ok;
    f_cassandra_port = port_str.toLong(&ok);
    if(!ok)
    {
        SNAP_LOG_FATAL("invalid cassandra_port, a valid number was expected instead of \"")(port_str)("\".");
        exit(1);
    }
    if(f_cassandra_port < 1 || f_cassandra_port > 65535)
    {
        SNAP_LOG_FATAL("invalid cassandra_port, a port must be between 1 and 65535, ")(f_cassandra_port)(" is not.");
        exit(1);
    }
    QtCassandra::QCassandra cassandra;
    if(!cassandra.connect(f_cassandra_host, f_cassandra_port))
    {
        SNAP_LOG_FATAL("the connection to the Cassandra server failed (")(f_cassandra_host)(":")(f_cassandra_port)(").");
        exit(1);
    }
    // we need to read all the contexts in order to make sure the
    // findContext() works
    cassandra.contexts();
    QString context_name(snap::get_name(snap::SNAP_NAME_CONTEXT));
    QSharedPointer<QtCassandra::QCassandraContext> context(cassandra.findContext(context_name));
    if(context.isNull())
    {
        // create the context since it doesn't exist yet
        context = cassandra.context(context_name);
        context->setStrategyClass("org.apache.cassandra.locator.SimpleStrategy");
        context->setReplicationFactor(1);
        context->create();
        // we don't put the tables in here so we can call the create_table()
        // and have the tables created as required (i.e. as we add new ones
        // they get added as expected, no need for special handling.)
    }

    // create missing tables
    create_table(context, get_name(SNAP_NAME_DOMAINS),  "List of domain descriptions.");
    create_table(context, get_name(SNAP_NAME_WEBSITES), "List of website descriptions.");
}

// XXX -- would this ever be necessary here?
/** \brief Get the QCassandra pointer.
 *
 * This function retrieves the QCassandra pointer from the server.
 */
//QSharedPointer<QtCassandra::QCassandra> server::get_cassandra()
//{
//    if(f_cassandra.isNull())
//    {
//        f_cassandra = QSharedPointer<QtCassandra::QCassandra>(new QtCassandra::QCassandra);
//        if(!f_cassandra->connect(f_cassandra_host, f_cassandra_port))
//        {
//            SNAP_LOG_FATAL("the connection to the Cassandra server failed (")(f_cassandra_host)(":")(f_cassandra_port)(")");
//            exit(1);
//        }
//    }
//
//    return f_cassandra;
//}

/** \brief Create a table in the specified context.
 *
 * The function checks whether the named table exists, if not it creates it with
 * default parameters. The result is a shared pointer to the table in question.
 *
 * \todo
 * Provide a structure that includes the different table parameters instead of
 * using hard coded defaults.
 *
 * \param[in] context  The context in which the table is to be created.
 * \param[in] table_name  The name of the new table, if it exists, nothing happens.
 * \param[in] comment  A comment about the new table.
 */
QSharedPointer<QtCassandra::QCassandraTable> server::create_table(QSharedPointer<QtCassandra::QCassandraContext> context, QString table_name, QString comment)
{
    // does table exist?
    QSharedPointer<QtCassandra::QCassandraTable> table(context->findTable(table_name));
    if(table.isNull())
    {
        // table is not there yet, create it
        table = context->table(table_name);
        table->setComment(comment);
        table->setColumnType("Standard"); // Standard or Super
        table->setKeyValidationClass("BytesType");
        table->setDefaultValidationClass("BytesType");
        table->setComparatorType("BytesType");
        table->setKeyCacheSavePeriodInSeconds(14400);
        table->setMemtableFlushAfterMins(60);
        //table->setMemtableThroughputInMb(247);
        //table->setMemtableOperationsInMillions(1.1578125);
        table->setGcGraceSeconds(864000);
        table->setMinCompactionThreshold(4);
        table->setMaxCompactionThreshold(22);
        table->setReplicateOnWrite(1);
        table->create();
    }
    return table;
}

/** \brief Detach the server unless in debug mode.
 *
 * This function detaches the server unless it is in debug mode.
 */
void server::detach()
{
    if(f_debug)
    {
        return;
    }

    // detaching using fork()
    pid_t child_pid = fork();
    if(child_pid == 0)
    {
        // this is the child, make sure we keep the log alive
        logging::reconfigure();
        return;
    }

    // since we're quitting immediately we do not need to save the child_pid

    if(child_pid == -1)
    {
        SNAP_LOG_FATAL("the server could not fork() a child process to detach itself from your console.");
        exit(1);
    }

    exit(0);
}

/** \brief Listen to incoming connections.
 *
 * This function loops over a listen waiting for connections to this
 * server. The listen is made blocking since there is nothing else
 * we have to do than wait for events from the Apache server and
 * our snap.cgi tool.
 *
 * This function never returns.
 *
 * If the function finds an error in one of the parameters used from the
 * configuration file, then it prints an error and calls exit(1).
 *
 * If the server cannot start listening, then it simply prints an error
 * and calls exit(1).
 */
void server::listen()
{
    // offer the user to setup the maximum number of pending connections
    long max_pending_connections(-1);
    bool ok;
    QString max_connections(f_parameters["max_pending_connections"]);
    if(!max_connections.isEmpty())
    {
        max_pending_connections = max_connections.toLong(&ok);
        if(!ok)
        {
            SNAP_LOG_FATAL("invalid max_pending_connections, a valid number was expected instead of \"")(max_connections)("\".");
            exit(1);
        }
        if(max_pending_connections < 1)
        {
            SNAP_LOG_FATAL("max_pending_connections must be positive, \"")(max_connections)("\" is not valid.");
            exit(1);
        }
    }

    // get the address/port info
    QString listen_info(f_parameters["listen"]);
    if(listen_info.isEmpty())
    {
        listen_info = "0.0.0.0:4004";
    }
    QStringList host(listen_info.split(":"));
    if(host.count() == 1)
    {
        host[1] = "4004";
    }

    // convert the address information
    QHostAddress a(host[0]);
    if(a.isNull())
    {
        SNAP_LOG_FATAL("invalid address specification in \"")(host[0])(":")(host[1])("\".");
        exit(1);
    }

    // convert the port information
    long p = host[1].toLong(&ok);
    if(!ok || p < 0 || p > 65535)
    {
        SNAP_LOG_FATAL("invalid port specification in \"")(host[0])(":")(host[1])("\".");
        exit(1);
    }

    // get timeout time for wait when children exist
    long timeout_wait_children(5000);
    QString timeout_wait_children_param(f_parameters["timeout_wait_children"]);
    if(!timeout_wait_children_param.isEmpty())
    {
        timeout_wait_children = timeout_wait_children_param.toLong(&ok);
        if(!ok)
        {
            SNAP_LOG_FATAL("invalid timeout_wait_children, a valid number was expected instead of \"")(timeout_wait_children_param)("\".");
            exit(1);
        }
        if(timeout_wait_children < 100)
        {
            SNAP_LOG_FATAL("timeout_wait_children must be at least 100, \"")(timeout_wait_children_param)("\" is not acceptable.");
            exit(1);
        }
    }

    // initialize the server
    tcp_client_server::tcp_server s(host[0].toUtf8().data(), p, max_pending_connections, true, true);

    // wait until we get killed
    {
        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGCHLD);
        sigprocmask(SIG_BLOCK, &set, NULL);
    }
    for(;;)
    {
        // capture zombies first
        snap_child_vector_t::size_type max_children(f_children_running.size());
        for(snap_child_vector_t::size_type idx(0); idx < max_children; ++idx)
        {
            if(f_children_running[idx]->check_status() == snap_child::SNAP_CHILD_STATUS_READY)
            {
                // it's ready, so it can be reused now
                f_children_waiting.push_back(f_children_running[idx]);
                f_children_running.erase(f_children_running.begin() + idx);

                // removed one child so decrement index:
                --idx;
                --max_children;
            }
        }

        // retrieve all the connections and process them
        int socket(s.accept());
        // callee becomes the owner of socket
        if(socket != -1)
        {
            process_connection(socket);
        }
    }
}

/** \brief Process an incoming connection.
 *
 * This function processes an incoming connection from a client.
 * This connection is from the snap.cgi to the snapserver.
 *
 * \param[in] socket  The socket which represents the new connection.
 */
void server::process_connection(int socket)
{
    snap_child *child;

    // we're handling one more connection, whether it works or
    // not we increase our internal counter
    ++f_connections_count;

    if(f_children_waiting.empty())
    {
        child = new snap_child(this);
    }
    else
    {
        child = f_children_waiting.back();
        f_children_waiting.pop_back();
    }

    if(child->process(socket))
    {
        // this child is now busy
        f_children_running.push_back(child);
    }
    else
    {
        // it failed, we can keep that child as a waiting child
        f_children_waiting.push_back(child);

        // and tell the user about a problem without telling much...
        // (see the logs for more info.)
        std::string err("Status: HTTP/1.1 503 Service Unavailable\n"
                      "Expires: Sun, 19 Nov 1978 05:00:00 GMT\n"
                      "Content-type: text/html\n"
                      "\n"
                      "<h1>503 Service Unavailable</h1>\n"
                      "<p>Server cannot start child process.</p>\n");
        write(socket, err.c_str(), err.size());
        // socket will be closed by the next accept() call
    }
}


/** \brief Run the backend process.
 *
 * This function creates a child and runs its backend function.
 *
 * The function may first initialize some more things in the server.
 *
 * When the backend process ends, the function returns. Assuming everything
 * works as expected, the function is exepcted to return cleanly.
 */
void server::backend()
{
    snap_child child(this);
    child.backend();
}


/** \brief Return the number of connections received by the server.
 *
 * This function returns the connections counter. Note that this
 * counter is just an in memory counter so once the server restarts
 * it is reset to zero.
 */
unsigned long server::connections_count()
{
    return f_connections_count;
}


/** \brief Implementation of the bootstrap signal.
 *
 * This function readies the bootstrap signal.
 *
 * At this time, it does nothing.
 *
 * \param[in,out] snap  The snap child process.
 *
 * \return true if the signal has to be sent to other plugins.
 */
bool server::bootstrap_impl(snap_child * /*snap*/)
{
    return true;
}

/** \brief Initialize the Snap Websites server.
 *
 * This function readies the Init signal.
 *
 * At this time, it does nothing.
 *
 * \return true if the signal has to be sent to other plugins.
 */
bool server::init_impl()
{
    return true;
}

/** \brief Update the Snap Websites server.
 *
 * This function ensure that the data managed by this plugin is up to
 * date.
 *
 * This function does nothing at this point.
 *
 * \param[in] last_updated  The date and time when the website was last updated.
 *
 * \return true if the signal has to be sent to other plugins.
 */
bool server::update_impl(int64_t /*last_updated*/)
{
    return true;
}

/** \brief Process a POST request at the specified URL.
 *
 * This function readies the process_cookies signal.
 *
 * At this time, it does nothing.
 *
 * \param[in] url  The URL to process a POST from.
 *
 * \return true if the signal has to be sent to other plugins.
 */
bool server::process_cookies_impl()
{
    return true;
}

/** \brief Process a POST request at the specified URL.
 *
 * This function readies the process_post signal.
 *
 * At this time, it does nothing.
 *
 * \param[in] url  The URL to process a POST from.
 *
 * \return true if the signal has to be sent to other plugins.
 */
bool server::process_post_impl(const QString& /*url*/)
{
    return true;
}

/** \brief Execute the URL.
 *
 * This function readies the Execute signal.
 *
 * At this time, it does nothing.
 *
 * \param[in] url  The URL to execute.
 *
 * \return true if the signal has to be sent to other plugins.
 */
bool server::execute_impl(const QString& /*url*/)
{
    return true;
}

/** \brief Execute the specified backend action.
 *
 * This function readies the register_backend_action signal.
 *
 * At this time, it does nothing.
 *
 * \param[in,out] actions  A map where plugins can register the actions they support.
 *
 * \return true if the signal has to be sent to other plugins.
 */
bool server::register_backend_action_impl(backend_action_map_t& /*actions*/)
{
    return true;
}

/** \brief Execute the backend processes.
 *
 * This function readies the backend_process signal.
 *
 * At this time, it does nothing.
 *
 * \return true if the signal has to be sent to other plugins.
 */
bool server::backend_process_impl()
{
    return true;
}

/** \brief Request new content to be saved.
 *
 * This function readies the Save Content signal.
 *
 * At this time, it does nothing.
 *
 * \return true if the signal has to be sent to other plugins.
 */
bool server::save_content_impl()
{
    return true;
}

/** \brief Implementation of the XSS filter signal.
 *
 * This function readies the XSS filter signal.
 *
 * At this time, it does nothing.
 *
 * \param[in] node  The HTML node to check with XSS filters.
 * \param[in] acceptable_tags  The tags kept in the specified HTML. (i.e. "p a ul li")
 * \param[in] acceptable_attributes  The list of (not) acceptable attributes (i.e. "!styles")
 *
 * \return true if the signal has to be sent to other plugins.
 */
bool server::xss_filter_impl(QDomNode& /*node*/,
                             const QString& /*acceptable_tags*/,
                             const QString& /*accepted_attributes*/)
{
    return true;
}

/** \brief Improve the die() signature to add at the bottom of pages.
 *
 * This function calls all the plugins that define the signature signal so
 * they can append their own link or other information to the signature.
 * The signature is a simple mechanism to get several plugins to link to
 * a page where the user can go to continue his browsing on that website.
 * In most cases it is never called because the site will have a
 * page_not_found() call which is answered properly and thus a regular
 * error page is shown.
 *
 * The signature parameter is passed to the plugins as an in/out parameter.
 * The plugins are free to do whatever they want, including completely
 * overwrite the content although keep in mind that you cannot ensure
 * that a plugin is last in the last. In most cases you should limit
 * yourself to doing something like this:
 *
 * \code
 *   signature += " <a href=\"/search\">Search This Website</a>";
 * \endcode
 *
 * This very function does nothing, just returthisns true.
 *
 * \param[in] path  The path that generated the error
 * \param[in,out] signature  The signature as inline HTML code (i.e. no blocks!)
 *
 * \return true if the signal has to be sent to other plugins.
 */
bool server::improve_signature_impl(const QString& path, QString& signature)
{
    return true;
}



} // namespace snap

// vim: ts=4 sw=4 et
