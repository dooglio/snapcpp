// Snap Websites Server -- snap websites server
// Copyright (C) 2011-2015  Made to Order Software Corp.
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

#include "snap_cassandra.h"
#include "snapwebsites.h"
#include "log.h"

#include <unistd.h>

#include <controlled_vars/controlled_vars.h>
#include <QtCassandra/QCassandra.h>

namespace snap
{


snap_cassandra::snap_cassandra( snap_config const& parameters )
    : f_parameters(parameters)
{
    // empty
}

void snap_cassandra::connect()
{
    // This function connects to the Cassandra database, but it doesn't
    // keep the connection. We are the server and the connection would
    // not be shared properly between all the children.
    f_cassandra_host = f_parameters["cassandra_host"];
    if(f_cassandra_host.isEmpty())
    {
        f_cassandra_host = "localhost";
    }
    //
    QString port_str( f_parameters["cassandra_port"] );
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

    // TODO:
    // We must stay "alive" waiting for the cassandra server to come up.
    // This takes entries into the f_parameters file:
    // wait_interval and wait_max_tries.
    //
    int wait_interval(f_parameters["wait_interval"].toInt());
    if(wait_interval < 5)
    {
        wait_interval = 5;
    }
    int wait_max_tries(f_parameters["wait_max_tries"].toInt());
    f_cassandra = QtCassandra::QCassandra::create();
    Q_ASSERT(f_cassandra);
    while( !f_cassandra->connect(f_cassandra_host, f_cassandra_port) )
    {
        // if wait_max_tries is 1 we're about to call exit(1) so we are not going
        // to retry once more
        if(wait_max_tries != 1)
        {
            SNAP_LOG_WARNING()
                << "The connection to the Cassandra server failed ("
                << f_cassandra_host << ":" << f_cassandra_port << "). "
                << "Try again in " << wait_interval << " secs.";
            sleep( wait_interval );
        }
        //
        if( wait_max_tries > 0 )
        {
            if( --wait_max_tries <= 0 )
            {
                SNAP_LOG_FATAL() << "TIMEOUT: Could not connect to remote Cassandra server at ("
                    << f_cassandra_host << ":" << f_cassandra_port << ")!";
                exit(1);
            }
        }
    }
}


void snap_cassandra::init_context()
{
    // create the context if it doesn't exist yet
    QtCassandra::QCassandraContext::pointer_t context( get_snap_context() );
    if( !context )
    {
        // create a new context
        QString const context_name(snap::get_name(snap::name_t::SNAP_NAME_CONTEXT));
        SNAP_LOG_INFO("Creating \"")(context_name)("\"...");
        context = f_cassandra->context(context_name);

        // this is the default for contexts, but just in case we were
        // to change that default at a later time...
        context->setDurableWrites(true);

        // TODO: add support for replications defined as a % so if we
        //       discover 10 nodes, we user 5 when replication is 50%
        //       (however, once set, we do not change this number...)
        int rep(3);
        QString replication(f_parameters["cassandra_replication"]);
        if(!replication.isEmpty())
        {
            bool ok(false);
            rep = replication.toInt(&ok);
            if(!ok)
            {
                SNAP_LOG_ERROR("unknown replication \"")(replication)("\", falling back to \"3\"");
                rep = 3;
                replication = "3";
            }
        }

        // for developers testing with a few nodes in a single data center,
        // SimpleStrategy is good enough; for anything larger ("a real
        // cluster",) it won't work right
        QString const strategy(f_parameters["cassandra_strategy"]);
        if(strategy == "simple")
        {
            context->setStrategyClass("org.apache.cassandra.locator.SimpleStrategy");

            // for simple strategy, use the replication_factor parameter
            // (see http://www.datastax.com/documentation/cql/3.0/cql/cql_reference/create_keyspace_r.html)
            context->setReplicationFactor(rep);
        }
        else
        {
            if(strategy == "local")
            {
                context->setStrategyClass("org.apache.cassandra.locator.LocalStrategy");
            }
            else
            {
                if(!strategy.isEmpty() && strategy != "network")
                {
                    SNAP_LOG_ERROR("unknown strategy \"")(strategy)("\", falling back to \"network\"");
                }
                context->setStrategyClass("org.apache.cassandra.locator.NetworkTopologyStrategy");
            }

            // here each data center gets a replication factor
            QString const data_centers(f_parameters["cassandra_data_centers"]);
            snap_string_list const names(data_centers.split(','));
            bool found(false);
            int const max_names(names.size());
            for(int idx(0); idx < max_names; ++idx)
            {
                // remove all spaces in each name
                QString const name(names[idx].simplified());
                if(!name.isEmpty())
                {
                    context->setDescriptionOption(name, replication);
                    found = true;
                }
            }
            if(!found)
            {
                SNAP_LOG_FATAL("the list of data centers is required when creating a context in a cluster which is not using \"simple\" as its strategy");
            }
        }
        context->create();
        // we don't put the tables in here so we can call the create_table()
        // and have the tables created as required (i.e. as we add new ones
        // they get added as expected, no need for special handling.)
    }
}


QtCassandra::QCassandraContext::pointer_t snap_cassandra::get_snap_context()
{
    if( !f_cassandra )
    {
        SNAP_LOG_FATAL() << "Must connect to cassandra first!";
        exit(1);
    }

    // we need to read all the contexts in order to make sure the
    // findContext() works
    f_cassandra->contexts();
    const QString context_name(snap::get_name(snap::name_t::SNAP_NAME_CONTEXT));
    return f_cassandra->findContext(context_name);
}


QString snap_cassandra::get_cassandra_host() const
{
    return f_cassandra_host;
}


int32_t snap_cassandra::get_cassandra_port() const
{
    return f_cassandra_port;
}


bool snap_cassandra::is_connected() const
{
    return f_cassandra->isConnected();
}


}
// namespace snap

// vim: ts=4 sw=4 et
