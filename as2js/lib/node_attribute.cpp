/* node_attribute.cpp -- written by Alexis WILKE for Made to Order Software Corp. (c) 2005-2014 */

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

#include    "as2js/node.h"

#include    "as2js/exceptions.h"
#include    "as2js/message.h"

/** \file
 * \brief Implementation of the Node class attributes.
 *
 * Node objects support a large set of attributes. Attributes can be added
 * and removed from a Node. Some attributes are mutually exclusive.
 */


namespace as2js
{



/**********************************************************************/
/**********************************************************************/
/***  NODE ATTRIBUTE  *************************************************/
/**********************************************************************/
/**********************************************************************/

/** \brief Anonymous namespace for attribute internal definitions.
 *
 * The Node attributes are organized in groups. In most cases, only one
 * attribute from the same group can be set at a time. Trying to set
 * another attribute from the same group generates an error.
 *
 * The tables defined here are used to determine whether attributes are
 * mutually exclusive.
 *
 * Note that such errors are considered to be bugs in the compiler. The
 * implementation needs to be fixed if such errors are detected.
 */
namespace
{

/** \brief List of attribute groups.
 *
 * The following enumeration defines a set of group attributes. These
 * are used internally to declare the list of attribute groups.
 *
 * \todo
 * Add a class name to this enumeration, even if private, it still
 * makes it a lot safer.
 */
enum
{
    /** \brief Conditional Compilation Group.
     *
     * This group includes the TRUE and FALSE attributes. A statement
     * can be marked as TRUE (compiled in) or FALSE (left out). A
     * statement cannot at the same time be TRUE and FALSE.
     */
    ATTRIBUTES_GROUP_CONDITIONAL_COMPILATION,

    /** \brief Function Type Group.
     *
     * Functions can be marked as ABSTRACT, CONSTRUCTOR, INLINE, NATIVE,
     * STATIC, and VIRTUAL. This group is used to detect whether a function
     * is marked by more than one of these attributes.
     *
     * Note that this group has exceptions:
     *
     * \li A NATIVE CONSTRUCTOR is considered valid.
     * \li A NATIVE VIRTUAL is considered valid.
     * \li A NATIVE STATIC is considered valid.
     * \li A STATIC INLINE is considered valid.
     */
    ATTRIBUTES_GROUP_FUNCTION_TYPE,

    /** \brief Function Contract Group.
     *
     * The function contract includes the REQUIRE ELSE and the
     * ENSURE THEN, both of which cannot be assigned to one
     * function simultaneously.
     *
     * Contracts are taken from the Effel language.
     */
    ATTRIBUTES_GROUP_FUNCTION_CONTRACT,

    /** \brief Switch Type Group.
     *
     * A 'switch' statement can be given a type: FOREACH, NOBREAK,
     * or AUTOBREAK. Only one type can be specified.
     *
     * The AUTOBREAK idea comes from languages such as Ada and
     * Visual BASIC which always break at the end of a case.
     */
    ATTRIBUTES_GROUP_SWITCH_TYPE,

