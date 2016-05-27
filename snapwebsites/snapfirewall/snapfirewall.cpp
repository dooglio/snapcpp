// Snap Websites Server -- firewall handling by snap
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

#include "snapwebsites.h"
#include "log.h"
#include "not_used.h"
#include "snap_cassandra.h"


namespace
{

class snap_firewall;


/** \brief Handle messages from the Snap Communicator server.
 *
 * This class is an implementation of the TCP client message connection
 * so we can handle incoming messages.
 */
class messager
        : public snap::snap_communicator::snap_tcp_client_permanent_message_connection
{
public:
    typedef std::shared_ptr<messager>    pointer_t;

                        messager(snap_firewall * sfw, std::string const & addr, int port);

    // snap::snap_communicator::snap_tcp_client_permanent_message_connection implementation
    virtual void        process_message(snap::snap_communicator_message const & message);
    virtual void        process_connection_failed(std::string const & error_message);
    virtual void        process_connected();

private:
    // this is owned by a server function so no need for a smart pointer
    snap_firewall *     f_snap_firewall;
};


/** \brief The timer to produce wake up calls once in a while.
 *
 * This timer is used to wake us once in a while as determined by when
 * an IP address has to be removed from the firewall.
 *
 * The date feature is always used on this timer (i.e. wake up
 * the process at a specific date and time in microseconds.)
 */
class wakeup_timer
        : public snap::snap_communicator::snap_timer
{
public:
    typedef std::shared_ptr<wakeup_timer>        pointer_t;

                                wakeup_timer(snap_firewall * sfw);

    // snap::snap_communicator::snap_timer implementation
    virtual void                process_timeout();

private:
    snap_firewall *             f_snap_firewall;
};


/** \brief Firewall process class.
 *
 * This class handles firewall requests.
 *
 * There are two requests that this process handles:
 *
 * 1) request to setup a firewall in the first place. This means setting
 *    up the necessary files under /etc so the server boots with a strong
 *    firewall as one would expect on any sane server;
 *
 * 2) request to, generally temporarilly, block IP addresses on the
 *    firewall; when a spam or hacker hit is detected, then a message
 *    is expected to be sent to this firewall process to block the
 *    IP address of that spammer or hacker.
 *
 * \msc
 * hscale = 2;
 * a [label="snapfirewall"],
 * b [label="snapcommunicator"],
 * c [label="other-process"],
 * d [label="iplock"];
 *
 * #
 * # Register snapfirewall
 * #
 * a=>a [label="connect socket to snapcommunicator"];
 * a->b [label="REGISTER service=snapfirewall;version=<VERSION>"];
 * b->a [label="READY"];
 * b->a [label="HELP"];
 * a->b [label="COMMANDS list=HELP,LOG,..."];
 *
 * #
 * # Reconfigure logger
 * #
 * b->a [label="LOG"];
 * a=>a [label="logging::recongigure()"];
 *
 * #
 * # Stop snapfirewall
 * #
 * b->a [label="STOP"];
 * a=>a [label="exit(0);"];
 *
 * #
 * # Block an IP address
 * #
 * c->b [label="snapfirewall/BLOCK ip=...;period=..."];
 * b->a [label="BLOCK ip=...;period=..."];
 * a->d [label="lock IP address with iptables"];
 *
 * #
 * # Wakeup timer
 * #
 * a->a [label="wakeup timer timed out"];
 * a=>a [label="unblocked an IP address"];
 * \endmsc
 */
class snap_firewall
{
public:
    typedef std::shared_ptr<snap_firewall>      pointer_t;

                                snap_firewall( int argc, char * argv[] );
                                ~snap_firewall();

    static pointer_t            instance( int argc, char * argv[] );

    void                        run();
    void                        process_timeout();
    void                        process_message(snap::snap_communicator_message const & message);

    static void                 sighandler( int sig );

private:
                                snap_firewall( snap_firewall const & ) = delete;
    snap_firewall &             operator = ( snap_firewall const & ) = delete;

    void                        usage();
    void                        setup_firewall();
    void                        next_wakeup();
    void                        stop(bool quitting);

