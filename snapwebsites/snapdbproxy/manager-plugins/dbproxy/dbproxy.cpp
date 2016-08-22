// Snap Websites Server -- manage the snapdbproxy settings
// Copyright (C) 2016  Made to Order Software Corp.
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

// dbproxy
//
#include "dbproxy.h"

// our lib
//
#include "snapmanager/form.h"

// snapwebsites lib
//
#include "join_strings.h"
#include "log.h"
#include "not_reached.h"
#include "not_used.h"
#include "qdomhelpers.h"
#include "qdomxpath.h"
#include "snap_cassandra.h"
#include "string_pathinfo.h"
#include "tokenize_string.h"

// Qt lib
//
#include <QFile>

// C lib
//
#include <sys/file.h>

// last entry
//
#include "poison.h"


SNAP_PLUGIN_START(dbproxy, 1, 0)


namespace
{

// TODO: offer the user a way to change this path?
//char const * g_service_filename = "/etc/snapwebsites/services.d/service-snapdbproxy.xml";

// TODO: get that path from the XML instead
char const * g_configuration_filename = "snapdbproxy";

// TODO: get that path from the XML instead and add the /snapwebsites.d/ part
char const * g_configuration_d_filename = "/etc/snapwebsites/snapwebsites.d/snapdbproxy.conf";


void file_descriptor_deleter(int * fd)
{
    if(close(*fd) != 0)
    {
        int const e(errno);
        SNAP_LOG_WARNING("closing file descriptor failed (errno: ")(e)(", ")(strerror(e))(")");
    }
}


} // no name namespace



/** \brief Get a fixed dbproxy plugin name.
 *
 * The dbproxy plugin makes use of different fixed names. This function
 * ensures that you always get the right spelling for a given name.
 *
 * \param[in] name  The name to retrieve.
 *
 * \return A pointer to the name.
 */
char const * get_name(name_t name)
{
    switch(name)
    {
    case name_t::SNAP_NAME_SNAPMANAGERCGI_DBPROXY_NAME:
        return "name";

    default:
        // invalid index
        throw snap_logic_exception("Invalid SNAP_NAME_SNAPMANAGERCGI_DBPROXY_...");

    }
    NOTREACHED();
}




/** \brief Initialize the dbproxy plugin.
 *
 * This function is used to initialize the dbproxy plugin object.
 */
dbproxy::dbproxy()
    //: f_snap(nullptr) -- auto-init
{
}


/** \brief Clean up the dbproxy plugin.
 *
 * Ensure the dbproxy object is clean before it is gone.
 */
dbproxy::~dbproxy()
{
}


/** \brief Get a pointer to the dbproxy plugin.
 *
 * This function returns an instance pointer to the dbproxy plugin.
 *
 * Note that you cannot assume that the pointer will be valid until the
 * bootstrap event is called.
 *
 * \return A pointer to the dbproxy plugin.
 */
