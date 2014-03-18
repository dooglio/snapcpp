// Snap Websites Server -- JavaScript WYSIWYG editor
// Copyright (C) 2013-2014  Made to Order Software Corp.
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

#include "editor.h"

#include "../output/output.h"
#include "../messages/messages.h"
#include "../permissions/permissions.h"
#include "../sessions/sessions.h"
#include "../filter/filter.h"
#include "../layout/layout.h"

#include "dbutils.h"
#include "qdomreceiver.h"
#include "qdomxpath.h"
#include "qdomhelpers.h"
#include "qxmlmessagehandler.h"
#include "snap_image.h"
#include "not_reached.h"
#include "log.h"

#include <QtCassandra/QCassandraLock.h>

#include <iostream>

#include <QXmlQuery>
#include <QFile>

#include "poison.h"


SNAP_PLUGIN_START(editor, 1, 0)


/** \brief Get a fixed editor plugin name.
 *
 * The editor plugin makes use of different names in the database. This
 * function ensures that you get the right spelling for a given name.
 *
 * \param[in] name  The name to retrieve.
 *
 * \return A pointer to the name.
 */
char const *get_name(name_t name)
{
    switch(name) {
    case SNAP_NAME_EDITOR_DRAFTS_PATH:
        return "admin/drafts";

    case SNAP_NAME_EDITOR_LAYOUT:
        return "editor::layout";

    case SNAP_NAME_EDITOR_PAGE_TYPE:
        return "editor::page_type";

    case SNAP_NAME_EDITOR_TYPE_FORMAT_PATH: // a format to generate the path of a page
        return "editor::type_format_path";

    case SNAP_NAME_EDITOR_TYPE_EXTENDED_FORMAT_PATH:
        return "editor::type_extended_format_path";

    default:
        // invalid index
        throw snap_logic_exception("Invalid SNAP_NAME_EDITOR_...");

    }
    NOTREACHED();
}




/** \brief Initialize the editor plugin.
 *
 * This function is used to initialize the editor plugin object.
 */
editor::editor()
    //: f_snap(NULL) -- auto-init
{
}


/** \brief Clean up the editor plugin.
 *
 * Ensure the editor object is clean before it is gone.
 */
editor::~editor()
{
}


/** \brief Initialize editor.
 *
 * This function terminates the initialization of the editor plugin
 * by registering for different events.
 *
 * \param[in] snap  The child handling this request.
 */
void editor::on_bootstrap(snap_child *snap)
{
    f_snap = snap;

    SNAP_LISTEN(editor, "server", server, process_post, _1);
    SNAP_LISTEN(editor, "layout", layout::layout, generate_header_content, _1, _2, _3, _4);
    SNAP_LISTEN(editor, "layout", layout::layout, generate_page_content, _1, _2, _3, _4);
    SNAP_LISTEN(editor, "form", form::form, validate_post_for_widget, _1, _2, _3, _4, _5, _6);
}


/** \brief Get a pointer to the editor plugin.
 *
 * This function returns an instance pointer to the editor plugin.
 *
 * Note that you cannot assume that the pointer will be valid until the
 * bootstrap event is called.
 *
 * \return A pointer to the editor plugin.
 */
