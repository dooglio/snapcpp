// Snap Websites Server -- install a websites in the database
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

#include "version.h"

#include <snapwebsites/log.h>
#include <snapwebsites/not_used.h>
#include <snapwebsites/snap_initialize_website.h>
#include <snapwebsites/snapwebsites.h>
#include <snapwebsites/snap_config.h>




namespace
{
    const std::vector<std::string> g_configuration_files; // Empty

    const advgetopt::getopt::option g_snapinstallwebsite_options[] =
    {
        {
            '\0',
            advgetopt::getopt::GETOPT_FLAG_SHOW_USAGE_ON_ERROR,
            nullptr,
            nullptr,
            "Usage: %p [-<opt>]",
            advgetopt::getopt::argument_mode_t::help_argument
        },
        {
            '\0',
            advgetopt::getopt::GETOPT_FLAG_SHOW_USAGE_ON_ERROR,
            nullptr,
            nullptr,
            "where -<opt> is one or more of:",
            advgetopt::getopt::argument_mode_t::help_argument
        },
        {
            'c',
            advgetopt::getopt::GETOPT_FLAG_ENVIRONMENT_VARIABLE | advgetopt::getopt::GETOPT_FLAG_SHOW_USAGE_ON_ERROR,
            "config",
            nullptr,
            "Configuration file to initialize snapdbproxy.",
            advgetopt::getopt::argument_mode_t::optional_argument
        },
        {
            '\0',
            advgetopt::getopt::GETOPT_FLAG_SHOW_USAGE_ON_ERROR,
            "domain",
            nullptr,
            "the domain and sub-domain for which a site is to be created (i.e. install.snap.website)",
            advgetopt::getopt::argument_mode_t::required_argument
        },
        {
            '\0',
            advgetopt::getopt::GETOPT_FLAG_SHOW_USAGE_ON_ERROR,
            "help",
            nullptr,
            "show this help output",
            advgetopt::getopt::argument_mode_t::no_argument
        },
        {
            '\0',
            advgetopt::getopt::GETOPT_FLAG_SHOW_USAGE_ON_ERROR,
            "port",
            nullptr,
            "the port used to access this website (usually 80 or 443)",
            advgetopt::getopt::argument_mode_t::required_argument
        },
        {
            '\0',
            advgetopt::getopt::GETOPT_FLAG_SHOW_USAGE_ON_ERROR,
            "version",
            nullptr,
            "show the version of the snapdb executable",
            advgetopt::getopt::argument_mode_t::no_argument
        },
        {
            '\0',
            0,
            nullptr,
            nullptr,
            nullptr,
            advgetopt::getopt::argument_mode_t::end_of_options
        }
    };


}
//namespace



int main(int argc, char * argv[])
{
    advgetopt::getopt opt(argc, argv, g_snapinstallwebsite_options, g_configuration_files, "SNAPINSTALLWEBSITE_OPTIONS");

    snap::logging::set_progname(argv[0]);
    snap::logging::configure_console();

    if(opt.is_defined("help"))
    {
        opt.usage(advgetopt::getopt::status_t::no_error, "snapdbproxy");
        exit(0);
        snap::NOTREACHED();
    }

    if(opt.is_defined("version"))
    {
        std::cout << SNAPDBPROXY_VERSION_STRING << std::endl;
        exit(0);
        snap::NOTREACHED();
    }

    if(!opt.is_defined("domain")
    || !opt.is_defined("port"))
    {
        std::cerr << "error: domain and port are both required." << std::endl;
        opt.usage(advgetopt::getopt::status_t::error, "snapdbproxy");
        exit(1);
        snap::NOTREACHED();
    }

    SNAP_LOG_INFO("Get snapserver info.");

    // read the snapserver IP:port information directly from the "snapserver"
    // configuration file
    //
    snap::snap_config config("snapserver");
    if(opt.is_defined("config"))
    {
        config.set_configuration_path(opt.get_string("config"));
    }
    config.set_parameter_default("listen", "127.0.0.1:4004");

    QString snap_host("127.0.0.1");
    int snap_port(4004);
    tcp_client_server::get_addr_port(config["listen"], snap_host, snap_port, "tcp");

    SNAP_LOG_INFO("snapserver is at ")(snap_host)(":")(snap_port)(".");

    // we need the URL:port to initialize the new website
    //
    //url, site_port
    QString const url(QString::fromUtf8(opt.get_string("domain").c_str()));
    if(url.isEmpty())
    {
        std::cerr << "error: domain cannot be empty and must be specified." << std::endl;
        exit(1);
        snap::NOTREACHED();
    }

    int const site_port(opt.get_long("port", 0, 0, 65535));

    SNAP_LOG_INFO("website is at ")(url)(":")(site_port)(".");

    // create a snap_initialize_website object and listen for messages
    // up until is_done() returns true
    //
    snap::snap_initialize_website::pointer_t initialize_website(std::make_shared<snap::snap_initialize_website>(snap_host, snap_port, url, site_port));

    SNAP_LOG_INFO("start website initializer.");

    if(!initialize_website->start_process())
    {
        SNAP_LOG_INFO("start_process() failed. Existing immediately.");
        exit(1);
        snap::NOTREACHED();
    }

    for(;;)
    {
        for(;;)
        {
            QString const status(initialize_website->get_status());
            if(status.isEmpty())
            {
                break;
            }
            SNAP_LOG_INFO(status);
        }

        if(initialize_website->is_done())
        {
            break;
        }

        // unfortunately, I do not have a non-poll version for this
        // one yet...
        //
        sleep(1);
    }

    return 0;
}


// vim: ts=4 sw=4 et