dbproxy * dbproxy::instance()
{
    return g_plugin_dbproxy_factory.instance();
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
QString dbproxy::description() const
{
    return "Manage the snapdbproxy settings.";
}


/** \brief Return our dependencies.
 *
 * This function builds the list of plugins (by name) that are considered
 * dependencies (required by this plugin.)
 *
 * \return Our list of dependencies.
 */
QString dbproxy::dependencies() const
{
    return "|server|";
}


/** \brief Check whether updates are necessary.
 *
 * This function is ignored in snapmanager.cgi and snapmanagerdaemon plugins.
 *
 * \param[in] last_updated  The UTC Unix date when the website was last updated (in micro seconds).
 *
 * \return The UTC Unix date of the last update of this plugin.
 */
int64_t dbproxy::do_update(int64_t last_updated)
{
    NOTUSED(last_updated);

    SNAP_PLUGIN_UPDATE_INIT();
    // no updating in snapmanager*
    SNAP_PLUGIN_UPDATE_EXIT();
}


/** \brief Initialize dbproxy.
 *
 * This function terminates the initialization of the dbproxy plugin
 * by registering for different events.
 *
 * \param[in] snap  The child handling this request.
 */
void dbproxy::bootstrap(snap_child * snap)
{
    f_snap = dynamic_cast<snap_manager::manager *>(snap);
    if(f_snap == nullptr)
    {
        throw snap_logic_exception("snap pointer does not represent a valid manager object.");
    }

    SNAP_LISTEN(dbproxy, "server", snap_manager::manager, retrieve_status, _1);
}


/** \brief Determine this plugin status data.
 *
 * This function builds a tree of statuses.
 *
 * \param[in] server_status  The map of statuses.
 */
void dbproxy::on_retrieve_status(snap_manager::server_status & server_status)
{
    if(f_snap->stop_now_prima())
    {
        return;
    }

    {
        snap_config snap_dbproxy_conf(g_configuration_filename);

        snap_manager::status_t const host_list(
                      snap_manager::status_t::state_t::STATUS_STATE_INFO
                    , get_plugin_name()
                    , "cassandra_host_list"
                    , snap_dbproxy_conf["cassandra_host_list"]);
        server_status.set_field(host_list);
    }

    try
    {
        // if the connection fails, we cannot have either of the following
        // fields so the catch() makes sure to avoid them
        //
        snap::snap_cassandra cassandra;
        cassandra.connect();

        auto context(cassandra.get_snap_context());
        if(!context)
        {
            // no context yet, offer to create the context
            //
            snap_manager::status_t const create_context(
                          snap_manager::status_t::state_t::STATUS_STATE_INFO
                        , get_plugin_name()
                        , "cassandra_create_context"
                        , "create");
            server_status.set_field(create_context);
        }
        else
        {
            // database is available and context is available,
            // offer to create the tables (it should be automatic
            // though, but this way we can click on this one last
            // time before installing a website)
            //
            snap_manager::status_t const create_tables(
                          snap_manager::status_t::state_t::STATUS_STATE_INFO
                        , get_plugin_name()
                        , "cassandra_create_tables"
                        , "create");
            server_status.set_field(create_tables);
        }
    }
    catch(snap::snap_cassandra_not_available_exception const &)
    {
        // ignore completely, we just cannot connect at this time
    }
}



/** \brief Transform a value to HTML for display.
 *
 * This function expects the name of a field and its value. It then adds
 * the necessary HTML to the specified element to display that value.
 *
 * If the value is editable, then the function creates a form with the
 * necessary information (hidden fields) to save the data as required
 * by that field (i.e. update a .conf/.xml file, create a new file,
 * remove a file, etc.)
 *
 * \param[in] server_status  The map of statuses.
 * \param[in] s  The field being worked on.
 *
 * \return true if we handled this field.
 */
bool dbproxy::display_value(QDomElement parent, snap_manager::status_t const & s, snap::snap_uri const & uri)
{
    QDomDocument doc(parent.ownerDocument());

    if(s.get_field_name() == "cassandra_host_list")
    {
        // the list if frontend snapmanagers that are to receive statuses
        // of the cluster computers; may be just one computer; should not
        // be empty; shows a text input field
        //
        snap_manager::form f(
                  get_plugin_name()
                , s.get_field_name()
                , snap_manager::form::FORM_BUTTON_RESET | snap_manager::form::FORM_BUTTON_SAVE
                );

        snap_manager::widget_input::pointer_t field(std::make_shared<snap_manager::widget_input>(
                          "Cassandra Node IP Addresses:"
                        , s.get_field_name()
                        , s.get_value()
                        , "The list of <strong>comma separated</strong> IP addresses used to connect to Cassandra."
                         " In general these are seed nodes, although it does not need to be."
                         " The C++ Cassandra driver will adjust the information as"
                         " required and connect to additional nodes automatically."
                        ));
        f.add_widget(field);

        f.generate(parent, uri);

        return true;
    }

    if(s.get_field_name() == "cassandra_create_context")
    {
        // the list if frontend snapmanagers that are to receive statuses
        // of the cluster computers; may be just one computer; should not
        // be empty; shows a text input field
        //
        snap_manager::form f(
                  get_plugin_name()
                , s.get_field_name()
                , snap_manager::form::FORM_BUTTON_SAVE
                );

        snap_manager::widget_input::pointer_t field(std::make_shared<snap_manager::widget_input>(
                          "Create Snap! Websites Context:"
                        , s.get_field_name()
                        , s.get_value()
                        , "The Snap! Websites Server makes use of a Cassandra context named snap_websites."
                         " It looks like that context does not yet exist."
                         " To create it, just click on the Save button. The value of the field is currently ignored."
                         " Note: be patient. The creation of the context can take a bit of time..."
                        ));
        f.add_widget(field);

        f.generate(parent, uri);

        return true;
    }

    if(s.get_field_name() == "cassandra_create_tables")
    {
        // the list if frontend snapmanagers that are to receive statuses
        // of the cluster computers; may be just one computer; should not
        // be empty; shows a text input field
        //
        snap_manager::form f(
                  get_plugin_name()
                , s.get_field_name()
                , snap_manager::form::FORM_BUTTON_SAVE
                );

        snap_manager::widget_input::pointer_t field(std::make_shared<snap_manager::widget_input>(
                          "Create Missing Snap! Websites Tables:"
                        , s.get_field_name()
                        , s.get_value()
                        , "The Snap! Websites Server makes use of a Cassandra context with various tables."
                         " Those tables must be created before one can install a Snap! domain and website."
                         " This function creates the missing tables. Tables that are already there are untouched."
                         " (i.e. we use CREATE IF NOT EXIST ...)."
                         " We ignore the value of the field here. Just click on the Save button to create the missing tables."
                         " Note: assuming that you install snapdb, create the context and then install other modules, then the"
                         " tables will get installed when installing those modules. To help developers, however, it can be"
                         " practical to have this button."
                        ));
        f.add_widget(field);

        f.generate(parent, uri);

        return true;
    }

    return false;
}


/** \brief Save 'new_value' in field 'field_name'.
 *
 * This function saves 'new_value' in 'field_name'.
 *
 * \param[in] button_name  The name of the button the user clicked.
 * \param[in] field_name  The name of the field to update.
 * \param[in] new_value  The new value to save in that field.
 * \param[in] old_or_installation_value  The old value, just in case
 *            (usually ignored,) or the installation values (only
 *            for the self plugin that manages bundles.)
 * \param[in] affected_services  The list of services that were affected
 *            by this call.
 *
 * \return true if the new_value was applied successfully.
 */
bool dbproxy::apply_setting(QString const & button_name, QString const & field_name, QString const & new_value, QString const & old_or_installation_value, std::set<QString> & affected_services)
{
    NOTUSED(old_or_installation_value);
    NOTUSED(button_name);

    // restore defaults?
    //
    //bool const use_default_value(button_name == "restore_default");

    if(field_name == "cassandra_host_list")
    {
        // to make use of the new list, make sure to restart
        //
        affected_services.insert("snapdbproxy");

        return f_snap->replace_configuration_value(g_configuration_d_filename, "cassandra_host_list", new_value);
    }

    if(field_name == "cassandra_create_context")
    {
        // TODO: use snap::process to collect the output
        //
        NOTUSED(system("snapcreatecontext"));
        return true;
    }

    if(field_name == "cassandra_create_tables")
    {
        // TODO: use snap::process to collect the output
        //
        NOTUSED(system("snapcreatetables"));
        return true;
    }

    return false;
}





SNAP_PLUGIN_END()

// vim: ts=4 sw=4 et
