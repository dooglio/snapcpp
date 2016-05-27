/*
 * Text:
 *      snapdbproxy_thread.cpp
 *
 * Description:
 *      Handle a thread running a TCP/IP connection used to communicate with
 *      Cassandra.
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
#include "snapdbproxy.h"

// our lib
//
#include "log.h"



/** \brief Initialize the thread.
 *
 * This constructor initializes the thread runner with the specified
 * session and socket.
 *
 * It creates a thread named "connection" with the thread runner attached.
 *
 * It also keeps a copy of the socket so it can close it using its
 * destructor.
 *
 * The thread is then started. If anything fails, except the start, you
 * are likely to get an exception. Otherwise the is_running() will
 * return false which means that you should not keep a copy of that
 * thread.
 */
snapdbproxy_thread::snapdbproxy_thread(QtCassandra::QCassandraSession::pointer_t session, int const s, QString const & cassandra_host_list, int cassandra_port)
    : f_connection(session, s, cassandra_host_list, cassandra_port)
    , f_thread("connection", &f_connection)
    , f_socket(s)
{
    if(!f_thread.start())
    {
        SNAP_LOG_FATAL("could not start connection thread to handle CQL proxying");
        // do not throw so that way we do not just kill the snapdbproxy daemon
        // also that way we reach the destructor which closes the socket
    }
}


/** \brief Clean up a thread.
 *
 * This function make sure to clean up a thread.
 *
 * First it makes sure that the thread was terminated and then it closes
 * the socket assiciated with this thread.
 */
snapdbproxy_thread::~snapdbproxy_thread()
{
    f_connection.kill();
    f_thread.stop();
    close(f_socket);
}


/** \brief Check whether the thread is still running.
 *
 * A thread may die. We do not currently get a signal of any sort when
 * that happens. Instead we use this function to know whether the
 * thread is still running or not.
 *
 * \return true if the thread is still running.
 */
bool snapdbproxy_thread::is_running() const
{
    return f_thread.is_running();
}



// vim: ts=4 sw=4 et