/*
 * Text:
 *      snaplock.cpp
 *
 * Description:
 *      A daemon to synchronize processes between any number of computers
 *      by blocking all of them but one.
 *
 * License:
 *      Copyright (c) 2016 Made to Order Software Corp.
 *
 *      http://snapwebsites.org/
 *      contact@m2osw.com
 *
 *      Permission is hereby granted, free of charge, to any person obtaining a
 *      copy of this software and associated documentation files (the
 *      "Software"), to deal in the Software without restriction, including
 *      without limitation the rights to use, copy, modify, merge, publish,
 *      distribute, sublicense, and/or sell copies of the Software, and to
 *      permit persons to whom the Software is furnished to do so, subject to
 *      the following conditions:
 *
 *      The above copyright notice and this permission notice shall be included
 *      in all copies or substantial portions of the Software.
 *
 *      THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 *      OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *      MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 *      IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 *      CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 *      TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 *      SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

// ourselves
//
#include "snaplock.h"

// our lib
//
#include "log.h"
#include "qstring_stream.h"
#include "dbutils.h"
#include "snap_string_list.h"

// 3rd party libs
//
#include <QtCore>
//#include <QtSql>
#include <QtCassandra/QCassandra.h>
//#include <controlled_vars/controlled_vars_need_init.h>
#include <advgetopt/advgetopt.h>

// system
//
#include <algorithm>
#include <iostream>
#include <sstream>

namespace
{
    const std::vector<std::string> g_configuration_files; // Empty

    const advgetopt::getopt::option g_snaplock_options[] =
    {
        {
            '\0',
            advgetopt::getopt::GETOPT_FLAG_SHOW_USAGE_ON_ERROR,
            nullptr,
            nullptr,
            "Usage: %p [-<opt>]",
            advgetopt::getopt::help_argument
        },
        {
            '\0',
            advgetopt::getopt::GETOPT_FLAG_SHOW_USAGE_ON_ERROR,
            nullptr,
            nullptr,
            "where -<opt> is one or more of:",
            advgetopt::getopt::help_argument
        },
        {
            'c',
            advgetopt::getopt::GETOPT_FLAG_ENVIRONMENT_VARIABLE | advgetopt::getopt::GETOPT_FLAG_SHOW_USAGE_ON_ERROR,
            "config",
            "/etc/snapwebsites/snaplock.conf",
            "Configuration file to initialize snaplock.",
            advgetopt::getopt::optional_argument
        },
        {
            '\0',
            advgetopt::getopt::GETOPT_FLAG_ENVIRONMENT_VARIABLE,
            "connect",
            nullptr,
            "Define the address and port of the snapcommunicator service (i.e. 127.0.0.1:4040).",
            advgetopt::getopt::required_argument
        },
        {
            '\0',
            advgetopt::getopt::GETOPT_FLAG_ENVIRONMENT_VARIABLE,
            "debug",
            nullptr,
            "Start the snaplock daemon in debug mode.",
            advgetopt::getopt::no_argument
        },
        {
            '\0',
            advgetopt::getopt::GETOPT_FLAG_SHOW_USAGE_ON_ERROR,
            "help",
            nullptr,
            "show this help output",
            advgetopt::getopt::no_argument
        },
        {
            'l',
            advgetopt::getopt::GETOPT_FLAG_ENVIRONMENT_VARIABLE,
            "logfile",
            nullptr,
            "Full path to the snaplock logfile.",
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
            "Define the name of the server this service is running on.",
            advgetopt::getopt::required_argument
        },
        {
            '\0',
            advgetopt::getopt::GETOPT_FLAG_ENVIRONMENT_VARIABLE,
            "snapdbproxy",
            nullptr,
            "This parameter is currently ignored since snaplock never connects to the Snap! data store.",
            advgetopt::getopt::required_argument
        },
        {
            '\0',
            advgetopt::getopt::GETOPT_FLAG_SHOW_USAGE_ON_ERROR,
            "version",
            nullptr,
            "show the version of the snapdb executable",
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


}
//namespace


/** \class snaplock
 * \brief Class handling intercomputer locking.
 *
 * This class is used in order to create an intercomputer lock on request.
 *
 * The class implements the Snap! Communicator messages and implements
 * the LOCK and UNLOCK commands and sends the LOCKED command to its
 * sender.
 *
 * The system makes use of the Lamport's Bakery Algorithm. This is
 * explained in the snaplock_ticket class.
 *
 * \note
 * At this time there is one potential problem that can arise: the
 * lock may fail to concretize because the computer to which you
 * first sent the LOCK message goes down in some way. The other
 * snaplock computers will have no clue that the lock was being
 * worked on, by which computer, and whether one of them should
 * take over. One way to remediate is to run one instance of
 * snaplock on each computer on which a lock is likely to happen.
 *
 * \warning
 * The LOCK mechanism uses the system clock of each computer to know when
 * a lock times out. You are responsible for making sure that all those
 * computers have a synchronized clocked (i.e. run a timed daemon.)
 * The difference in time should be as small as possible. The precision
 * required by snaplock is around 1 second.
 *
 * \sa snaplock_ticket
 */