    /** \brief Member Visibility Group.
     *
     * Variable and function members defined in a class can be given a
     * specific visibility of PUBLIC, PRIVATE, or PROTECTED.
     *
     * All the visibilities are mutually exclusive.
     *
     * Note that the visibility capability can either use a direct
     * attribute definition or a 'label' definition (as in C++).
     * The 'label' definition is ignored when a direct attribute is
     * used, in other words, the visibility can be contradictory in
     * that case and the compiler still accepts the entry (TBD.)
     */
    ATTRIBUTES_GROUP_MEMBER_VISIBILITY
};


/** \brief Table of group names.
 *
 * This table defines a set of names for the attribute groups. These
 * are used whenever an error is generated in link with that given
 * group.
 *
 * The index makes use of the group enumeration values.
 */
char const *g_attribute_groups[] =
{
    [ATTRIBUTES_GROUP_CONDITIONAL_COMPILATION] = "true and false",
    [ATTRIBUTES_GROUP_FUNCTION_TYPE] = "abstract, constructor, inline, native, static, and virtual",
    [ATTRIBUTES_GROUP_FUNCTION_CONTRACT] = "require else and ensure then",
    [ATTRIBUTES_GROUP_SWITCH_TYPE] = "foreach, nobreak, and autobreak",
    [ATTRIBUTES_GROUP_MEMBER_VISIBILITY] = "public, private, and protected"
};

}
// no name namesoace


/** \brief Get the current status of an attribute.
 *
 * This function returns true or false depending on the current status
 * of the specified attribute.
 *
 * The function verifies that the specified attribute (\p a) corresponds to
 * the type of data you are dealing with. If not, an exception is raised.
 *
 * If the attribute was never set, this function returns false.
 *
 * \note
 * All attributes are always considered false by default.
 *
 * \param[in] a  The attribute to retrieve.
 *
 * \return true if the attribute was set, false otherwise.
 *
 * \sa set_attribute()
 * \sa verify_attribute()
 */
bool Node::get_attribute(attribute_t a) const
{
    verify_attribute(a);
    return f_attributes[static_cast<size_t>(a)];
}


/** \brief Set an attribute.
 *
 * This function sets the specified attribute \p a to the specified value
 * \p v in this Node object.
 *
 * The function verifies that the specified attribute (\p a) corresponds to
 * the type of data you are dealing with.
 *
 * \param[in] a  The flag to set.
 * \param[in] v  The new value for the flag.
 *
 * \sa get_attribute()
 * \sa verify_attribute()
 * \sa verify_exclusive_attributes()
 */
void Node::set_attribute(attribute_t a, bool v)
{
    verify_attribute(a);
    if(v)
    {
        // exclusive attributes do not generate an exception, instead
        // we test the return value and if two exclusive attribute flags
        // were to be set simultaneously, we prevent the second one from
        // being set
        if(!verify_exclusive_attributes(a))
        {
            return;
        }
    }
    f_attributes[static_cast<size_t>(a)] = v;
}


/** \brief Verify that \p a corresponds to the Node type.
 *
 * This function verifies that \p a corresponds to a valid attribute
 * according to the type of this Node object.
 *
 * \note
 * At this point attributes can be assigned to any type of node
 * exception a NODE_PROGRAM which only accepts the NODE_ATTR_DEFINED
 * attribute.
 *
 * \exception exception_internal_error
 * If the attribute is not valid for this node type,
 * then this exception is raised.
 *
 * \param[in] a  The attribute to check.
 *
 * \sa set_attribute()
 * \sa get_attribute()
 */
void Node::verify_attribute(attribute_t a) const
{
    switch(a)
    {
    // member visibility
    case attribute_t::NODE_ATTR_PUBLIC:
    case attribute_t::NODE_ATTR_PRIVATE:
    case attribute_t::NODE_ATTR_PROTECTED:
    case attribute_t::NODE_ATTR_INTERNAL:
    case attribute_t::NODE_ATTR_TRANSIENT:
    case attribute_t::NODE_ATTR_VOLATILE:

    // function member type
    case attribute_t::NODE_ATTR_STATIC:
    case attribute_t::NODE_ATTR_ABSTRACT:
    case attribute_t::NODE_ATTR_VIRTUAL:
    case attribute_t::NODE_ATTR_ARRAY:
    case attribute_t::NODE_ATTR_INLINE:

    // function contracts
    case attribute_t::NODE_ATTR_REQUIRE_ELSE:
    case attribute_t::NODE_ATTR_ENSURE_THEN:

    // function/variable is defined in your system (execution env.)
    case attribute_t::NODE_ATTR_NATIVE:

    // function/variable will be removed in future releases, do not use
    case attribute_t::NODE_ATTR_DEPRECATED:
    case attribute_t::NODE_ATTR_UNSAFE:

    // operator overload (function member)
    case attribute_t::NODE_ATTR_CONSTRUCTOR:

    // function & member constrains
    case attribute_t::NODE_ATTR_FINAL:
    case attribute_t::NODE_ATTR_ENUMERABLE:

    // conditional compilation
    case attribute_t::NODE_ATTR_TRUE:
    case attribute_t::NODE_ATTR_FALSE:
    case attribute_t::NODE_ATTR_UNUSED:                      // if definition is used, error!

    // class attribute (whether a class can be enlarged at run time)
    case attribute_t::NODE_ATTR_DYNAMIC:

    // switch attributes
    case attribute_t::NODE_ATTR_FOREACH:
    case attribute_t::NODE_ATTR_NOBREAK:
    case attribute_t::NODE_ATTR_AUTOBREAK:
        // TBD -- we'll need to see whether we want to limit the attributes
        //        on a per node type basis and how we can do that properly
        if(f_type != node_t::NODE_PROGRAM)
        {
            return;
        }
        break;

    // attributes were defined
    case attribute_t::NODE_ATTR_DEFINED:
        // all nodes can receive this flag
        return;

    case attribute_t::NODE_ATTR_max:
        break;

    // default: -- do not define so the compiler can tell us if
    //             an enumeration is missing in this case
    }

    throw exception_internal_error("attribute / type missmatch in Node::verify_attribute()");
}


/** \brief Verify that we can indeed set an attribute.
 *
 * Many attributes are mutually exclusive. This function checks that
 * only one of a group of attributes gets set.
 *
 * This function is not called if you clear an attribute since in that
 * case the default applies.
 *
 * When attributes are found to be in conflict, it is not an internal
 * error, so instead the function generates an error message and the
 * function returns false. This means the compiler may end up generating
 * more errors than one might want to get.
 *
 * \exception exception_internal_error
 * This exception is raised whenever the parameter \p a is invalid.
 *
 * \param[in] a  The attribute being set.
 *
 * \return true if the attributes are not in conflict.
 *
 * \sa set_attribute()
 */
bool Node::verify_exclusive_attributes(attribute_t a) const
{
    bool conflict(false);
    char const *names;
    switch(a)
    {
    case attribute_t::NODE_ATTR_ARRAY:
    case attribute_t::NODE_ATTR_DEFINED:
    case attribute_t::NODE_ATTR_DEPRECATED:
    case attribute_t::NODE_ATTR_DYNAMIC:
    case attribute_t::NODE_ATTR_ENUMERABLE:
    case attribute_t::NODE_ATTR_FINAL:
    case attribute_t::NODE_ATTR_INTERNAL:
    case attribute_t::NODE_ATTR_TRANSIENT:
    case attribute_t::NODE_ATTR_UNSAFE:
    case attribute_t::NODE_ATTR_UNUSED:
    case attribute_t::NODE_ATTR_VOLATILE:
        // these attributes have no conflicts
        return true;

    // function contract
    case attribute_t::NODE_ATTR_REQUIRE_ELSE:
        conflict = f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_ENSURE_THEN)];
        names = g_attribute_groups[ATTRIBUTES_GROUP_FUNCTION_CONTRACT];
        break;