editor *editor::instance()
{
    return g_plugin_editor_factory.instance();
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
QString editor::description() const
{
    return "Offer a WYSIWYG* editor to people using the website."
        " The editor appears wherever a plugin creates a div tag with"
        " the contenteditable attribute set to true."
        "\n(*) WYSIWYG: What You See Is What You Get.";
}


/** \brief Check whether updates are necessary.
 *
 * This function updates the database when a newer version is installed
 * and the corresponding updates where not run.
 *
 * This works for newly installed plugins and older plugins that were
 * updated.
 *
 * \param[in] last_updated  The UTC Unix date when the website was last updated (in micro seconds).
 *
 * \return The UTC Unix date of the last update of this plugin.
 */
int64_t editor::do_update(int64_t last_updated)
{
    SNAP_PLUGIN_UPDATE_INIT();

    SNAP_PLUGIN_UPDATE(2014, 3, 18, 11, 20, 30, content_update);

    SNAP_PLUGIN_UPDATE_EXIT();
}


/** \brief Update the database with our content references.
 *
 * Send our content to the database so the system can find us when a
 * user references our administration pages, etc.
 *
 * \param[in] variables_timestamp  The timestamp for all the variables added to the database by this update (in micro-seconds).
 */
void editor::content_update(int64_t variables_timestamp)
{
    static_cast<void>(variables_timestamp);

    content::content::instance()->add_xml(get_plugin_name());
}


/** \brief Add editor specific tags to the layout DOM.
 *
 * This function adds different editor specific tags to the layout page
 * and body XML documents.
 *
 *
 * \param[in,out] ipath  The path being managed.
 * \param[in,out] page  The page being generated.
 * \param[in,out] body  The body being generated.
 * \param[in] ctemplate  The template in case path does not exist.
 */
void editor::on_generate_main_content(content::path_info_t& ipath, QDomElement& page, QDomElement& body, QString const& ctemplate)
{
    // a regular page
    output::output::instance()->on_generate_main_content(ipath, page, body, ctemplate);
}


/** \brief Setup page for the editor.
 *
 * The editor has a set of dynamic parameters that the users are offered
 * to setup. These parameters need to be sent to the user and we use this
 * function for that purpose.
 *
 * \todo
 * Look for a way to generate the editor data only if necessary (too
 * complex for now.)
 *
 * \param[in,out] ipath  The path being managed.
 * \param[in,out] header  The header being generated.
 * \param[in,out] metadata  The metadata being generated.
 * \param[in] ctemplate  The template in case path does not exist.
 */
void editor::on_generate_header_content(content::path_info_t& ipath, QDomElement& header, QDomElement& metadata, QString const& ctemplate)
{
    static_cast<void>(metadata);
    static_cast<void>(ctemplate);

    QDomDocument doc(header.ownerDocument());

    {
        QDomElement editor_tag(doc.createElement("editor"));
        metadata.appendChild(editor_tag);

        // define a set of dynamic parameters as defined by the user
        // /snap/head/metadata/session/<session-id>
        {
            sessions::sessions::session_info info;
            info.set_session_type(sessions::sessions::session_info::SESSION_INFO_FORM);
            info.set_session_id(EDITOR_SESSION_ID_EDIT);
            info.set_plugin_owner(get_plugin_name()); // ourselves
            info.set_page_path(ipath.get_key());
            //info.set_object_path();
            info.set_user_agent(f_snap->snapenv(snap::get_name(SNAP_NAME_CORE_HTTP_USER_AGENT)));
            info.set_time_to_live(86400);  // 24 hours
            QString const session(sessions::sessions::instance()->create_session(info));
            int32_t const random(info.get_session_random());

            QDomElement session_tag(doc.createElement("session"));
            editor_tag.appendChild(session_tag);
            QDomText session_text(doc.createTextNode(QString("%1/%2").arg(session).arg(random)));
            session_tag.appendChild(session_text);
        }
    }

    // TODO find a way to include the editor only if required
    content::content::instance()->add_javascript(header.ownerDocument(), "editor");
}


/* \brief Check whether \p cpath matches our introducers.
 *
 * This function checks that cpath matches our introducer and if
 * so we tell the path plugin that we're taking control to
 * manage this path.
 *
 * We understand "user" as in list of users.
 *
 * We understand "user/<name>" as in display that user information
 * (this may be turned off on a per user or for the entire website.)
 * Websites that only use an email address for the user identification
 * do not present these pages publicly.
 *
 * We understand "profile" which displays the current user profile
 * information in detail and allow for editing of what can be changed.
 *
 * We understand "login" which displays a form for the user to log in.
 *
 * We understand "verify-credentials" which is very similar to "login"
 * albeit simpler and only appears if the user is currently logged in
 * but not recently logged in (i.e. administration rights.)
 *
 * We understand "logout" to allow users to log out of Snap! C++.
 *
 * We understand "register" to display a registration form to users.
 *
 * We understand "verify" to check a session that is being returned
 * as the user clicks on the link we sent on registration.
 *
 * We understand "forgot-password" to let users request a password reset
 * via a simple form.
 *
 * \todo
 * If we cannot find a global way to check the Origin HTTP header
 * sent by the user agent, we probably want to check it here in
 * pages where the referrer should not be a "weird" 3rd party
 * website.
 *
 * \param[in,out] ipath  The path being handled dynamically.
 * \param[in,out] plugin_info  If you understand that cpath, set yourself here.
 */
//void editor::on_can_handle_dynamic_path(content::path_info_t& ipath, path::dynamic_plugin_t& plugin_info)
//{
//    if(ipath.get_cpath() == "admin/drafts/new")
//    {
//        // tell the path plugin that this is ours
//        plugin_info.set_plugin(this);
//    }
//}


/** \brief Execute the specified path.
 *
 * This is a dynamic page which the users plugin knows how to handle.
 *
 * This function never returns if the "page" is just a verification
 * process which redirects the user (i.e. "verify/<id>", and
 * "new-password/<id>" at this time.)
 *
 * Other paths may also redirect the user in case the path is not
 * currently supported (mainly because the user does not have
 * permission.)
 *
 * \param[in,out] ipath  The canonicalized path.
 *
 * \return true if the processing worked as expected, false if the page
 *         cannot be created ("Page Not Present" results on false)
 */
bool editor::on_path_execute(content::path_info_t& ipath)
{
    // the editor forms are generated using token replacements
    f_snap->output(layout::layout::instance()->apply_layout(ipath, this));

    return true;
}


void editor::on_validate_post_for_widget(content::path_info_t& ipath, sessions::sessions::session_info& info,
                                         QDomElement const& widget, QString const& widget_name,
                                         QString const& widget_type, bool const is_secret)
{
    static_cast<void>(widget);
    static_cast<void>(widget_type);
    static_cast<void>(is_secret);

    messages::messages *messages(messages::messages::instance());

    // we're only interested by our widgets
    QString const cpath(ipath.get_cpath());
    if(cpath == "admin/drafts/new")
    {
        // verify the type of the new page
        if(widget_name == "type")
        {
            // get the value
            QString const type(f_snap->postenv(widget_name));

            QtCassandra::QCassandraTable::pointer_t content_table(content::content::instance()->get_content_table());
            QString const site_key(f_snap->get_site_key_with_slash());
            QString const type_key(site_key + "types/taxonomy/system/content-types/" + type);
            if(!content_table->exists(type_key))
            {
                // TODO: test whether the user could create a new type, if so
                //       then do not err at all here
                messages->set_error(
                    "Unknown Type",
                    QString("Type \"%1\" is not yet defined and you do not have permission to create a new type of pages at this point.").arg(type),
                    "type doesn't exist and we do not yet offer a way to auto-create a content type",
                    false
                );
                info.set_session_type(sessions::sessions::session_info::SESSION_INFO_INCOMPATIBLE);
            }
        }
    }
}


/** \brief Process a post from one of the editor forms.
 *
 * This function processes the post of an editor form. The function uses the
 * \p ipath parameter in order to determine which form is being processed.
 *
 * \param[in,out] ipath  The path the user is accessing now.
 * \param[in] session_info  The user session being processed.
 */
void editor::on_process_form_post(content::path_info_t& ipath, sessions::sessions::session_info const& session_info)
{
    static_cast<void>(session_info);

    QString const cpath(ipath.get_cpath());
    if(cpath == "admin/drafts/new")
    {
        process_new_draft();
    }
    else
    {
        // this should not happen because invalid paths will not pass the
        // session validation process
        throw editor_exception_invalid_path("users::on_process_form_post() was called with an unsupported path: \"" + ipath.get_key() + "\"");
    }
}


/** \brief Finish the processing of a new draft.
 *
 * This function ends the processing of a new draft by saving the information
 * the user entered in the new draft form. This function creates a draft
 * under the admin/draft path under the user publishes the page. This allows
 * for the path of the new page to be better defined than if we were creating
 * the page at once.
 *
 * The path used under admin/draft simply makes use of the Unix time value.
 * If two or more users create a new draft simultaneously (within the same
 * second) then an additional .1 to .99 is added to the path. If more than
 * 100 users create a page simultaneously, the 101 and further fail saving
 * the new draft and will have to test again later.
 */
void editor::process_new_draft()
{
    content::content *content_plugin(content::content::instance());
    QtCassandra::QCassandraTable::pointer_t content_table(content_plugin->get_content_table());

    // get the 3 parameters entered by the user to get the new page started
    QString const type(f_snap->postenv("type"));
    QString const sibling(f_snap->postenv("sibling"));
    QString const title(f_snap->postenv("title"));
    QString const page_description(f_snap->postenv("description"));

    // TODO: test that "type" exists and if not creating it (if the user
    //       has engouh rights); we already checked whether the type
    //       existed and the user had engouh rights, but we want to test
    //       again; that being said, until we support creating new types
    //       we don't have to do anything here

    // now create the new page as a pure draft (opposed to an unpublised set
    // of changes on a page which is also called a draft, but is directly
    // linked to that one page.)
    time_t const start_time(f_snap->get_start_time());
    int64_t const start_date(f_snap->get_start_date());
    char const *drafts_path(get_name(SNAP_NAME_EDITOR_DRAFTS_PATH));
    QString const site_key(f_snap->get_site_key_with_slash());
    QString new_draft_key(QString("%1%2/%3").arg(site_key).arg(drafts_path).arg(start_time));

    // we got as much as we could ready before locking
    {
        // make sure this draft key is unique
        // lock the parent briefly
        QtCassandra::QCassandraLock lock(f_snap->get_context(), QByteArray(drafts_path));
        for(int extra(1); content_table->exists(new_draft_key); ++extra)
        {
            // TBD: Could it really ever happen that a website would have over
            //      100 people (i.e. not robots) create a page all at once?
            //      Should we offer to make this number a variable that
            //      administrators could bump up to be "safe"?
            if(extra >= 100) // 100 excluded since we start with zero (.0 is not included in the very first name)
            {
                // TODO: this error needs to be reported to the administrator(s)
                //       (especially if it happens often because that means
                //       robots are working on the website!)
                f_snap->die(snap_child::HTTP_CODE_CONFLICT,
                    "Conflict Error", "We could not create a new draft entry for you. Too many other drafts existed already. Please try again later.",
                    "Somehow the server was not able to generated another draft entry.");
                NOTREACHED();
            }
            new_draft_key = QString("%1%2/%3.%4").arg(site_key).arg(drafts_path).arg(start_time).arg(extra);
        }
        // create that row so the next user will detect it as existing
        // and we can then unlock the parent row
        content_table->row(new_draft_key)->cell(content::get_name(content::SNAP_NAME_CONTENT_CREATED))->setValue(start_date);
    }

    // before we go further officially create the content
    // TODO: fix the locale; it should come from the favorite locale of that
    //       user and we should offer the user to select another locale if
    //       he/she has more than one in his account
    QString const locale("xx");
    QString const owner(output::output::instance()->get_plugin_name());
    content::path_info_t draft_ipath;
    draft_ipath.set_path(new_draft_key);
    draft_ipath.force_branch(content_plugin->get_current_user_branch(new_draft_key, owner, locale, true));
    draft_ipath.force_revision(static_cast<snap_version::basic_version_number_t>(snap_version::SPECIAL_VERSION_FIRST_REVISION));
    draft_ipath.force_locale(locale);
    content_plugin->create_content(draft_ipath, owner, "page/draft");

    // save the title, description, and link to the type as a "draft type"
    QtCassandra::QCassandraTable::pointer_t data_table(content_plugin->get_data_table());
    QtCassandra::QCassandraRow::pointer_t revision_row(data_table->row(draft_ipath.get_revision_key()));
    revision_row->cell(content::get_name(content::SNAP_NAME_CONTENT_CREATED))->setValue(start_date);
    revision_row->cell(content::get_name(content::SNAP_NAME_CONTENT_TITLE))->setValue(title);
    revision_row->cell(content::get_name(content::SNAP_NAME_CONTENT_DESCRIPTION))->setValue(page_description);
    revision_row->cell(content::get_name(content::SNAP_NAME_CONTENT_BODY))->setValue(QString("enter page content here ([year])"));

    // link to the type, but not as the official type yet since this page
    // has to have a "draft page" type for a while
    {
        QString const link_name(get_name(SNAP_NAME_EDITOR_PAGE_TYPE));
        bool const source_unique(true);
        QString const link_to(get_name(SNAP_NAME_EDITOR_PAGE_TYPE));
        bool const destination_unique(false);
        content::path_info_t type_ipath;
        QString const type_key(site_key + "types/taxonomy/system/content-types/" + type);
        type_ipath.set_path(type_key);
        links::link_info source(link_name, source_unique, draft_ipath.get_key(), draft_ipath.get_branch());
        links::link_info destination(link_to, destination_unique, type_ipath.get_key(), type_ipath.get_branch());
        links::links::instance()->create_link(source, destination);
    }

    // give edit permission of the draft
    // <link name="permissions::view" to="permissions::view" mode="*:*">/types/permissions/rights/view/page/for-spammers</link>
    {
        QString const link_name(permissions::get_name(permissions::SNAP_NAME_PERMISSIONS_EDIT));
        bool const source_unique(false);
        QString const link_to(permissions::get_name(permissions::SNAP_NAME_PERMISSIONS_EDIT));
        bool const destination_unique(false);
        content::path_info_t type_ipath;
        // TBD -- should this includes the type of page?
        QString const type_key(site_key + "types/permissions/rights/edit/page");
        type_ipath.set_path(type_key);
        links::link_info source(link_name, source_unique, draft_ipath.get_key(), draft_ipath.get_branch());
        links::link_info destination(link_to, destination_unique, type_ipath.get_key(), type_ipath.get_branch());
        links::links::instance()->create_link(source, destination);
    }

    // redirect the user to the new page so he can edit it
    QString const qs_action(f_snap->get_server_parameter("qs_action"));
    f_snap->page_redirect(QString("%1?%2=edit").arg(draft_ipath.get_key()).arg(qs_action), snap_child::HTTP_CODE_FOUND,
            "Page was created successfully",
            "Sending you to your new page so that way you can edit it and ultimately publish it.");
    NOTREACHED();
}


/** \brief Check the URL and process the POST data accordingly.
 *
 * This function manages the data sent back by the editor.js script
 * and save the new values as reuired.
 *
 * The function verifies that the "editor_session" variable is set, if
 * not it ignores the POST sine another plugin may be the owner.
 *
 * \note
 * This function is a server signal generated by the snap_child
 * execute() function.
 *
 * \param[in] uri_path  The path received from the HTTP server.
 */
void editor::on_process_post(QString const& uri_path)
{
    QString const editor_full_session(f_snap->postenv("editor_session"));
    if(editor_full_session.isEmpty())
    {
        // if the editor_session variable does not exist, do not consider this
        // POST as an Editor POST
        return;
    }
    save_mode_t editor_save_mode(string_to_save_mode(f_snap->postenv("editor_save_mode")));
    if(editor_save_mode == EDITOR_SAVE_MODE_UNKNOWN)
    {
        // this could happen between versions (i.e. newer version wants to
        // use a new mode which we did not yet implement in the
        // string_to_save_mode() function.) -- it could be a problem between
        // a server that has a newer version and a server that does not...
        f_snap->die(snap_child::HTTP_CODE_NOT_ACCEPTABLE, "Not Acceptable",
                "Somehow the editor does not understand the Save command sent to the server.",
                QString("User gave us an unknown save mode (%1).").arg(f_snap->postenv("editor_save_mode")));
        NOTREACHED();
    }

    // [0] -- session Id, [1] -- random number
    QStringList const session_data(editor_full_session.split("/"));
    if(session_data.size() != 2)
    {
        // should never happen on a valid user
        f_snap->die(snap_child::HTTP_CODE_NOT_ACCEPTABLE, "Not Acceptable",
                "The session identification is not valid.",
                QString("User gave us an unknown session identifier (%1).").arg(editor_full_session));
        NOTREACHED();
    }

    messages::messages *messages(messages::messages::instance());

    content::path_info_t ipath;
    ipath.set_path(uri_path);
    ipath.set_main_page(true);
    ipath.force_locale("xx");

    // First we verify the session information from the meta tag:
    // <meta name="editor_session" content="session_id/random_number"/>
    sessions::sessions::session_info info;
    sessions::sessions::instance()->load_session(session_data[0], info, false);
    switch(info.get_session_type())
    {
    case sessions::sessions::session_info::SESSION_INFO_VALID:
        // unless we get this value we've got a problem with the session itself
        break;

    case sessions::sessions::session_info::SESSION_INFO_MISSING:
        f_snap->die(snap_child::HTTP_CODE_GONE, "Editor Session Gone", "It looks like you attempted to submit editor content without first loading it.", "User sent editor content with a session identifier that is not available.");
        NOTREACHED();
        return;

    case sessions::sessions::session_info::SESSION_INFO_OUT_OF_DATE:
        messages->set_http_error(snap_child::HTTP_CODE_GONE, "Editor Timeout", "Sorry! You sent this request back to Snap! way too late. It timed out. Please re-enter your information and re-submit.", "User did not click the submit button soon enough, the server session timed out.", true);
        return;

    case sessions::sessions::session_info::SESSION_INFO_USED_UP:
        // this should not happen because we do not mark editor sessions
        // for one time use
        messages->set_http_error(snap_child::HTTP_CODE_CONFLICT, "Editor Already Submitted", "This editor session was already processed.", "The user submitted the same session more than once.", true);
        return;

    default:
        throw snap_logic_exception("load_session() returned an unexpected SESSION_INFO_... value in editor::on_process_post()");

    }

    // verify that the session random number is compatible
    if(info.get_session_random() != session_data[1].toInt())
    {
        f_snap->die(snap_child::HTTP_CODE_NOT_ACCEPTABLE, "Not Acceptable",
                "The POST request does not correspond to the session that the editor generated.",
                QString("User POSTed a request with random number %1, but we expected %2.")
                        .arg(info.get_session_random())
                        .arg(session_data[1]));
        NOTREACHED();
    }

    // verify that the path is correct
    if(info.get_page_path() != ipath.get_key()
    || info.get_user_agent() != f_snap->snapenv(snap::get_name(SNAP_NAME_CORE_HTTP_USER_AGENT))
    || info.get_plugin_owner() != get_plugin_name())
    {
        // the path was tempered with? the agent changes between hits?
        f_snap->die(snap_child::HTTP_CODE_NOT_ACCEPTABLE, "Not Acceptable",
                "The POST request does not correspond to the editor it was defined for.",
                QString("User POSTed a request against \"%1\" with an incompatible page path (%2) or a different plugin (%3).")
                        .arg(ipath.get_key())
                        .arg(info.get_page_path())
                        .arg(info.get_plugin_owner()));
        NOTREACHED();
    }

    // editing a draft?
    if(ipath.get_cpath().startsWith("admin/drafts/"))
    {
        // adjust the mode for drafts are "special" content
        switch(editor_save_mode)
        {
        case EDITOR_SAVE_MODE_DRAFT:
            editor_save_mode = EDITOR_SAVE_MODE_SAVE;
        case EDITOR_SAVE_MODE_SAVE:
            break;

        case EDITOR_SAVE_MODE_PUBLISH:
            editor_save_mode = EDITOR_SAVE_MODE_NEW_BRANCH;
        case EDITOR_SAVE_MODE_NEW_BRANCH: // should not be accessible
            break;

        case EDITOR_SAVE_MODE_AUTO_DRAFT: // TBD
            break;

        case EDITOR_SAVE_MODE_UNKNOWN:
            // this should never happen
            throw snap_logic_exception("The UNKNOWN save mode was ignore, yet we have an edit_save_mode set to UNKNOWN.");

        }
    }

    // act on the data as per the user's specified mode
    switch(editor_save_mode)
    {
    case EDITOR_SAVE_MODE_DRAFT:
        break;

    case EDITOR_SAVE_MODE_NEW_BRANCH:
        editor_create_new_branch(ipath);
        break;

    case EDITOR_SAVE_MODE_SAVE:
        editor_save(ipath, info);
        break;

    case EDITOR_SAVE_MODE_PUBLISH:
        break;

    case EDITOR_SAVE_MODE_AUTO_DRAFT:
        break;

    case EDITOR_SAVE_MODE_UNKNOWN:
        // this should never happen
        throw snap_logic_exception("The UNKNOWN save mode was ignore, yet we have an edit_save_mode set to UNKNOWN.");

    }
}


/** \brief Transform the editor save mode to a number.
 *
 * This function transforms \p mode into a number representing the
 * save mode used with a POST.
 *
 * If the mode is not known, then EDITOR_SAVE_MODE_UNKNOWN is returned.
 * If your function cannot manage any mode, it should die() with a
 * corresponding error.
 *
 * \param[in] mode  The mode to be transformed.
 *
 * \return One of the EDITOR_SAVE_MODE_...
 */
editor::save_mode_t editor::string_to_save_mode(QString const& mode)
{
    if(mode == "draft")
    {
        return EDITOR_SAVE_MODE_DRAFT;
    }
    if(mode == "publish")
    {
        return EDITOR_SAVE_MODE_PUBLISH;
    }
    if(mode == "save")
    {
        return EDITOR_SAVE_MODE_SAVE;
    }
    if(mode == "new_branch")
    {
        return EDITOR_SAVE_MODE_NEW_BRANCH;
    }
    if(mode == "auto_draft")
    {
        return EDITOR_SAVE_MODE_AUTO_DRAFT;
    }

    return EDITOR_SAVE_MODE_UNKNOWN;
}


/** \brief Save the fields in a new revision.
 *
 * This function ensures that the current revision is copied in a new
 * revision and overwritten with the new data that the editor just
 * received (i.e. the user may just have changed his page title.)
 *
 * \param[in,out] ipath  The path to the page being updated.
 * \param[in,out] info  The session information, for the validation, just in case.
 */
void editor::editor_save(content::path_info_t& ipath, sessions::sessions::session_info& info)
{
    bool switch_branch(false);
    snap_version::version_number_t branch_number(ipath.get_branch());
    if(snap_version::SPECIAL_VERSION_SYSTEM_BRANCH == branch_number)
    {
        // force a user branch if that page still uses a system branch!
        branch_number = static_cast<int>(snap_version::SPECIAL_VERSION_USER_FIRST_BRANCH);  // FIXME cast
        switch_branch = true;
    }
    QString const key(ipath.get_key());
    QString const locale(ipath.get_locale());
    content::content *content_plugin(content::content::instance());
    QString const owner(output::output::instance()->get_plugin_name());

    // create the new revision and make it current
    //
    // TODO: if multiple users approval is required, we cannot make this
    //       new revision the current revision except if that's the very
    //       first (although the very first is not created here)
    //
    snap_version::version_number_t revision_number(content_plugin->get_new_revision(key, owner, branch_number, locale, true));

    // make this newer revision the current one
    if(switch_branch)
    {
        // working branch cannot really stay as the system branch
        // so force both branches in this case
        content_plugin->set_branch(key, owner, branch_number, false);
        content_plugin->set_branch(key, owner, branch_number, true);
        content_plugin->set_branch_key(key, owner, branch_number, true);
        content_plugin->set_branch_key(key, owner, branch_number, false);

// TODO: add revision manager
  } // <- the current/working revisions are not correctly handled yet...
    //    we should not force to the latest every time, but for now it's
    //    the way it is


        // in that case we also need to save the new revision accordingly
        content_plugin->set_current_revision(key, owner, branch_number, revision_number, locale, false);
        content_plugin->set_revision_key(key, owner, branch_number, revision_number, locale, false);
    //}
    content_plugin->set_current_revision(key, owner, branch_number, revision_number, locale, true);
    content_plugin->set_revision_key(key, owner, branch_number, revision_number, locale, true);

    // now save the new data
    ipath.force_revision(revision_number);
    QString const revision_key(ipath.get_revision_key());
    QtCassandra::QCassandraTable::pointer_t data_table(content_plugin->get_data_table());
    QtCassandra::QCassandraRow::pointer_t row(data_table->row(revision_key));

    //
    // TODO:
    //      look into getting and parsing the parser_xml ONCE
    //
    //      at this time it does that twice if no plugin ends the process
    //      by doing a redirect
    //

    // first load the XML code representing the editor widget for this page
    QDomDocument editor_widgets(get_editor_widgets(ipath));
    //layout::layout *layout_plugin(layout::layout::instance());
    //QString const script(layout_plugin->get_layout(ipath, get_name(SNAP_NAME_EDITOR_LAYOUT)));
    //if(script != "default")
    if(!editor_widgets.isNull())
    {
        //QString const name(layout_plugin->get_layout(ipath, layout::get_name(layout::SNAP_NAME_LAYOUT_LAYOUT)));
        //QtCassandra::QCassandraTable::pointer_t layout_table(layout_plugin->get_layout_table());
        //QString const parser_xml(layout_table->row(name)->cell(script)->value().stringValue());
        //QDomDocument editor_widgets("editor-form");
        //editor_widgets.setContent(parser_xml);

        messages::messages *messages(messages::messages::instance());

        // now go through all the widgets checking out their path, if the
        // path exists in doc then copy the data in the parser_xml
        QtCassandra::QCassandraRow::pointer_t revision_row(data_table->row(ipath.get_revision_key()));
        QDomNodeList widgets(editor_widgets.elementsByTagName("widget"));
        int const max_widgets(widgets.size());
        for(int i(0); i < max_widgets; ++i)
        {
            QDomElement widget(widgets.at(i).toElement());
            QString const widget_name(widget.attribute("id"));
            QString const field_name(widget.attribute("field"));
            QString const widget_type(widget.attribute("type"));
            QString const widget_auto_save(widget.attribute("auto-save", "string")); // this one is #IMPLIED
            bool const is_secret(widget.attribute("secret") == "secret"); // true if not "public" which is #IMPLIED

            if(widget_name.isEmpty())
            {
                // TODO: add some more information to this error message so
                //       we can find the element with the missing ID easily
                throw snap_logic_exception(QString("ID of a widget on line %1 found in an editor XML document is missing.").arg(widget.lineNumber()));
            }

            // now validate using a signal so any plugin can take over
            // the validation process
            sessions::sessions::session_info::session_info_type_t const session_type(info.get_session_type());
            // pretend that everything is fine so far...
            info.set_session_type(sessions::sessions::session_info::SESSION_INFO_VALID);
            int const errcnt(messages->get_error_count());
            int const warncnt(messages->get_warning_count());

            QString current_value;

            // note that a POST from the editor only includes fields that
            // changed (which reduces the size of the transfer); so we have
            // to check whether the value is available; however, we have to
            // check for required fields (since we only receive fields that
            // change, we cannot avoid saving the data)
            if(f_snap->postenv_exists(widget_name))
            {
                QString const post_value(f_snap->postenv(widget_name));
                validate_editor_post_for_widget(ipath, info, widget, widget_name, widget_type, is_secret);
                if(widget_auto_save == "int8")
                {
                    signed char c;
                    if(widget_type == "checkmark")
                    {
                        if(post_value == "off")
                        {
                            c = 0;
                        }
                        else
                        {
                            c = 1;
                        }
                    }
                    else
                    {
                        bool ok(false);
                        c = post_value.toInt(&ok, 10);
                        if(!ok)
                        {
                            f_snap->die(snap_child::HTTP_CODE_CONFLICT, "Conflict Error",
                                QString("field \"%1\" must be a valid decimal number, \"%2\" is not acceptable.")
                                        .arg(widget_name).arg(post_value),
                                "This is probably a hacker if we get the wrong value here. We should never get an invalid integer if checked by JavaScript.");
                            NOTREACHED();
                        }
                    }
                    revision_row->cell(field_name)->setValue(c);
                    current_value = QString("%1").arg(c);
                }
                else if(widget_auto_save == "string")
                {
                    // no special handling for empty strings here
                    revision_row->cell(field_name)->setValue(post_value);
                }
            }
            else
            {
                // get the current value from the database to verify the
                // current value (because it may [still] be wrong)
                QtCassandra::QCassandraValue const value(revision_row->cell(field_name)->value());
                if(!value.nullValue())
                {
                    if(widget_auto_save == "int8")
                    {
                        int const v(value.signedCharValue());
                        if(widget_type == "checkmark")
                        {
                            if(v == 0)
                            {
                                current_value = "off";
                            }
                            else
                            {
                                current_value = "on";
                            }
                        }
                        else
                        {
                            current_value = QString("%1").arg(current_value);
                        }
                    }
                    else if(widget_auto_save == "string")
                    {
                        // no special handling for empty strings here
                        current_value = value.stringValue();
                    }
                }
                validate_editor_post_for_widget(ipath, info, widget, widget_name, widget_type, is_secret);
            }

            if(info.get_session_type() != sessions::sessions::session_info::SESSION_INFO_VALID)
            {
                // it was not valid so mark the widgets as errorneous (i.e. so we
                // can display it with an error message)
                if(messages->get_error_count() == errcnt
                && messages->get_warning_count() == warncnt)
                {
                    // the pluing marked that it found an error but did not
                    // generate an actual error, do so here with a generic
                    // error message
                    messages->set_error(
                        "Invalid Content",
                        "\"" + form::form::html_64max(current_value, is_secret) + "\" is not valid for \"" + widget_name + "\".",
                        "unspecified error for widget",
                        false
                    );
                }
                messages::messages::message const& msg(messages->get_last_message());

                // Add the following to the widget so we can display the
                // widget as having an error and show the error on request
                //
                // <error>
                //   <title>$title</title>
                //   <message>$message</message>
                // </error>

                QDomElement err_tag(editor_widgets.createElement("error"));
                err_tag.setAttribute("idref", QString("messages_message_%1").arg(msg.get_id()));
                widget.appendChild(err_tag);
                QDomElement title_tag(editor_widgets.createElement("title"));
                err_tag.appendChild(title_tag);
                QDomText title_text(editor_widgets.createTextNode(msg.get_title()));
                title_tag.appendChild(title_text);
                QDomElement message_tag(editor_widgets.createElement("message"));
                err_tag.appendChild(message_tag);
                QDomText message_text(editor_widgets.createTextNode(msg.get_body()));
                message_tag.appendChild(message_text);
            }
            else
            {
                // restore the last type
                info.set_session_type(session_type);

                // TODO support for attachment so they don't just disappear on
                //      errors is required here; i.e. we need a way to be able
                //      to save all the valid attachments in a temporary place
                //      and then "move" them to their final location once the
                //      form validates properly
            }
        }
    }

    // allow each plugin to save special fields (i.e. no auto-save)
    save_editor_fields(ipath, row);

    // save the modification date in the branch
    content_plugin->modified_content(ipath);
}


/** \brief This function reads the editor widgets.
 *
 * This function is used to read the editor widgets. The function caches
 * the editor form in memory so that way we can put errors in it and thus
 * when we generate the page we can put the errors linked to each widgets.
 *
 * \param[in,out] ipath  The path for which we look for an editor form.
 *
 * \return The QDomDocument representing the editor form, may be null.
 */
QDomDocument editor::get_editor_widgets(content::path_info_t& ipath)
{
    static QMap<QString, QDomDocument> g_cached_form;

    QString const cpath(ipath.get_cpath());
    if(!g_cached_form.contains(cpath))
    {
        QDomDocument editor_widgets("editor-form");
        layout::layout *layout_plugin(layout::layout::instance());
        QString const script(layout_plugin->get_layout(ipath, get_name(SNAP_NAME_EDITOR_LAYOUT)));
        if(script != "default")
        {
            QString const layout_name(layout_plugin->get_layout(ipath, layout::get_name(layout::SNAP_NAME_LAYOUT_LAYOUT)));
            QStringList const names(layout_name.split("/"));
            if(names.size() > 0)
            {
                QString const name(names[0]);

                QtCassandra::QCassandraTable::pointer_t layout_table(layout_plugin->get_layout_table());
                QString const parser_xml(layout_table->row(name)->cell(script)->value().stringValue());
std::cerr << "Default [" << script << "," << name << "] -- [" << parser_xml.mid(0, 256) << "]\n";
                editor_widgets.setContent(parser_xml);
            }
        }
        g_cached_form[cpath] = editor_widgets;
    }

    return g_cached_form[cpath];
}


/** \brief Start a widget validation.
 *
 * This function prepares the validation of the specified widget by
 * applying common core validations proposed by the editor.
 *
 * The \p info parameter is used for the result. If something is wrong,
 * then the type of the session is changed from SESSION_INFO_VALID to
 * one of the SESSION_INFO_... that represent an error, in most cases we
 * use SESSION_INFO_INCOMPATIBLE.
 *
 * \param[in] cpath  The path where the form is defined
 * \param[in,out] info  The information linked with this form (loaded from the session)
 * \param[in] widget  The widget being tested
 * \param[in] widget_name  The name of the widget (i.e. the id="..." attribute value)
 * \param[in] widget_type  The type of the widget (i.e. the type="..." attribute value)
 * \param[in] is_secret  If true, the field is considered a secret field (i.e. a password.)
 *
 * \return Always return true so other plugins have a chance to validate too.
 */
bool editor::validate_editor_post_for_widget_impl(content::path_info_t& ipath, sessions::sessions::session_info& info, QDomElement const& widget, QString const& widget_name, QString const& widget_type, bool const is_secret)
{
    static_cast<void>(ipath);
    static_cast<void>(info);
    static_cast<void>(widget);
    static_cast<void>(widget_name);
    static_cast<void>(widget_type);
    static_cast<void>(is_secret);

    return true;
}


/** \brief Publish the page, making it the current page.
 *
 * This function saves the page in a new revision and makes it the current
 * revision. If the page does not exist yet, then it gets created (i.e.
 * saving from the admin/drafts area to a real page.
 *
 * The page type as defined when creating the draft is used as the type of
 * this new page. This generally defines the permissions, so we do not
 * worry about that here.
 *
 * \param[in,out] ipath  The path to the page being updated.
 */
void editor::editor_create_new_branch(content::path_info_t& ipath)
{
    messages::messages *messages(messages::messages::instance());
    content::content *content_plugin(content::content::instance());
    QtCassandra::QCassandraTable::pointer_t content_table(content::content::instance()->get_content_table());
    QtCassandra::QCassandraTable::pointer_t data_table(content::content::instance()->get_data_table());
    QString const site_key(f_snap->get_site_key_with_slash());

    // although we expect the URI sent by the editor to be safe, we filter it
    // again here really quick because the client sends this to us and thus
    // the data can be tainted
    QString page_uri(f_snap->postenv("editor_uri"));
    filter::filter::filter_uri(page_uri);

    // if the ipath is admin/drafts/<date> then we're dealing with a brand
    // new page; the URI we just filtered has to be unique
    bool const is_draft(ipath.get_cpath().startsWith("admin/drafts/"));

    // we got to retrieve the type used on the draft to create the full
    // page; the type is also used to define the path to the page
    //
    // IMPORTANT: it is different here from the normal case because
    //            we check the EDITOR page type and not the CONTENT
    //            page type...
    QString type_name;
    links::link_info info(is_draft ? content::get_name(content::SNAP_NAME_CONTENT_PAGE_TYPE)
                                   : get_name(SNAP_NAME_EDITOR_PAGE_TYPE),
                          false, ipath.get_key(), ipath.get_branch());
    QSharedPointer<links::link_context> link_ctxt(links::links::instance()->new_link_context(info));
    links::link_info type_info;
    if(link_ctxt->next_link(type_info))
    {
        QString const type(type_info.key());
        if(type.startsWith(site_key + "types/taxonomy/system/content-types/"))
        {
            type_name = type.mid(site_key.length() + 36);
        }
    }
    if(type_name.isEmpty())
    {
        // this should never happen, but we need a default in case the
        // type selected at the time the user created the draft is not
        // valid somehow; at this point the most secure without making
        // the page totally innaccessible is as follow
        //
        // TBD: should we use page/private instead?
        type_name = "page/secure";
    }

    // now that we have the type, we can get the path definition for that
    // type of pages; it is always important because when editing a page
    // you "lose" the path and "regain" it when you save
    QString type_format("[page-uri]"); // default is just the page URI computed from the title
    QString const type_key(QString("%1types/taxonomy/system/content-types/%2").arg(site_key).arg(type_name));
    if(content_table->row(type_key)->exists(get_name(SNAP_NAME_EDITOR_TYPE_FORMAT_PATH)))
    {
        type_format = content_table->row(type_key)->cell(get_name(SNAP_NAME_EDITOR_TYPE_FORMAT_PATH))->value().stringValue();
    }

    params_map_t params;
    QString key(format_uri(type_format, ipath, page_uri, params));
    if(is_draft)
    {
        // TBD: we probably should have a lock, but what would we lock in
        //      this case? (also it is rather unlikely that two people try
        //      to create a page with the exact same URI at the same time)
        QString extended_type_format;
        QString new_key;
        for(int i(0);; ++i)
        {
            // page already exists?
            if(i == 0)
            {
                new_key = key;
            }
            else
            {
                if(extended_type_format.isEmpty())
                {
                    if(content_table->row(type_key)->cell(get_name(SNAP_NAME_EDITOR_TYPE_EXTENDED_FORMAT_PATH)))
                    {
                        extended_type_format = content_table->row(type_key)->cell(get_name(SNAP_NAME_EDITOR_TYPE_EXTENDED_FORMAT_PATH))->value().stringValue();
                    }
                    if(extended_type_format.isEmpty()
                    || extended_type_format == type_format)
                    {
                        extended_type_format = QString("%1-[param(counter)]").arg(type_format);
                    }
                }
                new_key = format_uri(type_format, ipath, page_uri, params);
            }
            if(!content_table->exists(new_key)
            || !content_table->row(new_key)->exists(content::get_name(content::SNAP_NAME_CONTENT_CREATED)))
            {
                if(key != new_key)
                {
                    messages->set_warning(
                        "Editor Already Submitted",
                        QString("The URL \"<a href=\"%1\">%1</a>\" for your new page is already used by another page and was changed to \"%2\" for this new page.")
                                            .arg(key).arg(new_key),
                        "Changed URL because another page already used that one.");
                    key = new_key;
                }
                break;
            }
        }

        // this is a new page, create it now
        //
        // TODO: language "xx" is totally wrong, plus we actually need to
        //       publish ALL that languages present in the draft
        //
        QString const locale("xx");
        QString const owner(output::output::instance()->get_plugin_name());
        content::path_info_t page_ipath;
        page_ipath.set_path(key);
        page_ipath.force_branch(content_plugin->get_current_user_branch(key, owner, locale, true));
        page_ipath.force_revision(static_cast<snap_version::basic_version_number_t>(snap_version::SPECIAL_VERSION_FIRST_REVISION));
        page_ipath.force_locale(locale);
        content_plugin->create_content(page_ipath, owner, type_name);

        // it was created at the time the draft was created
        int64_t created_on(content_table->row(ipath.get_key())->cell(content::get_name(content::SNAP_NAME_CONTENT_CREATED))->value().int64Value());
        content_table->row(page_ipath.get_key())->cell(content::get_name(content::SNAP_NAME_CONTENT_CREATED))->setValue(created_on);

        // it is being issued now
        data_table->row(page_ipath.get_branch_key())->cell(content::get_name(content::SNAP_NAME_CONTENT_ISSUED))->setValue(f_snap->get_start_date());

        // copy the last revision
        dbutils::copy_row(data_table, ipath.get_revision_key(), data_table, page_ipath.get_revision_key());

        // TODO: copy links too...
    }
}


/** \brief Use a format string to generate a path.
 *
 * This function uses a format string to transform different parameters
 * available in a page to create its path (URI path.)
 *
 * The format uses tokens written between square brackets. The brackets
 * are used to clearly delimit the start and end of the tokens. The tokens
 * to not take any parameters. Instead, we decided to make it one simple
 * word per token. There is no recursivity support nor possibility to
 * add parameters to tokens. Instead, each and every token is a separate
 * keyword. More keywords can be added as more features are added.
 *
 * The keywords are transformed using the signal.
 *
 * \li [title] -- the title of the page filtered
 * \li [date] -- the date the page was issued (YMD)
 * \li [year] -- the year the page was issued
 * \li [month] -- the month the page was issued
 * \li [day] -- the day the page was issued
 * \li [time] -- the time the page was issued (HMS)
 * \li [hour] -- the hour the page was issued
 * \li [minute] -- the minute the page was issued
 * \li [second] -- the second the page was issued
 * \li [now] -- the date right now (YMD)
 * \li [now-year] -- the year right now
 * \li [now-month] -- the month right now
 * \li [now-day] -- the day right now
 * \li [now-time] -- the time the page was issued (HMS)
 * \li [now-hour] -- the hour right now
 * \li [now-minute] -- the minute right now
 * \li [now-second] -- the second right now
 * \li [mod] -- the modification date when the branch was last modified (YMD)
 * \li [mod-year] -- the year when the branch was last modified
 * \li [mod-month] -- the month when the branch was last modified
 * \li [mod-day] -- the day when the branch was last modified
 * \li [mod-time] -- the time the page was issued (HMS)
 * \li [mod-hour] -- the hour when the branch was last modified
 * \li [mod-minute] -- the minute when the branch was last modified
 * \li [mod-second] -- the second when the branch was last modified
 *
 * \todo
 * Look into ways to allow for extensions.
 *
 * \param[in] format  The format used to generate the string.
 * \param[in,out] ipath  The ipath of the page we're working on.
 * \param[in] page_name  The name of the page (i.e. the title transformed
 *                       to fit Snap! paths limitations)
 * \param[in] params  An array of parameters.
 *
 * \return The formatted path.
 */
QString editor::format_uri(QString const& format, content::path_info_t& ipath, QString const& page_name, params_map_t const& params)
{
    class parser
    {
    public:
        typedef ushort char_t;

        parser(editor *e, QString const& format, content::path_info_t& ipath, QString const& page_name, params_map_t const& params)
            : f_editor(e)
            , f_format(format)
            //, f_pos(0)
            , f_token_info(ipath, page_name, params)
            //, f_result("")
        {
        }

        void parse()
        {
            for(;;)
            {
                char_t c(getc());
                if(c == static_cast<char_t>(EOF))
                {
                    // done
                    break;
                }
                if(c == '[')
                {
                    if(!parse_token())
                    {
                        // TBD?
                    }
                }
                else
                {
                    f_result += QChar(c);
                }
            }
        }

        bool parse_token()
        {
            f_token_info.f_token.clear();
            for(;;)
            {
                char_t c(getc());
                if(c == static_cast<char_t>(EOF) || isspace(c))
                {
                    return false;
                }
                if(c == ']')
                {
                    break;
                }
                f_token_info.f_token += QChar(c);
            }
            f_token_info.f_result.clear();
            f_editor->replace_uri_token(f_token_info);
            f_result += f_token_info.f_result;
            return true;
        }

        char_t getc()
        {
            char_t c(static_cast<char_t>(EOF));
            if(f_pos < f_format.length())
            {
                c = f_format[f_pos].unicode();
                ++f_pos;
            }
            return c;
        }

        QString result()
        {
            return f_result;
        }

    private:
        editor *                    f_editor;
        QString const&              f_format;
        controlled_vars::zint32_t   f_pos;
        editor_uri_token            f_token_info;
        QString                     f_result;
    };
    parser result(this, format, ipath, page_name, params);
    result.parse();
    return result.result();
}


/** \brief Replace the specified token with data to generate a URI.
 *
 * This signal is used to transform tokens from URI format strings to
 * values. If your function doesn't know about the token, then just
 * return without doing anything. The main function returns false
 * if it understands the token and thus no other plugins receive the
 * signal in that case.
 *
 * The ipath represents the path to the page being saved. It may be
 * the page draft (under "admin/drafts".)
 *
 * The page_name parameter is computed from the page title. It is the title
 * all in lowercase, with dashes instead of spaces, and removal of
 * characters that are not generally welcome in a URI.
 *
 * The params map defines additional parameters tha are available at the
 * time the signal is called.
 *
 * The token is the keyword parsed our of the input format. For example, it
 * may be the word "year" to be replaced by the current year.
 *
 * \note
 * This function transforms the "editor" known tokens, this includes
 * all the tokens known by the editor and any plugin that cannot include
 * the editor without creating a circular dependency.
 *
 * \param[in,out] token_info  The information about this token.
 *
 * \return true if the token was not an editor basic token, false otherwise
 *         so other plugins get a chance to transform the token themselves
 */
bool editor::replace_uri_token_impl(editor_uri_token& token_info)
{
    //
    // TITLE
    //
    if(token_info.f_token == "page-uri")
    {
        token_info.f_result = token_info.f_page_name;
        return false;
    }

    QtCassandra::QCassandraTable::pointer_t content_table(content::content::instance()->get_content_table());
    QtCassandra::QCassandraTable::pointer_t data_table(content::content::instance()->get_data_table());

    //
    // TIME / DATE
    //
    enum type_t
    {
        TIME_SOURCE_UNKNOWN,
        TIME_SOURCE_NOW,
        TIME_SOURCE_CREATION_DATE,
        TIME_SOURCE_MODIFICATION_DATE
    };
    std::string time_format;
    type_t type(TIME_SOURCE_UNKNOWN);
    if(token_info.f_token == "date")
    {
        time_format = "%Y%m%d";
        type = TIME_SOURCE_CREATION_DATE;
    }
    else if(token_info.f_token == "year")
    {
        time_format = "%Y";
        type = TIME_SOURCE_CREATION_DATE;
    }
    else if(token_info.f_token == "month")
    {
        time_format = "%m";
        type = TIME_SOURCE_CREATION_DATE;
    }
    else if(token_info.f_token == "day")
    {
        time_format = "%d";
        type = TIME_SOURCE_CREATION_DATE;
    }
    else if(token_info.f_token == "time")
    {
        time_format = "%H%M%S";
        type = TIME_SOURCE_CREATION_DATE;
    }
    else if(token_info.f_token == "hour")
    {
        time_format = "%H";
        type = TIME_SOURCE_CREATION_DATE;
    }
    else if(token_info.f_token == "minute")
    {
        time_format = "%M";
        type = TIME_SOURCE_CREATION_DATE;
    }
    else if(token_info.f_token == "second")
    {
        time_format = "%S";
        type = TIME_SOURCE_CREATION_DATE;
    }
    else if(token_info.f_token == "now")
    {
        time_format = "%Y%m%d";
        type = TIME_SOURCE_NOW;
    }
    else if(token_info.f_token == "now-year")
    {
        time_format = "%Y";
        type = TIME_SOURCE_NOW;
    }
    else if(token_info.f_token == "now-month")
    {
        time_format = "%m";
        type = TIME_SOURCE_NOW;
    }
    else if(token_info.f_token == "now-day")
    {
        time_format = "%d";
        type = TIME_SOURCE_NOW;
    }
    else if(token_info.f_token == "now-time")
    {
        time_format = "%H%M%S";
        type = TIME_SOURCE_NOW;
    }
    else if(token_info.f_token == "now-hour")
    {
        time_format = "%H";
        type = TIME_SOURCE_NOW;
    }
    else if(token_info.f_token == "now-hour")
    {
        time_format = "%H";
        type = TIME_SOURCE_NOW;
    }
    else if(token_info.f_token == "now-minute")
    {
        time_format = "%M";
        type = TIME_SOURCE_NOW;
    }
    else if(token_info.f_token == "now-second")
    {
        time_format = "%S";
        type = TIME_SOURCE_NOW;
    }
    else if(token_info.f_token == "mod")
    {
        time_format = "%Y%m%d";
        type = TIME_SOURCE_MODIFICATION_DATE;
    }
    else if(token_info.f_token == "mod-year")
    {
        time_format = "%Y";
        type = TIME_SOURCE_MODIFICATION_DATE;
    }
    else if(token_info.f_token == "mod-month")
    {
        time_format = "%m";
        type = TIME_SOURCE_MODIFICATION_DATE;
    }
    else if(token_info.f_token == "mod-day")
    {
        time_format = "%d";
        type = TIME_SOURCE_MODIFICATION_DATE;
    }
    else if(token_info.f_token == "mod-time")
    {
        time_format = "%H%M%S";
        type = TIME_SOURCE_MODIFICATION_DATE;
    }
    else if(token_info.f_token == "mod-hour")
    {
        time_format = "%H";
        type = TIME_SOURCE_MODIFICATION_DATE;
    }
    else if(token_info.f_token == "mod-minute")
    {
        time_format = "%M";
        type = TIME_SOURCE_MODIFICATION_DATE;
    }
    else if(token_info.f_token == "mod-second")
    {
        time_format = "%S";
        type = TIME_SOURCE_MODIFICATION_DATE;
    }

    if(type != TIME_SOURCE_UNKNOWN)
    {
        time_t seconds;
        switch(type)
        {
        case TIME_SOURCE_CREATION_DATE:
            {
                QString cell_name;

                if(token_info.f_ipath.get_cpath().startsWith("admin/drafts/"))
                {
                    cell_name = content::get_name(content::SNAP_NAME_CONTENT_CREATED);
                }
                else
                {
                    cell_name = content::get_name(content::SNAP_NAME_CONTENT_ISSUED);
                }
                seconds = content_table->row(token_info.f_ipath.get_key())->cell(cell_name)->value().int64Value() / 1000000;
            }
            break;

        case TIME_SOURCE_MODIFICATION_DATE:
            seconds = data_table->row(token_info.f_ipath.get_branch_key())->cell(content::get_name(content::SNAP_NAME_CONTENT_MODIFIED))->value().int64Value() / 1000000;
            break;

        case TIME_SOURCE_NOW:
            seconds = f_snap->get_start_date() / 1000000;
            break;

        //case TIME_SOURCE_UNKNOWN: -- this is not possible, really! look at the if()
        default:
            throw snap_logic_exception("somehow the time parameter was set to an unknown value");

        }
        struct tm time_info;
        gmtime_r(&seconds, &time_info);
        char buf[256];
        buf[0] = '\0';
        strftime(buf, sizeof(buf), time_format.c_str(), &time_info);
        return false;
    }

    return true;
}


/** \brief Save fields that the editor and other plugins manage.
 *
 * This signal can be overridden by other plugins to save the fields that
 * they add to the editor manager.
 *
 * The row parameter passed down to this function is the revision row in
 * the data table. If you need to save data in another location (i.e. the
 * branch or even in the content table) then you want to look into generating
 * a key for that content and get the corresponding row. In most cases, though
 * saving your data in the revision row is the way to go.
 *
 * Note that the ipath parameter has its revision number set to the new
 * revision number that was allocated to save this data.
 *
 * \param[in,out] ipath  The ipath to the page being modified.
 * \param[in,out] row  The row where all the fields are to be saved.
 */
bool editor::save_editor_fields_impl(content::path_info_t& ipath, QtCassandra::QCassandraRow::pointer_t row)
{
    static_cast<void>(ipath);

    if(f_snap->postenv_exists("title"))
    {
        QString const title(f_snap->postenv("title"));
        // TODO: XSS filter title
        row->cell(content::get_name(content::SNAP_NAME_CONTENT_TITLE))->setValue(title);
    }
    if(f_snap->postenv_exists("body"))
    {
        QString body(f_snap->postenv("body"));
        // TODO: find a way to detect whether images are allowed in this
        //       field and if not make sure that if we find some err
        // body may include images, transform the <img src="inline-data"/>
        // to an <img src="/images/..."/> link instead
        parse_out_inline_img(ipath, body);
        // TODO: XSS filter body
        row->cell(content::get_name(content::SNAP_NAME_CONTENT_BODY))->setValue(body);
    }

    return true;
}


/** \brief Transform inline images into links.
 *
 * This function takes a value that was posted by the user of an editor
 * input field and transforms the <img> tags that have inline data into
 * images saved as files attachment to the current page and replace the
 * src="..." with the corresponding path.
 *
 * \param[in,out] ipath  The ipath to the page being modified.
 * \param[in,out] body  The HTML to be parsed and "fixed."
 */
void editor::parse_out_inline_img(content::path_info_t& ipath, QString& body)
{
    QDomDocument doc;
    //doc.setContent("<?xml version='1.1' encoding='utf-8'?><element>" + body + "</element>");
    doc.setContent(QString("<element>%1</element>").arg(body));
    QDomNodeList imgs(doc.elementsByTagName("img"));

    bool changed(false);
    int const max_images(imgs.size());
    for(int i(0); i < max_images; ++i)
    {
        QDomElement img(imgs.at(i).toElement());
        if(!img.isNull())
        {
            // data:image/jpeg;base64,...
            QString const src(img.attribute("src"));
            if(src.startsWith("data:"))
            {
                bool const valid(save_inline_image(ipath, img, src));
                if(valid)
                {
                    changed = true;
                }
                else
                {
                    // remove that tag, it is not considered valid so it
                    // may cause harm, who knows...
                    img.parentNode().removeChild(img);
                }
            }
        }
    }

    // if any image was switched, change the body with the new img tags
    if(changed)
    {
        // get the document back in the form of a string (unfortunate...)
        body = doc.toString(-1);
        body.remove("<element>").remove("</element>");
    }
}


bool editor::save_inline_image(content::path_info_t& ipath, QDomElement img, QString const& src)
{
    static uint32_t g_index = 0;

    // we only support images so the MIME type has to start with "image/"
    if(!src.startsWith("data:image/"))
    {
        return false;
    }

    // verify that it is base64 encoded, that's the only encoding we
    // support (and browsers too I would think?)
    int const p(src.indexOf(';', 11));
    if(p < 0
    || p > 64
    || src.mid(p, 8) != ";base64,")
    {
        return false;
    }

    // the type of image (i.e. "png", "jpeg", "gif"...)
    // we set that up so we know that it is "jpeg" and not "jpg"
    QString type(src.mid(11, p - 11));
    if(type != "png"
    && type != "jpeg"
    && type != "gif")
    {
        // not one of the image format that our JavaScript supports, so
        // ignore at once
        return false;
    }

    // this is an inline image
    QByteArray const base64(src.mid(p + 8).toUtf8());
    QByteArray const data(QByteArray::fromBase64(base64));

    // verify the image magic
    snap_image image;
    if(!image.get_info(data))
    {
        return false;
    }
    int const max_frames(image.get_size());
    if(max_frames == 0)
    {
        // a "valid" image file without actual frames?!
        return false;
    }
    for(int i(0); i < max_frames; ++i)
    {
        smart_snap_image_buffer_t ibuf(image.get_buffer(i));
        if(ibuf->get_mime_type().mid(6) != type)
        {
            // mime types do not match!?
            return false;
        }
    }

//void filter::filter_filename(QString& filename, QString const& extension (a.k.a. type))
//{
    // TODO: we should move this code fixing up the filename in a filter
    //       function because we probably give access to other plugins
    //       to such a feature.

    // get the filename in lowercase, remove path, fix extension, make sure
    // it is otherwise acceptable...
    QString filename(img.attribute("filename"));

    // remove the path if there is one
    int const slash(filename.lastIndexOf('/'));
    if(slash >= 0)
    {
        filename.remove(0, slash);
    }

    // force to all lowercase
    filename = filename.toLower();

    // avoid spaces in filenames
    filename.replace(" ", "-");

    // avoid "--", replace with a single "-"
    for(;;)
    {
        int const length(filename.length());
        filename.replace("--", "-");
        if(filename.length() == length)
        {
            break;
        }
    }

    // remove '-' at the start
    while(!filename.isEmpty() && filename[0] == '-')
    {
        filename.remove(0, 1);
    }

    // remove '-' at the end
    while(!filename.isEmpty() && *filename.end() == '-')
    {
        filename.remove(filename.length() - 1, 1);
    }

    // force the extension to what we defined in 'type' (image MIME)
    if(!filename.isEmpty())
    {
        int const period(filename.lastIndexOf('.'));
        filename = QString("%1.%2").arg(filename.left(period)).arg(type == "jpeg" ? "jpg" : type);
    }

    // prevent hidden Unix filenames, it could cause problems on Linux
    if(!filename.isEmpty() && filename[0] == '.')
    {
        // clear the filename if it has a name we do not
        // like (i.e. hidden Unix files are forbidden)
        filename.clear();
    }

    // user supplied filename is not considered valid, use a default name
    if(filename.isEmpty())
    {
        filename = QString("image.%1").arg(type);
    }
//}

    snap_child::post_file_t postfile;
    postfile.set_name("image");
    postfile.set_filename(filename);
    postfile.set_original_mime_type(type);
    postfile.set_creation_time(f_snap->get_start_time());
    postfile.set_modification_time(f_snap->get_start_time());
    postfile.set_data(data);
    postfile.set_image_width(image.get_buffer(0)->get_width());
    postfile.set_image_height(image.get_buffer(0)->get_height());
    ++g_index;
    postfile.set_index(g_index);
    content::attachment_file attachment(f_snap, postfile);
    attachment.set_multiple(false);
    attachment.set_cpath(ipath.get_cpath());
    attachment.set_field_name("image");
    attachment.set_attachment_owner(output::output::instance()->get_plugin_name());
    attachment.set_attachment_type("attachment/public");
    // TODO: define the locale in some ways... for now we use "neutral"
    content::content::instance()->create_attachment(attachment, ipath.get_branch(), "");

    // replace the inline image data block with a local (albeit full) URI
    // TBD: the probably won't work if the website definition uses a path
    img.setAttribute("src", QString("/%1/%2").arg(ipath.get_cpath()).arg(filename));

    return true;
}


/** \brief Setup for editor.
 *
 * The editor transforms all the fields added to the XML and that the user
 * is expected to be able to edit in a way that gives the user the ability
 * to click "Edit this field". More or less, this means adding a couple of
 * \<div\> tags around the data of those fields.
 *
 * In order to allow field editing, you need one \<div\> with class
 * "snap-editor". This field will also be given the attribute "field_name"
 * with the name of the field. Within that first \<div\> you want another
 * \<div\> with class "editor-content".
 *
 * \todo
 * We need to know whether the editor is only inserted if the action is
 * set to edit or even in view mode. At this point we need ot it for
 * a customer and only the edit mode requires the editor. This may also
 * be a setting in the database (per page, type, global...).
 *
 * \param[in] ipath  The path being managed.
 * \param[in,out] doc  The XML document that was generated for this body.
 * \param[in] xsl  The XSLT document that is about to be used to transform
 *                 the body (still as a string).
 */
void editor::on_generate_page_content(content::path_info_t& ipath, QDomElement& page, QDomElement& body, QString const& ctemplate)
{
    enum added_form_file_support_t
    {
        ADDED_FORM_FILE_NONE,
        ADDED_FORM_FILE_NOT_YET,
        ADDED_FORM_FILE_YES
    };
    static added_form_file_support_t g_added_editor_form_js_css(ADDED_FORM_FILE_NONE);

    static_cast<void>(ctemplate);

    content::content *content_plugin(content::content::instance());

    QDomDocument editor_widgets(get_editor_widgets(ipath));
    if(editor_widgets.isNull())
    {
        // no editor specified for this page, skip on it (no editing allowed)
        return;
    }

    // first load the XML code representing the editor widget for this page
//    QString const script(layout_plugin->get_layout(ipath, get_name(SNAP_NAME_EDITOR_LAYOUT)));
//    if(script == "default")
//    {
//std::cerr << "***\n*** Default so we cannot get the editor!?\n***\n";
//        // no editor specified for this page, skip on it (no editing allowed)
//        return;
//    }
//    QString const name(body.attribute("layout-name"));

    // editor enters in action

    // TODO: avoid the full XML conversion to a DOM and search the
    // Done? names using string indexOf() and similar code; it would
    //       be a lot faster! (we do not have to test that the XSL
    //       is valid here, it will be tested when used to parse the
    //       body XML document)

    //QDomDocument body_xsl("body");
    //body_xsl.setContent(xsl, true);

    //QString cell(content_table->row(ipath.get_key())->cell());
    //QDomXPath name_xpath;
    //name_xpath.setXPath("/xsl:stylesheet/xsl:variable[@name='layout-name']");
    //QDomXPath::node_vector_t name_nodes(name_xpath.apply(body_xsl));

    //QDomXPath editor_xpath;
    //editor_xpath.setXPath("/xsl:stylesheet/xsl:variable[@name='layout-editor']");
    //QDomXPath::node_vector_t editor_nodes(editor_xpath.apply(body_xsl));

    //if(name_nodes.size() != 1 || editor_nodes.size() != 1)
    //{
    //    //f_snap->die(snap_child::HTTP_CODE_CONFLICT, "Multiple or Missing Layout Name / Editor Definitions",
    //    //    "The theme is required to have exactly one variable named \"layout-name\" and one named \"layout-editor\".",
    //    //    "The programmer or themer need to fix this theme before accessing this page.");
    //    //NOTREACHED();
    //    // Most pages do not have a layout-editor entry (TBD at this point though)
    //    return;
    //}

    //QDomElement n(name_nodes.at(0).toElement());
    //QDomElement e(editor_nodes.at(0).toElement());
    //if(n.isNull() || e.isNull())
    //{
    //    throw snap_logic_exception("The nodes retrieved with QDomXPath were not elements.");
    //}
    //QString const name(n.text());
    //QString const script(e.text());

    //QtCassandra::QCassandraTable::pointer_t layout_table(layout_plugin->get_layout_table());
    //QString const parser_xml(layout_table->row(name)->cell(script)->value().stringValue());

    QDomDocument doc(page.ownerDocument());

    //QDomDocument editor_widgets("editor-form");
    //editor_widgets.setContent(parser_xml);

    // now go through all the widgets checking out their path, if the
    // path exists in doc then copy the data in the parser_xml
    QtCassandra::QCassandraTable::pointer_t data_table(content_plugin->get_data_table());
    QtCassandra::QCassandraRow::pointer_t revision_row(data_table->row(ipath.get_revision_key()));
    QDomNodeList widgets(editor_widgets.elementsByTagName("widget"));
    int const max_widgets(widgets.size());
    for(int i(0); i < max_widgets; ++i)
    {
        QDomElement w(widgets.at(i).toElement());
        QString const field_name(w.attribute("field"));
        QString const field_type(w.attribute("type"));
        QString const widget_auto_save(w.attribute("auto-save", "string")); // this one is #IMPLIED

        // get the current value from the database if it exists
        if(revision_row->exists(field_name))
        {
            QtCassandra::QCassandraValue const value(revision_row->cell(field_name)->value());
            QString current_value;
            bool set_value(true);
            if(widget_auto_save == "int8")
            {
                int const v(value.signedCharValue());
                if(field_type == "checkmark")
                {
                    if(v == 0)
                    {
                        current_value = "off";
                    }
                    else
                    {
                        current_value = "on";
                    }
                }
                else
                {
                    current_value = QString("%1").arg(current_value);
                }
            }
            else if(widget_auto_save == "string")
            {
                // no special handling for empty strings here
                current_value = revision_row->cell(field_name)->value().stringValue();
            }
            else
            {
                // If no auto-save we expect a plugin to furnish the current value
                set_value = false;
            }

            if(set_value)
            {
                QDomElement value_tag;
                value_tag = w.firstChildElement("value");
                if(value_tag.isNull())
                {
                    // no <value> tag, create one
                    value_tag = editor_widgets.createElement("value");
                    w.appendChild(value_tag);
                }
                else
                {
                    snap_dom::remove_all_children(value_tag);
                }
                snap_dom::insert_html_string_to_xml_doc(value_tag, current_value);
            }
        }
    }

    // now process the XML data with the plugin specialized data for
    // each field through the editor XSLT
    QFile editor_xsl_file(":/xsl/editor/editor-form.xsl");
    if(!editor_xsl_file.open(QIODevice::ReadOnly))
    {
        throw snap_logic_exception("Could not open resource file \":/xsl/editor/editor-form.xsl\".");
    }
    QByteArray const data(editor_xsl_file.readAll());
    if(data.isEmpty())
    {
        throw snap_logic_exception("Could not read resource file \":/xsl/editor/editor-form.xsl\".");
    }
    QString const editor_xsl(QString::fromUtf8(data.data(), data.size()));
    QXmlQuery q(QXmlQuery::XSLT20);
    QMessageHandler msg;
    msg.set_xsl(editor_xsl);
    msg.set_doc(editor_widgets.toString());
    q.setMessageHandler(&msg);
    q.setFocus(editor_widgets.toString());

    // set action variable to the current action
    QString const qs_action(f_snap->get_server_parameter("qs_action"));
    snap_uri const& uri(f_snap->get_uri());
    QString const action(uri.query_option(qs_action));
    q.bindVariable("action", QVariant(action));

    q.setQuery(editor_xsl);
    QDomDocument doc_output("widgets");
    QDomReceiver receiver(q.namePool(), doc_output);
    q.evaluateTo(&receiver);

//std::cerr << "Editor Focus [" << editor_widgets.toString() << "]\n";
//std::cerr << "Editor Output [" << doc_output.toString() << "]\n";

    QDomNodeList result_widgets(doc_output.elementsByTagName("widget"));
    int const max_results(result_widgets.size());
    for(int i(0); i < max_results; ++i)
    {
        QDomElement w(result_widgets.at(i).toElement());
        QString const path(w.attribute("path"));

        QDomElement field_tag(snap_dom::create_element(body, path));
        snap_dom::insert_node_to_xml_doc(field_tag, w);

        // this is a path in doc
        //QDomXPath path_xpath;
        //path_xpath.setXPath(path);
        //QDomXPath::node_vector_t path_nodes(path_xpath.apply(doc.documentElement()));
        //if(path_nodes.size() == 1)
        //{
        //    // the result overwrites the previous value!
        //    QDomElement parent(path_nodes.at(0).toElement());
        //    // TBD: we probably should remove the remove_all_children() call
        //    //      which should not be necessary anymore since we do not
        //    //      use that scheme anymore (i.e. we do not expect plugins
        //    //      to create data and then go under them and plug the
        //    //      editor in there! -- that makes it extremely laborious)
        //    snap_dom::remove_all_children(parent);
        //    snap_dom::insert_node_to_xml_doc(parent, w);
        //}

        if(g_added_editor_form_js_css == ADDED_FORM_FILE_NONE)
        {
            g_added_editor_form_js_css = ADDED_FORM_FILE_NOT_YET;
        }
    }
//std::cerr << "Editor XML [" << doc.toString() << "]\n";

    if(g_added_editor_form_js_css == ADDED_FORM_FILE_NOT_YET)
    {
        g_added_editor_form_js_css = ADDED_FORM_FILE_YES;

        content::content::instance()->add_javascript(doc, "editor");
        content::content::instance()->add_css(doc, "editor");
    }
}



SNAP_PLUGIN_END()

// vim: ts=4 sw=4 et