/** \brief Initializes a snaplock object.
 *
 * This function parses the command line arguments, reads configuration
 * files, setup the logger.
 *
 * It also immediately executes a --help or a --version command line
 * option and exits the process if these are present.
 *
 * \param[in] argc  The number of arguments in the argv array.
 * \param[in] argv  The array of argument strings.
 *
 */
snaplock::snaplock(int argc, char * argv[])
    : f_opt( argc, argv, g_snaplock_options, g_configuration_files, nullptr )
{
    // --help
    if( f_opt.is_defined( "help" ) )
    {
        usage(advgetopt::getopt::no_error);
        snap::NOTREACHED();
    }

    // --version
    if(f_opt.is_defined("version"))
    {
        std::cerr << SNAPWEBSITES_VERSION_STRING << std::endl;
        exit(1);
        snap::NOTREACHED();
    }

    // read the configuration file
    //
    f_config.read_config_file( f_opt.get_string("config").c_str() );

    // --debug
    f_debug = f_opt.is_defined("debug");

    // --server-name (mandatory)
    f_server_name = f_opt.get_string("server-name").c_str();

    // --connect (mandatory)
    tcp_client_server::get_addr_port(f_opt.get_string("connect").c_str(), f_communicator_addr, f_communicator_port, "tcp");

    // setup the logger: --nolog, --logfile, or config file log_config
    //
    if(f_opt.is_defined("nolog"))
    {
        snap::logging::configure_console();
    }
    else if(f_opt.is_defined("logfile"))
    {
        snap::logging::configure_logfile(QString::fromUtf8(f_opt.get_string( "logfile" ).c_str()));
    }
    else
    {
        if(f_config.contains("log_config"))
        {
            // use .conf definition when available
            f_log_conf = f_config["log_config"];
        }
        snap::logging::configure_conffile(f_log_conf);
    }

    if(f_debug)
    {
        // Force the logger level to DEBUG
        // (unless already lower)
        //
        snap::logging::reduce_log_output_level( snap::logging::log_level_t::LOG_LEVEL_DEBUG );
    }

    // offer the user to setup the maximum number of pending connections
    // from services that want to connect to Cassandra (this is only
    // the maximum number of "pending" connections and not the total
    // number of acceptable connections)
    //
    if(f_config.contains("max_pending_connections"))
    {
        QString const max_connections(f_config["max_pending_connections"]);
        if(!max_connections.isEmpty())
        {
            bool ok;
            f_max_pending_connections = max_connections.toLong(&ok);
            if(!ok)
            {
                SNAP_LOG_FATAL("invalid max_pending_connections, a valid number was expected instead of \"")(max_connections)("\".");
                exit(1);
            }
            if(f_max_pending_connections < 1)
            {
                SNAP_LOG_FATAL("max_pending_connections must be positive, \"")(max_connections)("\" is not valid.");
                exit(1);
            }
        }
    }

    // make sure there are no standalone parameters
    if( f_opt.is_defined( "--" ) )
    {
        std::cerr << "error: unexpected parameter found on daemon command line." << std::endl;
        usage(advgetopt::getopt::error);
    }
}


/** \brief Do some clean ups.
 *
 * At this point, the destructor is present mainly because we have
 * some virtual functions.
 */
snaplock::~snaplock()
{
}


/** \brief Print out usage and exit with 1.
 *
 * This function prints out the usage of the snaplock daemon and
 * then it exits.
 *
 * \param[in] status  The reason why the usage is bring printed: error
 *                    and no_error are currently supported.
 */
void snaplock::usage(advgetopt::getopt::status_t status)
{
    f_opt.usage( status, "snaplock" );
    exit(1);
}


/** \brief Run the snaplock daemon.
 *
 * This function is the core function of the daemon. It runs the loop
 * used to lock processes from any number of computers that have access
 * to the snaplock daemon network.
 */
