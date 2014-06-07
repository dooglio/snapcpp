/* rc.cpp -- written by Alexis WILKE for Made to Order Software Corp. (c) 2005-2014 */

/*

Copyright (c) 2005-2014 Made to Order Software Corp.

http://snapwebsites.org/project/as2js

Permission is hereby granted, free of charge, to any
person obtaining a copy of this software and
associated documentation files (the "Software"), to
deal in the Software without restriction, including
without limitation the rights to use, copy, modify,
merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom
the Software is furnished to do so, subject to the
following conditions:

The above copyright notice and this permission notice
shall be included in all copies or substantial
portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO
EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

// this is private
#include    "rc.h"

#include    "as2js/exceptions.h"
#include    "as2js/json.h"
#include    "as2js/message.h"

#include    <controlled_vars/controlled_vars_auto_enum_init.h>

#include    <cstring>


namespace as2js
{

namespace
{

char const *g_rc_directories[] =
{
    // check user defined variable
    "$AS2JS_RC",
    // try locally first (assuming you are a heavy JS developer, you'd
    // probably start with your local files)
    "as2js",
    // try your user "global" installation directory
    "~/.config/as2js",
    // try the system directory
    "/etc/as2js",
    nullptr
};

controlled_vars::fbool_t    g_home_initialized;
String                      g_home;

}
// no name namespace


/** \brief Find the resource file.
 *
 * This function tries to find a resource file.
 *
 * The resource file defines two paths where we can find the system
 * definitions and user imports.
 *
 * \param[in] accept_if_missing  Whether an error is generated (false)
 *                               if the file cannot be found.
 */
void rc_t::init_rc(bool const accept_if_missing)
{
    // internal defaults
    f_scripts = "as2js/scripts";
    f_db = "/tmp/as2js_packages.db";

    // first try to find a place with a .rc file
    FileInput::pointer_t in(new FileInput());
    String rcfilename;
    for(char const **dir = g_rc_directories; *dir != nullptr; ++dir)
    {
        std::stringstream buffer;
        if(**dir == '$')
        {
            String env_defined(getenv(*dir + 1));
            if(env_defined.empty())
            {
                continue;
            }
            buffer << env_defined << "/as2js.rc";
        }
        else if(**dir == '~' && (*dir)[1] == '/')
        {
            String home(get_home());
            if(home.empty())
            {
                // no valid $HOME variable
                continue;
            }
            buffer << home << "/" << (dir + 2) << "/as2js.rc";
        }
        else
        {
            buffer << *dir << "/as2js.rc";
        }
        rcfilename.from_utf8(buffer.str().c_str());
        if(!rcfilename.empty())
        {
            if(in->open(rcfilename))
            {
                // it worked, we are done
                break;
            }
            rcfilename.clear();
        }
    }

    if(rcfilename.empty())
    {
        if(!accept_if_missing)
        {
            // no position in this case...
            Message msg(MESSAGE_LEVEL_FATAL, AS_ERR_INSTALLATION);
            msg << "cannot find the as2js.rc file; the system default is usually put in /etc/as2js/as2js.rc";
            throw exception_exit(1, "cannot find the as2js.rc file; the system default is usually put in /etc/as2js/as2js.rc");
        }

        // nothing to load in this case...
    }
    else
    {
        JSON::pointer_t json(new JSON);
        JSON::JSONValue::pointer_t root(json->parse(in));
        JSON::JSONValue::type_t type(root->get_type());
        // null is accepted, in which case we keep the defaults
        if(type != JSON::JSONValue::JSON_TYPE_NULL)
        {
            Position pos;
            pos.set_filename(rcfilename);

            if(type != JSON::JSONValue::JSON_TYPE_OBJECT)
            {
                Message msg(MESSAGE_LEVEL_FATAL, AS_ERR_UNEXPECTED_RC, pos);
                msg << "A resource file (.rc) must be defined as a JSON object, or set to 'null'";
                throw exception_exit(1, "A resource file (.rc) must be defined as a JSON object, or set to 'null'");
            }

            JSON::JSONValue::object_t const& obj(root->get_object());
            for(JSON::JSONValue::object_t::const_iterator it(obj.begin()); it != obj.end(); ++it)
            {
                // the only type of value that we expect are objects within the
                // main object; each one represents a package
                JSON::JSONValue::type_t sub_type(it->second->get_type());
                if(sub_type != JSON::JSONValue::JSON_TYPE_STRING)
                {
                    Message msg(MESSAGE_LEVEL_FATAL, AS_ERR_UNEXPECTED_RC, pos);
                    msg << "A resource file is expected to be an object of string elements.";
                    throw exception_exit(1, "A resource file (.rc) must be defined as a JSON object, or set to 'null'");
                }

                String parameter_name(it->first);
                String parameter_value(it->second->get_string());

                if(parameter_name == "scripts")
                {
                    f_scripts = parameter_value;
                }
                else if(parameter_name == "db")
                {
                    f_db = parameter_value;
                }
            }
        }
    }
}


String const& rc_t::get_scripts() const
{
    return f_scripts;
}


String const& rc_t::get_db() const
{
    return f_db;
}


String const& rc_t::get_home()
{
    if(!g_home_initialized)
    {
        g_home_initialized = true;
        g_home.from_utf8(getenv("HOME"));

        // TODO: add test for tainted getenv() result
    }

    return g_home;
}

}
// namespace as2js

// vim: ts=4 sw=4 et
