// Snap Websites Server -- manage the snapbackend settings
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

// backend
//
#include "backend.h"

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


SNAP_PLUGIN_START(backend, 1, 0)


namespace
{

// TODO: offer the user a way to change this path?
struct backend_services
{
    char const *        f_service_name = nullptr;
    char const *        f_service_filename = nullptr;
    bool                f_recovery = true;
    int                 f_nice = 0;
};

backend_services g_services[4] = {
        { "snapbackend", "/etc/snapwebsites/services.d/service-snapbackend.xml", false,  5 },
        { "images",      "/etc/snapwebsites/services.d/service-images.xml",      true,  10 },
        { "pagelist",    "/etc/snapwebsites/services.d/service-pagelist.xml",    true,   3 },
        { "sendmail",    "/etc/snapwebsites/services.d/service-sendmail.xml",    true,   7 }
    };





} // no name namespace



/** \brief Get a fixed backend plugin name.
 *
 * The backend plugin makes use of different fixed names. This function
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
    case name_t::SNAP_NAME_SNAPMANAGERCGI_BACKEND_NAME:
        return "name";

    default:
        // invalid index
        throw snap_logic_exception("Invalid SNAP_NAME_SNAPMANAGERCGI_BACKEND_...");

    }
    NOTREACHED();
}




/** \brief Initialize the backend plugin.
 *
 * This function is used to initialize the backend plugin object.
 */
backend::backend()
    //: f_snap(nullptr) -- auto-init
{
}


/** \brief Clean up the backend plugin.
 *
 * Ensure the backend object is clean before it is gone.
 */
backend::~backend()
{
}


/** \brief Get a pointer to the backend plugin.
 *
 * This function returns an instance pointer to the backend plugin.
 *
 * Note that you cannot assume that the pointer will be valid until the
 * bootstrap event is called.
 *
 * \return A pointer to the backend plugin.
 */