    case attribute_t::NODE_ATTR_ENSURE_THEN:
        conflict = f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_REQUIRE_ELSE)];
        names = g_attribute_groups[ATTRIBUTES_GROUP_FUNCTION_CONTRACT];
        break;

    // member visibility
    case attribute_t::NODE_ATTR_PUBLIC:
        conflict = f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_PRIVATE)]
                || f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_PROTECTED)];
        names = g_attribute_groups[ATTRIBUTES_GROUP_MEMBER_VISIBILITY];
        break;

    case attribute_t::NODE_ATTR_PRIVATE:
        conflict = f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_PUBLIC)]
                || f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_PROTECTED)];
        names = g_attribute_groups[ATTRIBUTES_GROUP_MEMBER_VISIBILITY];
        break;

    case attribute_t::NODE_ATTR_PROTECTED:
        conflict = f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_PUBLIC)]
                || f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_PRIVATE)];
        names = g_attribute_groups[ATTRIBUTES_GROUP_MEMBER_VISIBILITY];
        break;

    // function type group
    case attribute_t::NODE_ATTR_ABSTRACT:
        conflict = f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_STATIC)]
                || f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_CONSTRUCTOR)]
                || f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_VIRTUAL)]
                || f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_NATIVE)]
                || f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_INLINE)];
        names = g_attribute_groups[ATTRIBUTES_GROUP_FUNCTION_TYPE];
        break;

    case attribute_t::NODE_ATTR_CONSTRUCTOR:
        conflict = f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_STATIC)]
                || f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_VIRTUAL)]
                || f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_INLINE)]
                || f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_ABSTRACT)];
        names = g_attribute_groups[ATTRIBUTES_GROUP_FUNCTION_TYPE];
        break;

    case attribute_t::NODE_ATTR_INLINE:
        conflict = f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_ABSTRACT)]
                || f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_CONSTRUCTOR)]
                || f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_NATIVE)]
                || f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_VIRTUAL)];
        names = g_attribute_groups[ATTRIBUTES_GROUP_FUNCTION_TYPE];
        break;

    case attribute_t::NODE_ATTR_NATIVE:
        conflict = f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_ABSTRACT)]
                || f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_INLINE)];
        names = g_attribute_groups[ATTRIBUTES_GROUP_FUNCTION_TYPE];
        break;

    case attribute_t::NODE_ATTR_STATIC:
        conflict = f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_ABSTRACT)]
                || f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_CONSTRUCTOR)]
                || f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_VIRTUAL)];
        names = g_attribute_groups[ATTRIBUTES_GROUP_FUNCTION_TYPE];
        break;

    case attribute_t::NODE_ATTR_VIRTUAL:
        conflict = f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_STATIC)]
                || f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_CONSTRUCTOR)]
                || f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_ABSTRACT)]
                || f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_INLINE)];
        names = g_attribute_groups[ATTRIBUTES_GROUP_FUNCTION_TYPE];
        break;

    // switch type group
    case attribute_t::NODE_ATTR_FOREACH:
        conflict = f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_NOBREAK)]
                || f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_AUTOBREAK)];
        names = g_attribute_groups[ATTRIBUTES_GROUP_SWITCH_TYPE];
        break;

    case attribute_t::NODE_ATTR_NOBREAK:
        conflict = f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_FOREACH)]
                || f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_AUTOBREAK)];
        names = g_attribute_groups[ATTRIBUTES_GROUP_SWITCH_TYPE];
        break;

    case attribute_t::NODE_ATTR_AUTOBREAK:
        conflict = f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_FOREACH)]
                || f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_NOBREAK)];
        names = g_attribute_groups[ATTRIBUTES_GROUP_SWITCH_TYPE];
        break;

    // conditional compilation group
    case attribute_t::NODE_ATTR_TRUE:
        conflict = f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_FALSE)];
        names = g_attribute_groups[ATTRIBUTES_GROUP_CONDITIONAL_COMPILATION];
        break;

    case attribute_t::NODE_ATTR_FALSE:
        conflict = f_attributes[static_cast<size_t>(attribute_t::NODE_ATTR_TRUE)];
        names = g_attribute_groups[ATTRIBUTES_GROUP_CONDITIONAL_COMPILATION];
        break;

    case attribute_t::NODE_ATTR_max:
        // this should already have been caught in the verify_attribute() function
        throw exception_internal_error("invalid attribute / flag in Node::verify_attribute()"); // LCOV_EXCL_LINE

    // default: -- do not define so the compiler can tell us if
    //             an enumeration is missing in this case
    // note: verify_attribute(() is called before this function
    //       and it catches completely invalid attribute numbers
    //       (i.e. negative, larger than NODE_ATTR_max)
    }

    if(conflict)
    {
        // this can be a user error so we emit an error instead of throwing
        Message msg(message_level_t::MESSAGE_LEVEL_ERROR, err_code_t::AS_ERR_INVALID_ATTRIBUTES, f_position);
        msg << "Attributes " << names << " are mutually exclusive. Only one of them can be used.";
        return false;
    }

    return true;
}


/** \brief Compare a set of attributes with the current attributes of this node.
 *
 * This function compares the specified set of attributes with the node's
 * attributes. If the sets are equal, then the function returns true.
 * Otherwise the function returns false.
 *
 * This function compares all the attributes, whether or not they are
 * valid for the current node type.
 *
 * \param[in] s  The set of attributes to compare with.
 *
 * \return true if \p s is equal to the node attributes.
 */
bool Node::compare_all_attributes(attribute_set_t const& s) const
{
    return f_attributes == s;
}


}
// namespace as2js

// vim: ts=4 sw=4 et