    advgetopt::getopt                           f_opt;
    snap::snap_config                           f_config;
    snap::snap_config                           f_server_config;
    QString                                     f_log_conf = "/etc/snapwebsites/snapfirewall.properties";
    QString                                     f_server_name;
    QString                                     f_communicator_addr = "127.0.0.1";
    int                                         f_communicator_port = 4040;
    snap::snap_communicator::pointer_t          f_communicator;
    snap::snap_cassandra                        f_cassandra;
    QtCassandra::QCassandraTable::pointer_t     f_firewall_table;
    bool                                        f_stop_received = false;
    bool                                        f_debug = false;
    messager::pointer_t                         f_messager;
    wakeup_timer::pointer_t                     f_wakeup_timer;
};










/** \brief Initializes the timer with a pointer to the snap firewall.
 *
 * The constructor saves the pointer of the snap_firewall object so
 * it can later be used when the process timeouts.
 *
 * By default the timer is "off" meaning that it will not trigger
 * a process_timeout() call until you turn it on.
 *
 * \param[in] sfw  A pointer to the snap_firewall object.
 */
wakeup_timer::wakeup_timer(snap_firewall * sfw)
    : snap_timer(-1)
    , f_snap_firewall(sfw)
{
    set_name("snap_firewall wakeup_timer");
}


/** \brief The wake up timer timed out.
 *
 * The wake up timer is used to know when we have to remove IP
 * addresses from the firewall. Adding happens at the start and
 * whenever another service tells us to add an IP. Removal,
 * however, we are on our own.
 *
 * Whenever an IP is added by a service, it is accompagned by a
 * time period it should be blocked for. This may be forever, however,
 * when the amount of time is not forever, the snapfirewall tool
 * needs to wake up at some point. Note that those times are saved in
 * the database so one can know when to remove IPs even across restart
 * (actually, on a restart we usually do the opposite, we refill the
 * firewall with existing IP addresses that have not yet timed out;
 * however, if this was not a full server restart, then we do removals
 * only.)
 *
 * Note that the messager may receive an UNBLOCK command in which
 * case an IP gets removed immediately and the timer reset to the
 * next IP that needs to be removed as required.
 */
void wakeup_timer::process_timeout()
{
    f_snap_firewall->process_timeout();
}





/** \brief The messager initialization.
 *
 * The messager is a connection to the snapcommunicator server.
 *
 * In most cases we receive BLOCK, STOP, and LOG messages from it. We
 * implement a few other messages too (HELP, READY...)
 *
 * We use a permanent connection so if the snapcommunicator restarts
 * for whatever reason, we reconnect automatically.
 *
 * \note
 * The messager connection used by the snapfirewall tool makes use
 * of a thread. You will want to change this initialization function
 * if you intend to fork() direct children of ours (i.e. not fork()
 * + execv() as we do to run iptables.)
 *
 * \param[in] sfw  The snap firewall server we are listening for.
 * \param[in] addr  The address to connect to. Most often it is 127.0.0.1.
 * \param[in] port  The port to listen on (4040).
 */
messager::messager(snap_firewall * sfw, std::string const & addr, int port)
    : snap_tcp_client_permanent_message_connection(addr, port)
    , f_snap_firewall(sfw)
{
    set_name("snap_firewall messager");
}


/** \brief Pass messages to the Snap Firewall.
 *
 * This callback is called whenever a message is received from
 * Snap! Communicator. The message is immediately forwarded to the
 * snap_firewall object which is expected to process it and reply
 * if required.
 *
 * \param[in] message  The message we just received.
 */
void messager::process_message(snap::snap_communicator_message const & message)
{
    f_snap_firewall->process_message(message);
}


/** \brief The messager could not connect to snapcommunicator.
 *
 * This function is called whenever the messagers fails to
 * connect to the snapcommunicator server. This could be
 * because snapcommunicator is not running or because the
 * configuration information for the snapfirewall is wrong...
 *
 * With snapinit the snapcommunicator should always already
 * be running so this error should not happen once everything
 * is properly setup.
 *
 * \param[in] error_message  An error message.
 */
void messager::process_connection_failed(std::string const & error_message)
{
    SNAP_LOG_ERROR("connection to snapcommunicator failed (")(error_message)(")");

    // also call the default function, just in case
    snap_tcp_client_permanent_message_connection::process_connection_failed(error_message);
}


/** \brief The connection was established with Snap! Communicator.
 *
 * Whenever the connection is established with the Snap! Communicator,
 * this callback function is called.
 *
 * The messager reacts by REGISTERing the snap_firewall with the Snap!
 * Communicator. The name of the backend is taken from the action
 * it was called with.
 */
void messager::process_connected()
{
    snap_tcp_client_permanent_message_connection::process_connected();

    snap::snap_communicator_message register_firewall;
    register_firewall.set_command("REGISTER");
    register_firewall.add_parameter("service", "snapfirewall");
    register_firewall.add_parameter("version", snap::snap_communicator::VERSION);
    send_message(register_firewall);
}






/** \brief List of configuration files.
 *
 * This variable is used as a list of configuration files. It is
 * empty here because the configuration file may include parameters
 * that are not otherwise defined as command line options.
 */
std::vector<std::string> const g_configuration_files;


/** \brief Command line options.
 *
 * This table includes all the options supported by the server.
 */
advgetopt::getopt::option const g_snapfirewall_options[] =
{
    {
        '\0',
        advgetopt::getopt::GETOPT_FLAG_SHOW_USAGE_ON_ERROR,
        NULL,
        NULL,
        "Usage: %p [-<opt>]",
        advgetopt::getopt::help_argument
    },
    {
        '\0',
        advgetopt::getopt::GETOPT_FLAG_SHOW_USAGE_ON_ERROR,
        NULL,
        NULL,
        "where -<opt> is one or more of:",
        advgetopt::getopt::help_argument
    },
    {
        'c',
        advgetopt::getopt::GETOPT_FLAG_ENVIRONMENT_VARIABLE | advgetopt::getopt::GETOPT_FLAG_SHOW_USAGE_ON_ERROR,
        "config",
        "/etc/snapwebsites/snapfirewall.conf",
        "Configuration file to initialize snapfirewall.",
        advgetopt::getopt::optional_argument
    },
    {
        '\0',
        advgetopt::getopt::GETOPT_FLAG_ENVIRONMENT_VARIABLE,
        "connect",
        nullptr,
        "The address and port information to connect to snapcommunicator (defined in /etc/snapwebsites/snapinit.xml).",
        advgetopt::getopt::required_argument
    },
    {
        '\0',
        advgetopt::getopt::GETOPT_FLAG_ENVIRONMENT_VARIABLE,
        "debug",
        nullptr,
        "Start the snapfirewall in debug mode.",
        advgetopt::getopt::no_argument
    },
    {
        'h',
        advgetopt::getopt::GETOPT_FLAG_SHOW_USAGE_ON_ERROR,
        "help",
        nullptr,
        "Show usage and exit.",
        advgetopt::getopt::no_argument
    },
    {
        'l',
        advgetopt::getopt::GETOPT_FLAG_ENVIRONMENT_VARIABLE,
        "logfile",
        nullptr,
        "Full path to the snapfirewall logfile.",
        advgetopt::getopt::optional_argument
    },
    {
        'n',
        advgetopt::getopt::GETOPT_FLAG_ENVIRONMENT_VARIABLE,
        "nolog",
        nullptr,
        "Only output to the console, not a log file.",
        advgetopt::getopt::no_argument
    },
    {
        '\0',
        advgetopt::getopt::GETOPT_FLAG_ENVIRONMENT_VARIABLE,
        "server-name",
        nullptr,
        "The name of the server that is going to run this instance of snapfirewall (defined in /etc/snapwebsites/snapinit.conf), this parameter is required.",
        advgetopt::getopt::required_argument
    },
    {
        '\0',
        advgetopt::getopt::GETOPT_FLAG_ENVIRONMENT_VARIABLE,
        "snapdbproxy",
        nullptr,
        "The address and port information to connect to snapdbproxy (defined in /etc/snapwebsites/snapinit.xml).",
        advgetopt::getopt::required_argument
    },
    {
        '\0',
        advgetopt::getopt::GETOPT_FLAG_SHOW_USAGE_ON_ERROR,
        "version",
        nullptr,
        "show the version of the snapfirewall executable.",
        advgetopt::getopt::no_argument
    },
    {
        '\0',
        0,
        nullptr,
        nullptr,
        nullptr,
        advgetopt::getopt::end_of_options
    }
};



/** \brief This function initialize a snap_firewall object.
 *
 * The constructor puts in place the command line options by
 * parsing them. Also if the user specified --help or
 * --version, then the corresponding data is printed and
 * the process ends immediately.
 *
 * As we are at it, we also load the configuration file and
 * setup the logger.
 *
 * \param[in] argc  The command line argc parameter.
 * \param[in] argv  The command line argv parameter.
 */
snap_firewall::snap_firewall( int argc, char * argv[] )
    : f_opt(argc, argv, g_snapfirewall_options, g_configuration_files, "SNAPFIREWALL_OPTIONS")
    , f_cassandra(f_server_config)
{
    if(f_opt.is_defined("help"))
    {
        usage();
        snap::NOTREACHED();
    }

    if(f_opt.is_defined("version"))
    {
        std::cout << SNAPWEBSITES_VERSION_STRING << std::endl;
        exit(0);
        snap::NOTREACHED();
    }

    f_debug = f_opt.is_defined("debug");

    // read the configuration file
    //
    f_config.read_config_file( f_opt.get_string("config").c_str() );

    // the "server" configuration file is used in f_cassandra and it
    // needs to have the cassandra connection information
    //
    f_server_config["snapdbproxy_listen"] = f_opt.get_string("snapdbproxy").c_str();

    // setup the logger
    //
    if( f_opt.is_defined( "nolog" ) )
    {
        snap::logging::configure_console();
    }
    else if( f_opt.is_defined("logfile") )
    {
        snap::logging::configure_logfile( QString::fromUtf8(f_opt.get_string( "logfile" ).c_str()) );
    }
    else
    {
        if( f_config.contains("log_config") )
        {
            // use .conf definition when available
            f_log_conf = f_config["log_config"];
        }
        snap::logging::configure_conffile( f_log_conf );
    }

    if( f_debug )
    {
        // Force the logger level to DEBUG
        // (unless already lower)
        //
        snap::logging::reduce_log_output_level( snap::logging::log_level_t::LOG_LEVEL_DEBUG );
    }

    // do not do too much in the constructor or we may get in
    // trouble (i.e. calling shared_from_this() from the
    // constructor fails)
}


/** \brief Clean up the snap firewall.
 *
 * This function is used to do some clean up of the snap firewall
 * environment.
 */
snap_firewall::~snap_firewall()
{
    f_communicator.reset();
}


/** \brief Print out the usage information for snapfirewall.
 *
 * This function returns the snapfirewall usage information to the
 * user whenever an invalid command line option is used or
 * --help is used explicitly.
 *
 * The function does not return.
 */
void snap_firewall::usage()
{
    f_opt.usage( advgetopt::getopt::no_error, "snapfirewall" );
    snap::NOTREACHED();
}




/** \brief Execute the firewall run() loop.
 *
 * This function initializes the various connections used by the
 * snapfirewall process and then runs the event loop.
 *
 * In effect, this function finishes the initialization of the
 * snap_firewall object.
 */
void snap_firewall::run()
{
    // Stop on these signals, log them, then terminate.
    //
    signal( SIGSEGV, snap_firewall::sighandler );
    signal( SIGBUS,  snap_firewall::sighandler );
    signal( SIGFPE,  snap_firewall::sighandler );
    signal( SIGILL,  snap_firewall::sighandler );

    // get the server name
    //
    f_server_name = f_opt.get_string("server-name").c_str();

    SNAP_LOG_INFO("--------------------------------- snapfirewall started on ")(f_server_name);

    // connect to Cassandra and get a pointer to our firewall table
    //
    {
        f_cassandra.connect();
        f_cassandra.create_table("firewall", "List of IP addresses we want blocked");
        f_firewall_table = f_cassandra.create_table("firewall", "List of IP addresses we want blocked");
    }

    // retrieve the snap communicator information
    //
    tcp_client_server::get_addr_port(f_opt.get_string("connect").c_str(), f_communicator_addr, f_communicator_port, "tcp");

    // initialize the communicator and its connections
    //
    f_communicator = snap::snap_communicator::instance();

    f_wakeup_timer.reset(new wakeup_timer(this));
    f_communicator->add_connection(f_wakeup_timer);

    f_messager.reset(new messager(this, f_communicator_addr.toUtf8().data(), f_communicator_port));
    f_communicator->add_connection(f_messager);

    f_communicator->run();
}


/** \brief Setup the firewall on startup.
 *
 * On startup we have to assume that the firewall is not yet properly setup
 * so we run the follow process once.
 *
 * The process gets all the IPs defined in the database and:
 *
 * \li unblock the addresses which are out of date
 * \li unblock and (re-)block adresses that are not out of date
 *
 * The unblock and re-block process is necessary in case you are restarting
 * the process. The problem is that the IP address may already be in your
 * firewall. If that's the case, just blocking would duplicate it, which
 * would slow down the firewall for nothing and also would not properly
 * unblock the IP when we receive the timeout because that process would
 * only remove one instance.
 */
void snap_firewall::setup_firewall()
{
    int64_t const now(snap::snap_communicator::get_current_date());
    int64_t const limit(now + 60LL * 1000000LL);

    QtCassandra::QCassandraRow::pointer_t row(f_firewall_table->row(f_server_name));
    row->clearCache();

    // the first row we keep has a date we use to know when to wake up
    // next and drop that IP from our firewall
    //
    bool first(true);

    // run through the entire table
    //
    auto column_predicate = std::make_shared<QtCassandra::QCassandraCellRangePredicate>();
    column_predicate->setCount(100);
    column_predicate->setIndex(); // behave like an index
    for(;;)
    {
        row->readCells(column_predicate);
        QtCassandra::QCassandraCells const cells(row->cells());
        if(cells.isEmpty())
        {
            // it looks like we are done
            break;
        }

        for(QtCassandra::QCassandraCells::const_iterator it(cells.begin());
                                                         it != cells.end();
                                                         ++it)
        {
            QtCassandra::QCassandraCell::pointer_t cell(*it);

            // first we want to unblock that IP address
            //
            QString const ip(cell->value().stringValue());
            QString iplock_remove(QString("iplock --remove %1").arg(ip));
            int const rr(system(iplock_remove.toUtf8().data()));
            if(rr != 0)
            {
                int const e(errno);
                SNAP_LOG_ERROR("an error occurred trying to run \"")(iplock_remove)("\", errno: ")(e)(" -- ")(strerror(e));
            }

            QByteArray const key(it.key());
            int64_t const drop_date(QtCassandra::safeInt64Value(key, 0, -1));
            if(drop_date < limit)
            {
                // drop that row, it is too old
                //
                row->dropCell(key);
            }
            else
            {
                // this IP is still expected to be blocked, so
                // re-block it
                //
                if(first)
                {
                    // on the first one, we want to mark that as the
                    // time when the block has to be dropped
                    //
                    // Note: only the first one is necessary since these
                    //       are sorted by date in the database
                    //
                    first = false;
                    f_wakeup_timer->set_timeout_date(drop_date);
                }

                QString const iplock_block(QString("iplock --block %1").arg(ip));
                int const rb(system(iplock_block.toUtf8().data()));
                if(rb != 0)
                {
                    int const e(errno);
                    SNAP_LOG_ERROR("an error occurred trying to run \"")(iplock_block)("\", errno: ")(e)(" -- ")(strerror(e));
                }
            }
        }
    }
}


/** \brief Timeout is called whenever an IP address needs to be unblocked.
 *
 * This function is called when the wakeup timer times out. We set the
 * date when the wakeup timer has to time out to the next IP that
 * times out. That information comes from the Cassandra database.
 *
 * Certain IP addresses are permanently added to the firewall,
 * completely preventing the offender from accessing us for the
 * rest of time.
 */
void snap_firewall::process_timeout()
{
    // STOP received?
    // the timer may still tick once after we received a STOP event
    // so we want to check here to make sure we are good.
    //
    if(f_stop_received)
    {
        return;
    }

    // we are interested only by the columns that concern us, which
    // means columns that have a name starting with the server name
    // as defined in the snapserver.conf file
    //
    //      <server-name> '/' <date with leading zeroes in minutes (10 digits)>
    //
    int64_t const now(snap::snap_communicator::get_current_date());

    QtCassandra::QCassandraRow::pointer_t row(f_firewall_table->row(f_server_name));
    row->clearCache();

    // unblock IP addresses which have a timeout in the past
    //
    auto column_predicate = std::make_shared<QtCassandra::QCassandraCellRangePredicate>();
    QByteArray limit;
    QtCassandra::setInt64Value(limit, 0);  // whatever the first column is
    column_predicate->setStartCellKey(limit);
    QtCassandra::setInt64Value(limit, now + 60LL * 1000000LL);  // until now within 1 minute
    column_predicate->setEndCellKey(limit);
    column_predicate->setCount(100);
    column_predicate->setIndex(); // behave like an index
    for(;;)
    {
        row->readCells(column_predicate);
        QtCassandra::QCassandraCells const cells(row->cells());
        if(cells.isEmpty())
        {
            // it looks like we are done
            break;
        }

        // any entries we grab here, we drop right now
        //
        for(QtCassandra::QCassandraCells::const_iterator it(cells.begin());
                                                         it != cells.end();
                                                         ++it)
        {
            QtCassandra::QCassandraCell::pointer_t cell(*it);

            // first we want to unblock that IP address
            //
            QString const ip(cell->value().stringValue());
            QString iplock(QString("iplock --remove %1").arg(ip));
            int r(system(iplock.toUtf8().data()));
            if(r != 0)
            {
                int const e(errno);
                SNAP_LOG_ERROR("an error occurred trying to run \"")(iplock)("\", errno: ")(e)(" -- ")(strerror(e));
            }

            // now drop that row
            //
            QByteArray const key(cell->columnKey());
            row->dropCell(key);
        }
    }

    next_wakeup();
}


/** \brief Called whenever the firewall table changes.
 *
 * Whenever the firewall table changes, the next wake up date may change.
 * This function makes sure to determine what the smallest date is and
 * saves that in the wakeup timer if such a smaller date exists.
 *
 * \note
 * At this time, the setup() function does this on its own since it has
 * the information without the need for yet another access to the
 * database.
 */
void snap_firewall::next_wakeup()
{
    QtCassandra::QCassandraRow::pointer_t row(f_firewall_table->row(f_server_name));

    // determine whether there is another IP in the table and if so at
    // what time we need to wake up to remove it from the firewall
    //
    auto column_predicate = std::make_shared<QtCassandra::QCassandraCellRangePredicate>();
    column_predicate->setCount(1);
    column_predicate->setIndex(); // behave like an index
    row->clearCache();
    row->readCells(column_predicate);
    QtCassandra::QCassandraCells const cells(row->cells());
    if(!cells.isEmpty())
    {
        QByteArray const key(cells.begin().key());
        int64_t const limit(QtCassandra::safeInt64Value(key, 0, -1));
        if(limit >= 0)
        {
            // we have a valid date to wait on,
            // save it in our wakeup timer
            //
            f_wakeup_timer->set_timeout_date(limit);
        }
    }
}


/** \brief Process a message received from Snap! Communicator.
 *
 * This function gets called whenever the Snap! Communicator sends
 * us a message. This includes the READY and HELP commands, although
 * the most important one is certainly the BLOCK and STOP commands
 * used to block an IP address for a given period of time and the
 * request for this process to STOP as soon as possible.
 *
 * \param[in] message  The message we just received.
 */
void snap_firewall::process_message(snap::snap_communicator_message const & message)
{
    SNAP_LOG_TRACE("received messager message [")(message.to_message())("] for ")(f_server_name);

    QString const command(message.get_command());

    if(command == "BLOCK")
    {
        // BLOCK an ip address
        //

        // check the ip and period parameters, if valid, forward to
        // other snapfirewall instances
        //
        QString const ip(message.get_parameter("ip"));
        if(ip.isEmpty())
        {
            SNAP_LOG_ERROR("BLOCK sent to \"")(f_server_name)("\" service without an IP. BLOCK will be ignored.");
            return;
        }

        int64_t block_period(24LL * 60LL * 60LL * 1000000LL);
        QString const period(message.get_parameter("period"));
        if(!period.isEmpty())
        {
            if(period == "hour")
            {
                block_period = 60LL * 60LL * 1000000LL;
            }
            else if(period == "day")
            {
                block_period = 24LL * 60LL * 60LL * 1000000LL;
            }
            else if(period == "week")
            {
                block_period = 7LL * 24LL * 60LL * 60LL * 1000000LL;
            }
            else if(period == "month")
            {
                block_period = 31LL * 24LL * 60LL * 60LL * 1000000LL;
            }
            else if(period == "year")
            {
                block_period = 366LL * 24LL * 60LL * 60LL * 1000000LL;
            }
            else if(period == "forever")
            {
                // 5 years is certainly very much like forever!
                //
                block_period = 5LL * 366LL * 24LL * 60LL * 60LL * 1000000LL;
            }
            else
            {
                // keep default of 1 day, but log an error
                //
                SNAP_LOG_ERROR("unknown period \"")(period)("\" to block an IP address. Revert to default of 1 day.");
            }
        }

        // TODO: send to other snapfirewall instances...

        // save in our list of blocked IP addresses
        //
        QtCassandra::QCassandraRow::pointer_t row(f_firewall_table->row(f_server_name));
        int64_t const now(snap::snap_communicator::get_current_date());
        QByteArray key;
        QtCassandra::setInt64Value(key, now + block_period);
        row->cell(key)->setValue(ip);

        next_wakeup();

        QString const iplock_block(QString("iplock --block %1").arg(ip));
        int const r(system(iplock_block.toUtf8().data()));
        if(r != 0)
        {
            int const e(errno);
            SNAP_LOG_ERROR("an error occurred trying to run \"")(iplock_block)("\", errno: ")(e)(" -- ")(strerror(e));
        }

        return;
    }

    if(command == "LOG")
    {
        // logrotate just rotated the logs, we have to reconfigure
        //
        SNAP_LOG_INFO("Logging reconfiguration.");
        snap::logging::reconfigure();
        return;
    }

    if(command == "STOP")
    {
        // Someone is asking us to leave (probably snapinit)
        //
        stop(false);
        return;
    }
    if(command == "QUITTING")
    {
        // If we received the QUITTING command, then somehow we sent
        // a message to Snap! Communicator, which is already in the
        // process of quitting... we should get a STOP too, but we
        // can just quit ASAP too
        //
        stop(true);
        return;
    }

    if(command == "READY")
    {
        // Snap! Communicator received our REGISTER command
        //

        return;
    }

    if(command == "HELP")
    {
        // Snap! Communicator is asking us about the commands that we support
        //
        snap::snap_communicator_message reply;
        reply.set_command("COMMANDS");

        // list of commands understood by service
        //
        reply.add_parameter("list", "BLOCK,HELP,LOG,QUITTING,READY,STOP,UNKNOWN");

        f_messager->send_message(reply);

        // now that we are fully registered, setup the firewall
        //
        setup_firewall();

        // send a message to the snapinit service letting it know
        // that it can now safely start the snapserver (i.e. we
        // blocked all the unwanted IP addresses)
        //
        // We can do this here because we blocked the initialization
        // to setup the firewall. So the snapfirewall has been safe
        // for a little while now.
        //
        snap::snap_communicator_message safe_message;
        safe_message.set_command("SAFE");
        safe_message.set_service("snapinit");
        safe_message.add_parameter("name", "firewall");
        f_messager->send_message(safe_message);

        return;
    }

    if(command == "UNKNOWN")
    {
        // we sent a command that Snap! Communicator did not understand
        //
        SNAP_LOG_ERROR("we sent unknown command \"")(message.get_parameter("command"))("\" and probably did not get the expected result.");
        return;
    }

    // unknown command is reported and process goes on
    //
    SNAP_LOG_ERROR("unsupported command \"")(command)("\" was received on the connection with Snap! Communicator.");
    {
        snap::snap_communicator_message reply;
        reply.set_command("UNKNOWN");
        reply.add_parameter("command", command);
        f_messager->send_message(reply);
    }

    return;
}


/** \brief Called whenever we receive the STOP command or equivalent.
 *
 * This function makes sure the snapfirewall exits as quickly as
 * possible.
 *
 * \li Marks the messager as done.
 * \li Disabled wakeup timer.
 * \li UNREGISTER from snapcommunicator.
 * \li Remove wakeup timer from snapcommunicator.
 *
 * \note
 * If the f_messager is still in place, then just sending the
 * UNREGISTER is enough to quit normally. The socket of the
 * f_messager will be closed by the snapcommunicator server
 * and we will get a HUP signal. However, we get the HUP only
 * because we first mark the messager as done.
 *
 * \param[in] quitting  Set to true if we received a QUITTING message.
 */
void snap_firewall::stop(bool quitting)
{
    f_stop_received = true;

    if(f_messager)
    {
        f_messager->mark_done();
    }

    // stop the timer immediately, although that will not prevent
    // one more call to their callbacks which thus still have to
    // check the f_stop_received flag
    //
    if(f_wakeup_timer)
    {
        f_wakeup_timer->set_enable(false);
        f_wakeup_timer->set_timeout_date(-1);
    }

    // unregister if we are still connected to the messager
    // and Snap! Communicator is not already quitting
    //
    if(f_messager && !quitting)
    {
        snap::snap_communicator_message cmd;
        cmd.set_command("UNREGISTER");
        cmd.add_parameter("service", "snapfirewall");
        f_messager->send_message(cmd);
    }

    if(f_communicator)
    {
        //f_communicator->remove_connection(f_messager); -- this one will get an expected HUP shortly
        f_communicator->remove_connection(f_wakeup_timer);
    }
}


/** \brief A static function to capture various signals.
 *
 * This function captures unwanted signals like SIGSEGV and SIGILL.
 *
 * The handler logs the information and then the service exists.
 * This is done mainly so we have a chance to debug problems even
 * when it crashes on a remote server.
 *
 * \warning
 * The signals are setup after the construction of the snap_firewall
 * object because that's where we initialize the logger.
 *
 * \param[in] sig  The signal that was just emitted by the OS.
 */
void snap_firewall::sighandler( int sig )
{
    QString signame;
    switch( sig )
    {
    case SIGSEGV:
        signame = "SIGSEGV";
        break;

    case SIGBUS:
        signame = "SIGBUS";
        break;

    case SIGFPE:
        signame = "SIGFPE";
        break;

    case SIGILL:
        signame = "SIGILL";
        break;

    default:
        signame = "UNKNOWN";
        break;

    }

    {
        snap::snap_exception_base::output_stack_trace();
        SNAP_LOG_FATAL("Fatal signal caught: ")(signame);
    }

    // Exit with error status
    //
    ::exit( 1 );
    snap::NOTREACHED();
}






} // no name namespace


int main(int argc, char * argv[])
{
    try
    {
        // create an instance of the snap_firewall object
        //
        snap_firewall firewall( argc, argv );

        // Now run!
        //
        firewall.run();

        // exit normally (i.e. we received a STOP message on our
        // connection with the Snap! Communicator service.)
        //
        return 0;
    }
    catch( snap::snap_exception const & e )
    {
        SNAP_LOG_FATAL("snapfirewall: snap_exception caught! ")(e.what());
    }
    catch( std::invalid_argument const & e )
    {
        SNAP_LOG_FATAL("snapfirewall: invalid argument: ")(e.what());
    }
    catch( std::exception const & e )
    {
        SNAP_LOG_FATAL("snapfirewall: std::exception caught! ")(e.what());
    }
    catch( ... )
    {
        SNAP_LOG_FATAL("snapfirewall: unknown exception caught!");
    }

    return 1;
}


// vim: ts=4 sw=4 et