backend * backend::instance()
{
    return g_plugin_backend_factory.instance();
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
QString backend::description() const
{
    return "Manage the snapbackend settings.";
}


/** \brief Return our dependencies.
 *
 * This function builds the list of plugins (by name) that are considered
 * dependencies (required by this plugin.)
 *
 * \return Our list of dependencies.
 */
QString backend::dependencies() const
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
int64_t backend::do_update(int64_t last_updated)
{
    NOTUSED(last_updated);

    SNAP_PLUGIN_UPDATE_INIT();
    // no updating in snapmanager*
    SNAP_PLUGIN_UPDATE_EXIT();
}


/** \brief Initialize backend.
 *
 * This function terminates the initialization of the backend plugin
 * by registering for different events.
 *
 * \param[in] snap  The child handling this request.
 */
void backend::bootstrap(snap_child * snap)
{
    f_snap = dynamic_cast<snap_manager::manager *>(snap);
    if(f_snap == nullptr)
    {
        throw snap_logic_exception("snap pointer does not represent a valid manager object.");
    }

    SNAP_LISTEN(backend, "server", snap_manager::manager, retrieve_status, _1);
}


/** \brief Determine this plugin status data.
 *
 * This function builds a tree of statuses.
 *
 * \param[in] server_status  The map of statuses.
 */
void backend::on_retrieve_status(snap_manager::server_status & server_status)
{
    if(f_snap->stop_now_prima())
    {
        return;
    }

    // TODO: make the path a parameter from snapinit somehow?
    //       (also it will change once we have a broken up version of
    //       the file)
    //
    for(auto const & service_info : g_services)
    {
        bool valid_xml(false);
        QFile input(service_info.f_service_filename);
        if(input.open(QIODevice::ReadOnly))
        {
            QDomDocument doc;
            doc.setContent(&input, false);

            // TBD: do we need the search? We expect only one <service> root tag
            //      with a name, we could just check the name?
            QDomXPath dom_xpath;
            dom_xpath.setXPath(QString("/service[@name=\"%1\"]").arg(service_info.f_service_name)); // the name varies, so do not check it
            QDomXPath::node_vector_t result(dom_xpath.apply(doc));
            if(result.size() > 0)
            {
                if(result[0].isElement())
                {
                    QDomElement service(result[0].toElement());
                    QString const disabled_attr(service.attribute("disabled"));
                    snap_manager::status_t const disabled(snap_manager::status_t::state_t::STATUS_STATE_INFO,
                                                          get_plugin_name(),
                                                          QString("%1::disabled").arg(service_info.f_service_name),
                                                          disabled_attr.isEmpty() ? "enabled" : "disabled");
                    server_status.set_field(disabled);

                    if(service_info.f_recovery)
                    {
                        QDomElement recovery_tag(service.firstChildElement("recovery"));
                        snap_manager::status_t const recovery(snap_manager::status_t::state_t::STATUS_STATE_INFO,
                                                              get_plugin_name(),
                                                              QString("%1::recovery").arg(service_info.f_service_name),
                                                              recovery_tag.text());
                        server_status.set_field(recovery);
                    }
                    else
                    {
                        QDomElement cron_tag(service.firstChildElement("cron"));
                        snap_manager::status_t const cron(snap_manager::status_t::state_t::STATUS_STATE_INFO,
                                                              get_plugin_name(),
                                                              QString("%1::cron").arg(service_info.f_service_name),
                                                              cron_tag.text());
                        server_status.set_field(cron);
                    }

                    QDomElement nice_tag(service.firstChildElement("nice"));
                    snap_manager::status_t const nice(snap_manager::status_t::state_t::STATUS_STATE_INFO,
                                                          get_plugin_name(),
                                                          QString("%1::nice").arg(service_info.f_service_name),
                                                          nice_tag.text());
                    server_status.set_field(nice);

                    valid_xml = true;
                }
            }
        }
        if(!valid_xml)
        {
            snap_manager::status_t const snapbackend(snap_manager::status_t::state_t::STATUS_STATE_ERROR,
                                                  get_plugin_name(),
                                                  QString(service_info.f_service_name) + "configuration",
                                                  QString("Could not read \"%1\" file or it was missing a snapbackend service.")
                                                        .arg(service_info.f_service_filename));
            server_status.set_field(snapbackend);
        }
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
bool backend::display_value(QDomElement parent, snap_manager::status_t const & s, snap::snap_uri const & uri)
{
    QDomDocument doc(parent.ownerDocument());

    int const pos(s.get_field_name().indexOf("::"));
    if(pos <= 0)
    {
        return false;
    }

    QString const service_name(s.get_field_name().mid(0, pos));
    QString const field_name(s.get_field_name().mid(pos + 2));

    if(service_name.isEmpty()
    || field_name.isEmpty())
    {
        return false;
    }

    if(field_name == "disabled")
    {
        // the list if frontend snapmanagers that are to receive statuses
        // of the cluster computers; may be just one computer; should not
        // be empty; shows a text input field
        //
        snap_manager::form f(
                  get_plugin_name()
                , s.get_field_name()
                , snap_manager::form::FORM_BUTTON_RESET | snap_manager::form::FORM_BUTTON_SAVE_EVERYWHERE | snap_manager::form::FORM_BUTTON_SAVE | snap_manager::form::FORM_BUTTON_RESTORE_DEFAULT
                );

        snap_manager::widget_input::pointer_t field(std::make_shared<snap_manager::widget_input>(
                          QString("Enable/Disable %1 Backend").arg(service_name)
                        , s.get_field_name()
                        , s.get_value()
                        , QString("Define whether the %1 backend is \"enabled\" or \"disabled\".").arg(service_name)
                        ));
        f.add_widget(field);

        f.generate(parent, uri);

        return true;
    }

    if(field_name == "recovery")
    {
        // the list if frontend snapmanagers that are to receive statuses
        // of the cluster computers; may be just one computer; should not
        // be empty; shows a text input field
        //
        snap_manager::form f(
                  get_plugin_name()
                , s.get_field_name()
                , snap_manager::form::FORM_BUTTON_RESET | snap_manager::form::FORM_BUTTON_SAVE_EVERYWHERE | snap_manager::form::FORM_BUTTON_SAVE | snap_manager::form::FORM_BUTTON_RESTORE_DEFAULT
                );

        snap_manager::widget_input::pointer_t field(std::make_shared<snap_manager::widget_input>(
                          QString("Recovery Delay for %1 Backend").arg(service_name)
                        , s.get_field_name()
                        , s.get_value()
                        , QString("Delay before restarting %1 if it fails to restart immediately after a crash. This number is in seconds.").arg(service_name)
                        ));
        f.add_widget(field);

        f.generate(parent, uri);

        return true;
    }

    if(field_name == "cron")
    {
        // the list if frontend snapmanagers that are to receive statuses
        // of the cluster computers; may be just one computer; should not
        // be empty; shows a text input field
        //
        snap_manager::form f(
                  get_plugin_name()
                , s.get_field_name()
                , snap_manager::form::FORM_BUTTON_RESET | snap_manager::form::FORM_BUTTON_SAVE_EVERYWHERE | snap_manager::form::FORM_BUTTON_SAVE | snap_manager::form::FORM_BUTTON_RESTORE_DEFAULT
                );

        snap_manager::widget_input::pointer_t field(std::make_shared<snap_manager::widget_input>(
                          QString("CRON Delay between runs of %1").arg(service_name)
                        , s.get_field_name()
                        , s.get_value()
                        , QString("The delay, in seconds, between each run of the %1 backend process. Note that this defines an exact tick, if the process outruns this delay, snapinit waits for the next tick, no matter what.").arg(service_name)
                        ));
        f.add_widget(field);

        f.generate(parent, uri);

        return true;
    }

    if(field_name == "nice")
    {
        // the list if frontend snapmanagers that are to receive statuses
        // of the cluster computers; may be just one computer; should not
        // be empty; shows a text input field
        //
        snap_manager::form f(
                  get_plugin_name()
                , s.get_field_name()
                , snap_manager::form::FORM_BUTTON_RESET | snap_manager::form::FORM_BUTTON_SAVE_EVERYWHERE | snap_manager::form::FORM_BUTTON_SAVE | snap_manager::form::FORM_BUTTON_RESTORE_DEFAULT
                );

        snap_manager::widget_input::pointer_t field(std::make_shared<snap_manager::widget_input>(
                          QString("Nice value for %1").arg(service_name)
                        , s.get_field_name()
                        , s.get_value()
                        , "The nice value is the same as the nice command line Unix utility. It changes the priority of the process. The larger the value, the weak the priority of that process (it will yield to processes with a smaller nice value.)"
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
bool backend::apply_setting(QString const & button_name, QString const & field_name, QString const & new_value, QString const & old_or_installation_value, std::set<QString> & affected_services)
{
    NOTUSED(old_or_installation_value);

    // restore defaults?
    //
    bool const use_default_value(button_name == "restore_default");

    int const pos(field_name.indexOf("::"));
    if(pos <= 0)
    {
        return false;
    }

    QString const service_name(field_name.mid(0, pos));
    QString const field(field_name.mid(pos + 2));

    if(service_name.isEmpty()
    || field.isEmpty())
    {
        return false;
    }

    // determine filename using the list of supported backend services
    //
    int nice_value(0);
    QString filename;
    for(auto const & service : g_services)
    {
        if(service_name == service.f_service_name)
        {
            filename = service.f_service_filename;
            nice_value = service.f_nice;
            break;
        }
    }
    if(filename.isEmpty())
    {
        return false;
    }

SNAP_LOG_WARNING("Got field \"")(field)("\" to change for \"")(service_name)("\" filename = [")(filename)("].");

    if(field == "disabled")
    {
        QFile file(filename);
        if(file.open(QIODevice::ReadWrite))
        {
            QDomDocument doc;
            doc.setContent(&file, false);

            QDomXPath dom_xpath;
            dom_xpath.setXPath(QString("/service[@name=\"%1\"]").arg(service_name));
            QDomXPath::node_vector_t result(dom_xpath.apply(doc));
            if(result.size() > 0)
            {
                if(result[0].isElement())
                {
                    // although this is about the snapbackend, we have to
                    // restart the snapinit process if we want the change to
                    // be taken in account
                    //
                    affected_services.insert(service_name);

                    QDomElement service(result[0].toElement());
                    if(use_default_value
                    || new_value.mid(0, 1).toUpper() == "D")
                    {
                        service.setAttribute("disabled", "disabled");
                    }
                    else
                    {
                        service.removeAttribute("disabled");
                    }

                    QString output(doc.toString(2));
                    QByteArray output_utf8(output.toUtf8());
                    file.seek(0L);
                    file.write(output_utf8);
                    file.resize(output_utf8.size());
                    return true;
                }
            }
        }
        return false;
    }

    if(field == "recovery")
    {
SNAP_LOG_WARNING(" -- recovery...");
        QFile file(filename);
        if(file.open(QIODevice::ReadWrite))
        {
SNAP_LOG_WARNING(" -- opened file...");
            QDomDocument doc;
            doc.setContent(&file, false);

            QDomXPath dom_xpath;
            dom_xpath.setXPath(QString("/service[@name=\"%1\"]/recovery").arg(service_name));
            QDomXPath::node_vector_t result(dom_xpath.apply(doc));
SNAP_LOG_WARNING(" -- found ")(result.size())(" matches...");
            if(result.size() > 0)
            {
                if(result[0].isElement())
                {
SNAP_LOG_WARNING(" -- ")(result.size())(" match 0 is an element...");
                    // although this is about the snapfirewall, we have to
                    // restart the snapinit process if we want the change to
                    // be taken in account
                    //
                    affected_services.insert(service_name);

                    QDomElement recovery(result[0].toElement());
                    // remove existing children
                    while(!recovery.firstChild().isNull())
                    {
                        recovery.removeChild(result[0].firstChild());
                    }
                    // now save the new recovery value
SNAP_LOG_WARNING(" -- ")(use_default_value ? "set default value" : "use new value")(" for this time...");
                    QDomText recovery_text(doc.createTextNode(use_default_value ? QString("3600") : new_value));
                    recovery.appendChild(recovery_text);

                    QString output(doc.toString(2));
                    QByteArray output_utf8(output.toUtf8());
                    file.seek(0L);
                    file.write(output_utf8);
                    file.resize(output_utf8.size());
                    return true;
                }
            }
        }
        return false;
    }

    if(field == "cron")
    {
        QFile file(filename);
        if(file.open(QIODevice::ReadWrite))
        {
            QDomDocument doc;
            doc.setContent(&file, false);

            QDomXPath dom_xpath;
            dom_xpath.setXPath(QString("/service[@name=\"%1\"]/cron").arg(service_name));
            QDomXPath::node_vector_t result(dom_xpath.apply(doc));
            if(result.size() > 0)
            {
                if(result[0].isElement())
                {
                    // although this is about the snapfirewall, we have to
                    // restart the snapinit process if we want the change to
                    // be taken in account
                    //
                    affected_services.insert(service_name);

                    QDomElement cron(result[0].toElement());
                    // remove existing children
                    while(!cron.firstChild().isNull())
                    {
                        cron.removeChild(result[0].firstChild());
                    }
                    // now save the new cron value
                    QDomText cron_text(doc.createTextNode(use_default_value ? QString("300") : new_value));
                    cron.appendChild(cron_text);

                    QString output(doc.toString(2));
                    QByteArray output_utf8(output.toUtf8());
                    file.seek(0L);
                    file.write(output_utf8);
                    file.resize(output_utf8.size());
                    return true;
                }
            }
        }
        return false;
    }

    if(field == "nice")
    {
        QFile file(filename);
        if(file.open(QIODevice::ReadWrite))
        {
            QDomDocument doc;
            doc.setContent(&file, false);

            QDomXPath dom_xpath;
            dom_xpath.setXPath(QString("/service[@name=\"%1\"]/nice").arg(service_name));
            QDomXPath::node_vector_t result(dom_xpath.apply(doc));
            if(result.size() > 0)
            {
                if(result[0].isElement())
                {
                    // although this is about the snapfirewall, we have to
                    // restart the snapinit process if we want the change to
                    // be taken in account
                    //
                    affected_services.insert(service_name);

                    QDomElement nice(result[0].toElement());
                    // remove existing children
                    while(!nice.firstChild().isNull())
                    {
                        nice.removeChild(result[0].firstChild());
                    }
                    // now save the new nice value
                    QDomText nice_text(doc.createTextNode(use_default_value ? QString("%1").arg(nice_value) : new_value));
                    nice.appendChild(nice_text);

                    QString output(doc.toString(2));
                    QByteArray output_utf8(output.toUtf8());
                    file.seek(0L);
                    file.write(output_utf8);
                    file.resize(output_utf8.size());
                    return true;
                }
            }
        }
        return false;
    }

    return false;
}





SNAP_PLUGIN_END()

// vim: ts=4 sw=4 et