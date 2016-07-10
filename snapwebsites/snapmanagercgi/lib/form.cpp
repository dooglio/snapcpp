//
// File:        form.cpp
// Object:      Helper functions to generate a simple form.
//
// Copyright:   Copyright (c) 2016 Made to Order Software Corp.
//              All Rights Reserved.
//
// http://snapwebsites.org/
// contact@m2osw.com
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "form.h"

#include "manager.h"

#include "log.h"
#include "not_reached.h"
#include "not_used.h"
#include "qdomhelpers.h"

#include "poison.h"

namespace snap_manager
{



widget::widget(QString const & name)
    : f_name(name)
{
}


widget_input::widget_input(QString const & label, QString const & name, QString const & initial_value, QString const & description)
    : widget(name)
    , f_label(label)
    , f_value(initial_value)
    , f_description(description)
{
}


void widget_input::generate(QDomElement parent)
{
    QDomDocument doc(parent.ownerDocument());

    if(!f_label.isEmpty())
    {
        QDomElement label(doc.createElement("label"));
        label.setAttribute("for", f_name);
        parent.appendChild(label);

        //QDomText label_text(doc.createTextNode(f_label));
        //label.appendChild(label_text);
        snap::snap_dom::insert_html_string_to_xml_doc(label, f_label);
    }

    QDomElement input(doc.createElement("input"));
    input.setAttribute("type", "input"); // be explicit
    input.setAttribute("name", f_name);
    input.setAttribute("value", f_value);
    input.setAttribute("id", f_name); // names have to be unique so this is enough for id
    parent.appendChild(input);

    if(!f_description.isEmpty())
    {
        QDomElement p(doc.createElement("p"));
        p.setAttribute("class", "description");
        parent.appendChild(p);

        //QDomText p_text(doc.createTextNode(f_description));
        //p.appendChild(p_text);
        snap::snap_dom::insert_html_string_to_xml_doc(p, f_description);
    }

}


form::form(QString const & plugin_name, QString const & field_name, button_t buttons)
    : f_plugin_name(plugin_name)
    , f_field_name(field_name)
    , f_buttons(buttons)
{
}


void form::generate(QDomElement parent)
{
    QDomDocument doc(parent.ownerDocument());

    // create the form tag
    //
    QDomElement form_tag(doc.createElement("form"));
    parent.appendChild(form_tag);

    // add the plugin name and field name as hidden fields
    //
    QDomElement plugin_name_tag(doc.createElement("input"));
    plugin_name_tag.setAttribute("type", "hidden");
    plugin_name_tag.setAttribute("value", f_plugin_name);
    form_tag.appendChild(plugin_name_tag);

    QDomElement field_name_tag(doc.createElement("input"));
    field_name_tag.setAttribute("type", "hidden");
    field_name_tag.setAttribute("value", f_field_name);
    form_tag.appendChild(field_name_tag);

    // add the widgets defined by the caller
    //
    std::for_each(f_widgets.begin(), f_widgets.end(), [&form_tag](auto const & w) { w->generate(form_tag); });

    // add reset and save buttons
    //
    if((f_buttons & FORM_BUTTON_RESET) != 0)
    {
        QDomElement button(doc.createElement("button"));
        button.setAttribute("type", "reset");
        form_tag.appendChild(button);

        QDomText text(doc.createTextNode("Reset"));
        button.appendChild(text);
    }
    if((f_buttons & FORM_BUTTON_SAVE) != 0)
    {
        QDomElement button(doc.createElement("button"));
        button.setAttribute("type", "submit");
        button.setAttribute("name", "save");
        form_tag.appendChild(button);

        QDomText text(doc.createTextNode("Save"));
        button.appendChild(text);
    }
    if((f_buttons & FORM_BUTTON_SAVE_EVERYWHERE) != 0)
    {
        QDomElement button(doc.createElement("button"));
        button.setAttribute("type", "submit");
        button.setAttribute("name", "save_everywhere");
        form_tag.appendChild(button);

        QDomText text(doc.createTextNode("Save Everywhere"));
        button.appendChild(text);
    }
    if((f_buttons & FORM_BUTTON_RESTORE_DEFAULT) != 0)
    {
        QDomElement button(doc.createElement("button"));
        button.setAttribute("type", "submit");
        button.setAttribute("name", "restore_default");
        form_tag.appendChild(button);

        QDomText text(doc.createTextNode("Restore Default"));
        button.appendChild(text);
    }
}


void form::add_widget(widget::pointer_t w)
{
    f_widgets.push_back(w);
}



} // namespace snap_manager
// vim: ts=4 sw=4 et