void snaplock::run()
{
    // Stop on these signals, log them, then terminate.
    //
    // Note: the handler uses the logger which the create_instance()
    //       initializes
    //
    signal( SIGSEGV, snaplock::sighandler );
    signal( SIGBUS,  snaplock::sighandler );
    signal( SIGFPE,  snaplock::sighandler );
    signal( SIGILL,  snaplock::sighandler );

    // initialize the communicator and its connections
    //
    f_communicator = snap::snap_communicator::instance();

    // create a messager to communicate with the Snap Communicator process
    // and snapinit as required
    //
    f_messager.reset(new snaplock_messager(this, f_communicator_addr.toUtf8().data(), f_communicator_port));
    f_communicator->add_connection(f_messager);

    // now run our listening loop
    //
    f_communicator->run();
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
 * The signals are setup after the construction of the snaplock
 * object because that is where we initialize the logger.
 *
 * \param[in] sig  The signal that was just emitted by the OS.
 */
void snaplock::sighandler( int sig )
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


/** \brief Return the number of known computers running snaplock.
 *
 * This function is used by the snaplock_ticket objects to calculate
 * the quorum so as to know how many computers need to reply to
 * our messages before we can be sure we got the correct
 * results.
 *
 * \return The number of instances of snaplock running and connected.
 */
int snaplock::get_computer_count() const
{
    return f_computers.size();
}


/** \brief Calculate the quorum number of computers.
 *
 * This function dynamically recalculates the QUORUM that is required
 * to make sure that a value is valid between all the running computers.
 *
 * Because the network can go up and down (i.e. clashes, busy, etc.)
 * the time it takes to get an answer from a computer can be really
 * high. This is generally not acceptable when attempting to do a
 * lock as quickly as possible (i.e. low microseconds).
 *
 * The way to avoid having to wait for all the computers to answer is
 * to use the quorum number of computers which is a little more than
 * half:
 *
 * \f[
 *      {number of computers} over 2 + 1
 * \f]
 *
 * So if you using 4 or 5 computers for the lock, we need an answer
 * from 3 computers to make sure that we have the correct value.
 *
 * As computers running snaplock appear and disappear, the quorum
 * number will change, dynamically.
 */
int snaplock::quorum() const
{
    return f_computers.size() / 2 + 1;
}


/** \brief Process a message received from Snap! Communicator.
 *
 * This function gets called whenever the Snap! Communicator sends
 * us a message. This includes the READY and HELP commands, although
 * the most important one is certainly the STOP command.
 *
 * \param[in] message  The message we just received.
 */
void snaplock::process_message(snap::snap_communicator_message const & message)
{
    SNAP_LOG_TRACE("received messager message [")(message.to_message())("] for ")(f_server_name);

    QString const command(message.get_command());

    switch(command[0].unicode())
    {
    case 'A':
        if(command == "ADDTICKET")
        {
            // return our maximum ticket to the requester
            add_ticket(message);
            return;
        }
        break;

    case 'D':
        if(command == "DIED")
        {
            lockgone(message);
            return;
        }
        if(command == "DROPTICKET")
        {
            // return our maximum ticket to the requester
            drop_ticket(message);
            return;
        }
        break;

    case 'G':
        if(command == "GETMAXTICKET")
        {
            // return our maximum ticket to the requester
            get_max_ticket(message);
            return;
        }
        break;

    case 'H':
        if(command == "HELP")
        {
            // Snap! Communicator is asking us about the commands that we support
            //
            snap::snap_communicator_message reply;
            reply.set_command("COMMANDS");

            // list of commands understood by service
            // (many are considered to be internal commands... users
            // should look at the LOCK and UNLOCK messages only)
            //
            reply.add_parameter("list", "ADDTICKET,DIED,DROPTICKET,GETMAXTICKET,HELP,LOCK,LOCKENTERED,LOCKENTERING,LOCKEXITING,LOCKREADY,LOG,MAXTICKET,QUITTING,READY,STOP,TICKETADDED,UNKNOWN,UNLOCK");

            f_messager->send_message(reply);
            return;
        }
        break;

    case 'L':
        if(command.length() >= 3
        && command[1].unicode() == 'O')
        {
            if(command.length() >= 4
            && command[2].unicode() == 'C'
            && command[3].unicode() == 'K')
            {
                if(command.length() == 4) // command == "LOCK"
                {
                    // a client is asking for a LOCK...
                    //
                    lock(message);
                    return;
                }
                else
                {
                    switch(command[4].unicode())
                    {
                    case 'E':
                        if(command == "LOCKENTERED")
                        {
                            // once we received a QUORUM of LOCKENTERED commands we can
                            // move forward with getting the largest ticket number
                            //
                            lockentered(message);
                            return;
                        }

                        if(command == "LOCKENTERING")
                        {
                            // we are preparing to instantiate a ticket
                            //
                            lockentering(message);
                            return;
                        }

                        if(command == "LOCKEXITING")
                        {
                            // we entered the ticket loop, now we can release the
                            // entering objects
                            //
                            lockexiting(message);
                            return;
                        }
                        break;

                    case 'R':
                        if(command == "LOCKREADY")
                        {
                            // we or another snaplock is saying that it is ready
                            //
                            lockready(message);
                            return;
                        }
                        break;

                    }
                }
            }
            else if(command.length() == 3
                 && command[2].unicode() == 'G') // command == "LOG"
            {
                // logrotate just rotated the logs, we have to reconfigure
                //
                SNAP_LOG_INFO("Logging reconfiguration.");
                snap::logging::reconfigure();
                return;
            }
        }
        break;

    case 'M':
        if(command == "MAXTICKET")
        {
            // return our maximum ticket to the requester
            max_ticket(message);
            return;
        }
        break;

    case 'Q':
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
        break;

    case 'R':
        if(command == "READY")
        {
            ready();
            return;
        }
        break;

    case 'S':
        if(command == "STOP")
        {
            // Someone is asking us to leave (probably snapinit)
            //
            stop(false);
            return;
        }
        break;

    case 'T':
        if(command == "TICKETADDED")
        {
            // the addition of a ticket worked
            //
            ticket_added(message);
            return;
        }
        break;

    case 'U':
        if(command == "UNLOCK")
        {
            // a client is done with a lock and asking to UNLOCK...
            //
            unlock(message);
            return;
        }

        if(command == "UNKNOWN")
        {
            // we sent a command that Snap! Communicator did not understand
            //
            SNAP_LOG_ERROR("we sent unknown command \"")(message.get_parameter("command"))("\" and probably did not get the expected result.");
            return;
        }
        break;

    }

    // unknown commands get reported and process goes on
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


/** \brief We are now registered with snapcommunicator.
 *
 * This function let the local snapinit process know that we are
 * ready ("SAFE").
 *
 * Further, it broadcasts a LOCKREADY event to all the other snaplock
 * process (and "unfortunately" to self too.) This message allows us
 * to know how many snaplock processes are running as we register
 * each one of them while doing so.
 */
void snaplock::ready()
{
    // Snap! Communicator received our REGISTER command
    //

    // send a message to the snapinit service letting it know
    // that it can now start processes that require the snap lock
    //
    snap::snap_communicator_message dbready_message;
    dbready_message.set_command("SAFE");
    dbready_message.set_service("snapinit");
    dbready_message.add_parameter("name", "snaplock");
    f_messager->send_message(dbready_message);

    // tell other snaplock instances that are already listening that
    // we are ready; this way we can calculate the number of computers
    // available in our ring and use that to calculate the QUORUM
    //
    snap::snap_communicator_message lockready_message;
    lockready_message.set_command("LOCKREADY");
    lockready_message.set_service("*");
    lockready_message.add_parameter("server_name", f_server_name);
    lockready_message.add_parameter("pid", getpid());
    f_messager->send_message(lockready_message);
}


/** \brief Called whenever we receive the STOP command or equivalent.
 *
 * This function makes sure the snaplock exits as quickly as
 * possible.
 *
 * \li Marks the messager as done.
 * \li UNREGISTER from snapcommunicator.
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
void snaplock::stop(bool quitting)
{
    if(f_messager)
    {
        f_messager->mark_done();

        // unregister if we are still connected to the messager
        // and Snap! Communicator is not already quitting
        //
        if(!quitting)
        {
            snap::snap_communicator_message cmd;
            cmd.set_command("UNREGISTER");
            cmd.add_parameter("service", "snaplock");
            f_messager->send_message(cmd);
        }
    }
}


/** \brief Try to get a set of parameters.
 *
 * This function attempts to get the specified set of parameters from the
 * specified message.
 *
 * The function throws if a parameter is missing or invalid (i.e. an
 * integer is not valid.)
 *
 * \note
 * The timeout parameter is always viewed as optional. It is set to
 * "now + DEFAULT_TIMEOUT" if undefined in the message. If specified in
 * the message, there is no minimum or maximum (i.e. it may already
 * have timed out.)
 *
 * \param[in] message  The message from which we get parameters.
 * \param[out] object_name  A pointer to a QString that receives the object name.
 * \param[out] client_pid  A pointer to a pid_t that receives the client pid.
 * \param[out] timeout  A pointer to an int64_t that receives the timeout date in seconds.
 *
 * \return true if all specified parameters could be retrieved.
 */
void snaplock::get_parameters(snap::snap_communicator_message const & message, QString * object_name, pid_t * client_pid, time_t * timeout, QString * key)
{
    // get the "object name" (what we are locking)
    // in Snap, the object name is often a URI plus the action we are performing
    //
    if(object_name != nullptr)
    {
        *object_name = message.get_parameter("object_name");
        if(object_name->isEmpty())
        {
            // name missing
            //
            throw snap::snap_communicator_invalid_message("snaplock::get_parameters(): Invalid object name. We cannot lock the empty string.");
        }
    }

    // get the pid (process identifier) of the process that is
    // requesting the lock; this is important to be able to distinguish
    // multiple processes on the same computer requesting a lock
    //
    if(client_pid != nullptr)
    {
        *client_pid = message.get_integer_parameter("pid");
        if(*client_pid < 1)
        {
            // invalid pid
            //
            throw snap::snap_communicator_invalid_message(QString("snaplock::get_parameters(): Invalid pid specified for a lock (%1). It must be a positive decimal number.").arg(message.get_parameter("pid")).toUtf8().data());
        }
    }

    // get the time limit we will wait up to before we decide we
    // cannot obtain that lock
    //
    if(timeout != nullptr)
    {
        if(message.has_parameter("timeout"))
        {
            // this timeout may already be out of date in which case
            // the lock will automatically fail
            //
            *timeout = message.get_integer_parameter("timeout");
        }
        else
        {
            *timeout = time(nullptr) + DEFAULT_TIMEOUT;
        }
    }

    // get the key of a ticket or entering object
    //
    if(key != nullptr)
    {
        *key = message.get_parameter("key");
        if(key->isEmpty())
        {
            // key missing
            //
            throw snap::snap_communicator_invalid_message("snaplock::get_parameters(): A key cannot be an empty string.");
        }
    }
}


/** \brief Lock the resource.
 *
 * This function locks the specified resource \p object_name. It returns
 * when the resource is locked or when the lock timeout is reached.
 *
 * See the snaplock_ticket class for more details about the locking
 * mechanisms (algorithm and MSC implementation).
 *
 * Note that if lock() is called with an empty string then the function
 * unlocks the lock and returns immediately with false. This is equivalent
 * to calling unlock().
 *
 * \note
 * The function reloads all the parameters (outside of the table) because
 * we need to support a certain amount of dynamism. For example, an
 * administrator may want to add a new host on the system. In that case,
 * the list of host changes and it has to be detected here.
 *
 * \warning
 * The object name is left available in the lock table. Do not use any
 * secure/secret name/word, etc. as the object name.
 *
 * \bug
 * At this point there is no proper protection to recover from errors
 * that would happen while working on locking this entry. This means
 * failures may result in a lock that never ends.
 *
 * \param[in] message  The lock message.
 *
 * \return true if the lock was successful, false otherwise.
 *
 * \sa unlock()
 */
void snaplock::lock(snap::snap_communicator_message const & message)
{
    QString object_name;
    pid_t client_pid(0);
    time_t timeout(0);
    get_parameters(message, &object_name, &client_pid, &timeout, nullptr);

    if(timeout <= time(nullptr))
    {
        SNAP_LOG_WARNING("Lock on \"")(object_name)("\" / \"")(client_pid)("\" timed out before we could start the locking process.");
        return;
    }

    // create an entering key
    //
    QString const entering_key(QString("%1/%2").arg(f_server_name).arg(client_pid));

    snaplock_ticket::pointer_t ticket(std::make_shared<snaplock_ticket>(
                                        this,
                                        f_messager,
                                        object_name,
                                        entering_key,
                                        timeout,
                                        message.get_sent_from_server(),
                                        message.get_sent_from_service()));

    f_entering_tickets[object_name][entering_key] = ticket;

    ticket->entering();
}


/** \brief Unlock the resource.
 *
 * This function unlocks the resource specified in the call to lock().
 *
 * \param[in] message  The unlock message.
 *
 * \sa lock()
 */
void snaplock::unlock(snap::snap_communicator_message const & message)
{
    QString object_name;
    pid_t client_pid(0);
    get_parameters(message, &object_name, &client_pid, nullptr, nullptr);

    // make sure this ticket exists
    auto const obj_ticket(f_tickets.find(object_name));
    if(obj_ticket != f_tickets.end())
    {
        QString const entering_key(QString("%1/%2").arg(f_server_name).arg(client_pid));
        for(auto key_ticket : obj_ticket->second)
        {
            if(key_ticket.second->get_entering_key() == entering_key)
            {
                key_ticket.second->drop_ticket();
                break;
            }
        }
    }
}


///** \brief Remove a ticket we are done with (i.e. unlocked).
// *
// * This command drops the specified ticket (object_name).
// *
// * \param[in] message  The entering message.
// */
//void snaplock::dropticket(snap::snap_communicator_message const & message)
//{
//    QString object_name;
//    QString key;
//    get_parameters(message, &object_name, nullptr, nullptr, &key);
//
//    auto obj_ticket(f_tickets.find(object_name));
//    if(obj_ticket != f_tickets.end())
//    {
//        auto key_ticket(obj_ticket->second.find(key));
//        if(key_ticket != obj_ticket->second.end())
//        {
//            obj_ticket->second.erase(key_ticket);
//            if(obj_ticket->second.empty())
//            {
//                f_tickets.erase(obj_ticket);
//            }
//
//SNAP_LOG_WARNING("dropped ticket so check for next activation...");
//            // one ticket was erased, another may be first now
//            //
//            activate_first_lock(object_name);
//        }
//    }
//}


/** \brief Remove a ticket we are done with (i.e. unlocked).
 *
 * This command drops the specified ticket (object_name).
 *
 * \param[in] message  The entering message.
 */
void snaplock::lockentering(snap::snap_communicator_message const & message)
{
    QString object_name;
    time_t timeout(0);
    QString key;
    get_parameters(message, &object_name, nullptr, &timeout, &key);

    // the server_name and client_pid never include a slash so using
    // such as separators is safe
    //
    if(timeout > time(nullptr)) // still in the future?
    {
        // the entering is just a flag (i.e. entering[i] = true)
        // in our case the existance of a ticket is enough to know
        // that we entered
        //
        bool allocate(true);
        auto const obj_ticket(f_entering_tickets.find(object_name));
        if(obj_ticket != f_entering_tickets.end())
        {
            auto const key_ticket(obj_ticket->second.find(key));
            allocate = key_ticket == obj_ticket->second.end();
        }
        if(allocate)
        {
            // ticket does not exist, so create it now
            // (note: ticket should only exist on originator)
            //
            f_entering_tickets[object_name][key] = std::make_shared<snaplock_ticket>(
                                      this
                                    , f_messager
                                    , object_name
                                    , key
                                    , timeout
                                    , ""
                                    , "");
        }

        snap::snap_communicator_message reply;
        reply.set_command("LOCKENTERED");
        reply.reply_to(message);
        reply.add_parameter("object_name", object_name);
        reply.add_parameter("key", key);
        f_messager->send_message(reply);
    }

    cleanup();
}


/** \brief Tell all the tickets that we received a LOCKENTERED message.
 *
 * This function calls all the tickets entered() function which
 * process the LOCKENTERED message.
 *
 * We pass the key and "our ticket" number along so it can actually
 * create the ticket if required.
 *
 * \param[in] message  The LOCKENTERED message.
 */
void snaplock::lockentered(snap::snap_communicator_message const & message)
{
    QString object_name;
    QString key;
    get_parameters(message, &object_name, nullptr, nullptr, &key);

    auto const obj_entering_ticket(f_entering_tickets.find(object_name));
    if(obj_entering_ticket != f_entering_tickets.end())
    {
        auto const key_entering_ticket(obj_entering_ticket->second.find(key));
        if(key_entering_ticket != obj_entering_ticket->second.end())
        {
            key_entering_ticket->second->entered();
        }
    }
}


/** \brief Remove a ticket we are done with (i.e. unlocked).
 *
 * This command drops the specified ticket (object_name).
 *
 * \param[in] message  The entering message.
 */
void snaplock::lockexiting(snap::snap_communicator_message const & message)
{
    QString object_name;
    QString key;
    get_parameters(message, &object_name, nullptr, nullptr, &key);

    // when exiting we just remove the entry with that key
    //
    auto const obj_entering(f_entering_tickets.find(object_name));
    if(obj_entering != f_entering_tickets.end())
    {
        auto const key_entering(obj_entering->second.find(key));
        if(key_entering != obj_entering->second.end())
        {
            obj_entering->second.erase(key_entering);

            // we also want to remove it from the ticket f_entering
            // map if it is there (older ones are there!)
            //
            bool run_activation(false);
            auto const obj_ticket(f_tickets.find(object_name));
            if(obj_ticket != f_tickets.end())
            {
                for(auto key_ticket : obj_ticket->second)
                {
                    key_ticket.second->remove_entering(key);
                    run_activation = true;
                }
            }
            if(run_activation)
            {
                // try to activate the lock right now since it could
                // very well be the only ticket and that is exactly
                // when it is viewed as active!
                //
                activate_first_lock(object_name);
            }
        }

        if(obj_entering->second.empty())
        {
            f_entering_tickets.erase(obj_entering);
        }
    }
}


/** \brief Called whenever a snaplock computer is acknowledging itself.
 *
 * This function gets called on a LOCKREADY event which is sent whenever
 * a snaplock process is initialized on a computer.
 *
 * The message is expected to include the computer name and the process
 * PID (we also use a PID in case someone was to start two instances
 * on the same computer, otherwise it could cause problems.)
 *
 * \param[in] message  The LOCKREADY message.
 */
void snaplock::lockready(snap::snap_communicator_message const & message)
{
    pid_t snaplock_pid;
    get_parameters(message, nullptr, &snaplock_pid, nullptr, nullptr);

    // get the "object name" (what we are locking)
    // in Snap, the object name is often a URI plus the action we are performing
    //
    QString const server_name(message.get_parameter("server_name"));
    if(server_name.isEmpty())
    {
        // name missing
        //
        throw snap::snap_communicator_invalid_message("snaplock::lockready(): Invalid server name (empty).");
    }

    QString const snaplock_key(QString("%1/%2").arg(server_name).arg(snaplock_pid));
    f_computers[snaplock_key] = true;
}


/** \brief Called whenever a service managed by snapinit dies.
 *
 * This function is used to know that a service died as per
 * snapinit. (i.e. a crash or a normal STOP was sent to the
 * service.)
 *
 * This allows us to manage the f_computers list of computers running
 * snaplock.
 *
 * \param[in] message  The LOCKREADY message.
 */
void snaplock::lockgone(snap::snap_communicator_message const & message)
{
    // was is a snaplock service at least?
    QString const service_name(message.get_parameter("service"));
    if(service_name != "snaplock")
    {
        return;
    }

    // get the pid
    pid_t snaplock_pid;
    get_parameters(message, nullptr, &snaplock_pid, nullptr, nullptr);

    // TODO: replace the service/server name with the message remote IP
    QString const snaplock_key(QString("%1/%2").arg(service_name).arg(snaplock_pid));
    auto it(f_computers.find(snaplock_key));
    if(it != f_computers.end())
    {
        f_computers.erase(it);
    }
}


/** \brief Make sure the very first ticket is marked as LOCKED.
 *
 * This function is called whenever the f_tickets map changes
 * (more specifically, one of its children) to make sure
 * that the first ticket is clearly marked as being locked.
 *
 * Note that the function may be called many times when the
 * first ticket does not actually change. This is fine because
 * the ticket activate_lock() function can safely be called any
 * number of times.
 *
 * \param[in] object_name  The name of the object which very
 *                         first ticket may have changed.
 */
void snaplock::activate_first_lock(QString const & object_name)
{
    auto const obj_ticket(f_tickets.find(object_name));
    if(obj_ticket != f_tickets.end()
    && !obj_ticket->second.empty())
    {
        // there is a ticket that should be active
        //
SNAP_LOG_WARNING("activate first lock called and we found a first ticket...");
        obj_ticket->second.begin()->second->activate_lock();
    }
}


/** \brief Clean timed out entries if any.
 *
 * This function goes through the list of tickets and entering
 * entries and removes any one of them that timed out. This is
 * important if a process dies and does not properly remove
 * its lock.
 */
void snaplock::cleanup()
{
    // remove any f_tickets that timed out
    //
    for(auto obj_ticket(f_tickets.begin()); obj_ticket != f_tickets.end(); )
    {
        bool try_activate(false);
        for(auto key_ticket(obj_ticket->second.begin()); key_ticket != obj_ticket->second.end(); )
        {
            if(key_ticket->second->timed_out())
            {
                key_ticket->second->lock_failed();
                key_ticket = obj_ticket->second.erase(key_ticket);
                try_activate = true;
            }
            else
            {
                ++key_ticket;
            }
        }

        if(obj_ticket->second.empty())
        {
            obj_ticket = f_tickets.erase(obj_ticket);
        }
        else
        {
            if(try_activate)
            {
                // something was erased, a new ticket may be first
                //
                activate_first_lock(obj_ticket->first);
            }

            ++obj_ticket;
        }
    }

    // remove any f_entering that timed out
    //
    for(auto obj_entering(f_entering_tickets.begin()); obj_entering != f_entering_tickets.end(); )
    {
        for(auto key_entering(obj_entering->second.begin()); key_entering != obj_entering->second.end(); )
        {
            if(key_entering->second->timed_out())
            {
                key_entering = obj_entering->second.erase(key_entering);
            }
            else
            {
                ++key_entering;
            }
        }

        if(obj_entering->second.empty())
        {
            obj_entering = f_entering_tickets.erase(obj_entering);
        }
        else
        {
            ++obj_entering;
        }
    }
}


/** \brief One of the snaplock process asked for us to drop a ticket.
 *
 * This function searches for the specified ticket and removes it from
 * the system.
 *
 * If the specified ticket does not exist, nothing happens.
 *
 * \warning
 * The DROPTICKET even receives either the ticket key (if available)
 * or the entering key (when the ticket key was not yet available.)
 * Note that the ticket key should always exists by now, but just
 * in case we do so in order to be able to drop a ticket at any one
 * time.
 *
 * \param[in] message  The message we just received.
 */
void snaplock::drop_ticket(snap::snap_communicator_message const & message)
{
    QString object_name;
    QString key;
    get_parameters(message, &object_name, nullptr, nullptr, &key);

    snap::snap_string_list const segments(key.split('/'));

    // drop the regular ticket
    //
    // if we have only 2 segments, then there is no corresponding ticket
    // since tickets are added only once we have a ticket_id
    //
    QString entering_key;
    if(segments.size() == 3)
    {
        auto obj_ticket(f_tickets.find(object_name));
        if(obj_ticket != f_tickets.end())
        {
            auto key_ticket(obj_ticket->second.find(key));
            if(key_ticket != obj_ticket->second.end())
            {
                obj_ticket->second.erase(key_ticket);
            }

            if(obj_ticket->second.empty())
            {
                f_tickets.erase(obj_ticket);
            }

            // one ticket was erased, another may be first now
            //
            activate_first_lock(object_name);
        }

        // we received the ticket_id in the message, so
        // we have to regenerate the entering_key without
        // the ticket_id (which is the first element)
        //
        entering_key = QString("%1/%2").arg(segments[1]).arg(segments[2]);
    }
    else
    {
        // we received the entering_key in the message, use as is
        entering_key = key;
    }

    // drop the entering ticket
    //
    auto obj_entering_ticket(f_entering_tickets.find(object_name));
    if(obj_entering_ticket != f_entering_tickets.end())
    {
        auto key_entering_ticket(obj_entering_ticket->second.find(entering_key));
        if(key_entering_ticket != obj_entering_ticket->second.end())
        {
            obj_entering_ticket->second.erase(key_entering_ticket);
        }

        if(obj_entering_ticket->second.empty())
        {
            f_entering_tickets.erase(obj_entering_ticket);
        }
    }
}


/** \brief Search for the largest ticket.
 *
 * This function searches the list of tickets for the largest one
 * and returns that number.
 *
 * \return The largest ticket number that currently exist in the list
 *         of tickets.
 */
void snaplock::get_max_ticket(snap::snap_communicator_message const & message)
{
    QString object_name;
    QString key;
    get_parameters(message, &object_name, nullptr, nullptr, &key);

    // remove any f_tickets that timed out by now because these should
    // not be taken in account in the max. computation
    //
    cleanup();

    // determine last ticket defined in this snaplock
    //
    // note that we do not need to check the f_entering_tickets list
    // since that one does not yet have any ticket and thus max there
    // would return 0 every time
    //
    snaplock_ticket::ticket_id_t last_ticket(0);
    auto obj_ticket(f_tickets.find(object_name));
    if(obj_ticket != f_tickets.end())
    {
        for(auto key_ticket(obj_ticket->second.begin()); key_ticket != obj_ticket->second.end(); ++key_ticket)
        {
            snaplock_ticket::ticket_id_t const ticket_number(key_ticket->second->get_ticket_number());
            if(ticket_number > last_ticket)
            {
                last_ticket = ticket_number;
            }
        }
    }

    snap::snap_communicator_message reply;
    reply.set_command("MAXTICKET");
    reply.reply_to(message);
    reply.add_parameter("object_name", object_name);
    reply.add_parameter("key", key);
    reply.add_parameter("ticket_id", last_ticket);
    f_messager->send_message(reply);
}


/** \brief Search for the largest ticket.
 *
 * This function searches the list of tickets for the largest one
 * and returns that number.
 *
 * \return The largest ticket number that currently exist in the list
 *         of tickets.
 */
void snaplock::max_ticket(snap::snap_communicator_message const & message)
{
    QString object_name;
    QString key;
    get_parameters(message, &object_name, nullptr, nullptr, &key);

    // the MAXTICKET is an answer that has to go in a still un-added ticket
    //
    auto const obj_entering_ticket(f_entering_tickets.find(object_name));
    if(obj_entering_ticket != f_entering_tickets.end())
    {
        auto const key_entering_ticket(obj_entering_ticket->second.find(key));
        if(key_entering_ticket != obj_entering_ticket->second.end())
        {
            key_entering_ticket->second->max_ticket(message.get_integer_parameter("ticket_id"));
        }
    }
}


/** \brief Add a ticket from another snaplock.
 *
 * Tickets get duplicated on all the available snaplock.
 *
 * \note
 * Although we only need a QUORUM number of nodes to receive a copy of
 * the data, the data still get broadcast to all the snaplock objects.
 * After this message arrives any one of the snaplock process can
 * handle the unlock if the UNLOCK message gets sent to another process
 * instead of the one which first created the ticket. This is the point
 * of the implementation since we want to be tolerant (as in if one of
 * the computers go down, the locking mechanism still works.)
 */
void snaplock::add_ticket(snap::snap_communicator_message const & message)
{
    QString object_name;
    QString key;
    time_t timeout;
    get_parameters(message, &object_name, nullptr, &timeout, &key);

#ifdef _DEBUG
    {
        auto const obj_ticket(f_tickets.find(object_name));
        if(obj_ticket != f_tickets.end())
        {
            auto const key_ticket(obj_ticket->second.find(key));
            if(key_ticket != obj_ticket->second.end())
            {
                // this ticket exists on this system, so ignore
                //
                throw std::logic_error("snaplock::add_ticket() ticket already exists");
            }
        }
    }
#endif

    // the client_pid parameter is part of the key (3rd segment)
    //
    snap::snap_string_list const segments(key.split('/'));
    if(segments.size() < 3)
    {
        throw std::logic_error("snaplock_ticket::add_ticket() called with an invalid ticket key (not enough segments).");
    }
    //bool ok(false);
    //pid_t const client_pid(segments[2].toInt(&ok, 10));
    //if(!ok)
    //{
    //    throw std::logic_error("snaplock_ticket::add_ticket() called with an invalid ticket key (client_pid is not a number).");
    //}

    // by now all existing snaplock instances should already have
    // an entering ticket for that one ticket
    //
    auto const obj_entering_ticket(f_entering_tickets.find(object_name));
    if(obj_entering_ticket == f_entering_tickets.end())
    {
        throw std::logic_error("entering ticket for object_name not found in add_ticket()");
    }
    // the key we need to search is not the new ticket key but the
    // entering key, build it from the segments
    //
    QString const entering_key(QString("%1/%2").arg(segments[1]).arg(segments[2]));
    auto const key_entering_ticket(obj_entering_ticket->second.find(entering_key));
    if(key_entering_ticket == obj_entering_ticket->second.end())
    {
        throw std::logic_error("entering ticket for object_name.key not found in add_ticket()");
    }

    // make it an official ticket now
    //
    // this should happen on all snaplock other than the one that
    // first received the LOCK message
    //
    f_tickets[object_name][key] = key_entering_ticket->second;
    //f_tickets[object_name][key] = std::make_shared<snaplock_ticket>(
    //                                    this,
    //                                    f_messager,
    //                                    object_name,
    //                                    f_server_name,
    //                                    client_pid,
    //                                    timeout,
    //                                    "",
    //                                    "");

    snap::snap_communicator_message ticket_added_message;
    ticket_added_message.set_command("TICKETADDED");
    ticket_added_message.reply_to(message);
    ticket_added_message.add_parameter("object_name", object_name);
    ticket_added_message.add_parameter("key", key);
    f_messager->send_message(ticket_added_message);
}


void snaplock::ticket_added(snap::snap_communicator_message const & message)
{
    QString object_name;
    QString key;
    get_parameters(message, &object_name, nullptr, nullptr, &key);

    auto const obj_ticket(f_tickets.find(object_name));
    if(obj_ticket != f_tickets.end())
    {
        auto const key_ticket(obj_ticket->second.find(key));
        if(key_ticket != obj_ticket->second.end())
        {
            // this ticket exists on this system, so ignore
            //
            auto const obj_entering_ticket(f_entering_tickets.find(object_name));
            if(obj_entering_ticket == f_entering_tickets.end())
            {
                // this should never happen because we should still have
                // our own f_entering in that map!
                //
                throw std::logic_error("snaplock_ticket::ticket_added() called with an object not present in f_entering_ticket.");
            }
            key_ticket->second->ticket_added(obj_entering_ticket->second);
        }
    }
}



// vim: ts=4 sw=4 et
