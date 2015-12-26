// Snap Websites Server -- password policy handling
// Copyright (C) 2012-2015  Made to Order Software Corp.
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

/** \file
 * \brief Users security check structure handling.
 *
 * This file is the implementation of the user_security_t class used
 * to check whether a user is considered valid before registering him
 * or sending an email to him.
 */

#include "password.h"

#include "poison.h"


SNAP_PLUGIN_EXTENSION_START(password)


/** \brief The policy to use with this object.
 *
 * The constructor loads the policy specified by name. If you do not
 * specify a policy name (i.e. use an emptry string, "") then the
 * initialization is not applied.
 *
 * \param[in] policy_name  The name of the policy to load.
 */
policy_t::policy_t(QString const & policy_name)
{
    if(!policy_name.isEmpty()
    && policy_name != "blacklist")
    {
        content::content * content_plugin(content::content::instance());
        QtCassandra::QCassandraTable::pointer_t revision_table(content_plugin->get_revision_table());

        // load the policy from the database
        content::path_info_t settings_ipath;
        settings_ipath.set_path(QString("admin/settings/password/%1").arg(policy_name));
        QtCassandra::QCassandraRow::pointer_t settings_row(revision_table->row(settings_ipath.get_revision_key()));

        f_limit_duration               = settings_row->cell(get_name(name_t::SNAP_NAME_PASSWORD_LIMIT_DURATION))->value().safeSignedCharValue(0, 0) != 0;
        f_maximum_duration             = settings_row->cell(get_name(name_t::SNAP_NAME_PASSWORD_MAXIMUM_DURATION))->value().safeInt64Value(0, 92);
        f_minimum_length               = settings_row->cell(get_name(name_t::SNAP_NAME_PASSWORD_MINIMUM_LENGTH))->value().safeInt64Value(0, 0);
        f_minimum_lowercase_letters    = settings_row->cell(get_name(name_t::SNAP_NAME_PASSWORD_MINIMUM_LOWERCASE_LETTERS))->value().safeInt64Value(0, 0);
        f_minimum_uppercase_letters    = settings_row->cell(get_name(name_t::SNAP_NAME_PASSWORD_MINIMUM_UPPERCASE_LETTERS))->value().safeInt64Value(0, 0);
        f_minimum_letters              = settings_row->cell(get_name(name_t::SNAP_NAME_PASSWORD_MINIMUM_LETTERS))->value().safeInt64Value(0, 0);
        f_minimum_digits               = settings_row->cell(get_name(name_t::SNAP_NAME_PASSWORD_MINIMUM_DIGITS))->value().safeInt64Value(0, 0);
        f_minimum_spaces               = settings_row->cell(get_name(name_t::SNAP_NAME_PASSWORD_MINIMUM_SPACES))->value().safeInt64Value(0, 0);
        f_minimum_special              = settings_row->cell(get_name(name_t::SNAP_NAME_PASSWORD_MINIMUM_SPECIAL))->value().safeInt64Value(0, 0);
        f_minimum_unicode              = settings_row->cell(get_name(name_t::SNAP_NAME_PASSWORD_MINIMUM_UNICODE))->value().safeInt64Value(0, 0);
        f_minimum_variation            = settings_row->cell(get_name(name_t::SNAP_NAME_PASSWORD_MINIMUM_VARIATION))->value().safeInt64Value(0, 0);
        f_minimum_length_of_variations = settings_row->cell(get_name(name_t::SNAP_NAME_PASSWORD_MINIMUM_LENGTH_OF_VARIATIONS))->value().safeInt64Value(0, 1);
        f_check_blacklist              = settings_row->cell(get_name(name_t::SNAP_NAME_PASSWORD_CHECK_BLACKLIST))->value().safeSignedCharValue(0, 0) != 0;
        f_check_username               = settings_row->cell(get_name(name_t::SNAP_NAME_PASSWORD_CHECK_USERNAME))->value().safeInt64Value(0, 1);
        f_check_username_reversed      = settings_row->cell(get_name(name_t::SNAP_NAME_PASSWORD_CHECK_USERNAME_REVERSED))->value().safeSignedCharValue(0, 1) != 0;
        f_prevent_old_passwords        = settings_row->cell(get_name(name_t::SNAP_NAME_PASSWORD_PREVENT_OLD_PASSWORDS))->value().safeSignedCharValue(0, 0) != 0;
        f_minimum_old_passwords        = settings_row->cell(get_name(name_t::SNAP_NAME_PASSWORD_MINIMUM_OLD_PASSWORDS))->value().safeInt64Value(0, 1);
        f_old_passwords_maximum_age    = settings_row->cell(get_name(name_t::SNAP_NAME_PASSWORD_OLD_PASSWORDS_MAXIMUM_AGE))->value().safeInt64Value(0, 365);
    }
}


/** \brief Count the characters of a password.
 *
 * The policy structure is used to either load a policy (see constructor)
 * or to count the characters found in a user password (this function.)
 *
 * In order to use the policy_t class for a password count instead of
 * a policy loaded from the database, one calls this function.
 *
 * \param[in] user_password  The password the user entered.
 */
void policy_t::count_password_characters(QString const & user_password)
{
    // count the various types of characters
    f_minimum_length = user_password.length();

    for(QChar const * s(user_password.constData()); !s->isNull(); ++s)
    {
//SNAP_LOG_WARNING("character ")(static_cast<char>(s->unicode))(" -- category ")(static_cast<int>(s->category()));
        switch(s->category())
        {
        case QChar::Letter_Lowercase:
        case QChar::Letter_Other:
            ++f_minimum_letters;
            ++f_minimum_lowercase_letters;
            break;

        case QChar::Letter_Uppercase:
        case QChar::Letter_Titlecase:
            ++f_minimum_letters;
            ++f_minimum_uppercase_letters;
            break;

        case QChar::Number_DecimalDigit:
        case QChar::Number_Letter:
        case QChar::Number_Other:
            ++f_minimum_digits;
            break;

        case QChar::Mark_SpacingCombining:
        case QChar::Separator_Space:
        case QChar::Separator_Line:
        case QChar::Separator_Paragraph:
            ++f_minimum_spaces;
            ++f_minimum_special; // it is also considered special
            break;

        default:
            if(s->unicode() < 0x100)
            {
                ++f_minimum_special;
            }
            break;

        }

        if(s->unicode() >= 0x100)
        {
            ++f_minimum_unicode;
        }
    }
}


/** \brief Whether to limit the lifespan of a password.
 *
 * A password policy offers you to define a duration after which
 * a user has to replace his password. By default passwords last
 * forever and thus this function returns false.
 *
 * \return true if the duration is effective.
 */
bool policy_t::get_limit_duration() const
{
    return f_limit_duration;
}


/** \brief Retrieve the number of days a password lasts.
 *
 * This function defines a number of days the passwords managed
 * by this policy lasts. By default this is set to 92 days.
 * The function forces the duration to a minimum of 7 days
 * (1 week.)
 *
 * \return The maximum duration of a password managed by this policy.
 */
int64_t policy_t::get_maximum_duration() const
{
    return std::min(static_cast<int64_t>(7LL), f_maximum_duration);
}


/** \brief The minimum number of characters.
 *
 * When loading the policy from the database, this is the
 * miminum number of characters that must exist in the
 * password, counting hidden characters like 0xFEFF.
 *
 * When counting the characters of a password, this is the
 * total number of characters found.
 *
 * \return The minimum number of characters in a password.
 */
int64_t policy_t::get_minimum_length() const
{
    return f_minimum_length;
}


/** \brief The minimum number of lowercase letters characters.
 *
 * When loading the policy from the database, this is the
 * number of lowercase letters characters that must exist in the
 * password.
 *
 * When counting the characters of a password, this is the
 * set of lowercase letters characters found.
 *
 * \return The minimum number of lowercase letters characters.
 */
int64_t policy_t::get_minimum_lowercase_letters() const
{
    return f_minimum_lowercase_letters;
}


/** \brief The minimum number of uppercase letters characters.
 *
 * When loading the policy from the database, this is the
 * number of uppercase letters characters that must exist in the
 * password.
 *
 * When counting the characters of a password, this is the
 * set of uppercase letters characters found.
 *
 * \return The minimum number of uppercase letters characters.
 */
int64_t policy_t::get_minimum_uppercase_letters() const
{
    return f_minimum_uppercase_letters;
}


/** \brief The minimum number of letters characters.
 *
 * When loading the policy from the database, this is the
 * number of letters characters that must exist in the
 * password.
 *
 * When counting the characters of a password, this is the
 * set of letters characters found.
 *
 * \note
 * Letters in this context is any Uncode character that
 * resolves as a letter, whether uppercase or lowercase.
 *
 * \return The minimum number of letters characters.
 */
int64_t policy_t::get_minimum_letters() const
{
    return f_minimum_letters;
}


/** \brief The minimum number of digits characters.
 *
 * When loading the policy from the database, this is the
 * number of digits characters that must exist in the
 * password.
 *
 * When counting the characters of a password, this is the
 * set of digits characters found.
 *
 * \note
 * Any character considered a digit by Unicode is counted
 * as such. So it does not need to be '0' to '9' from
 * the ASCII range (byte codes 0x30 to 0x39.)
 *
 * \return The minimum number of digits characters.
 */
int64_t policy_t::get_minimum_digits() const
{
    return f_minimum_digits;
}


/** \brief The minimum number of spaces characters.
 *
 * When loading the policy from the database, this is the
 * number of spaces characters that must exist in the
 * password.
 *
 * When counting the characters of a password, this is the
 * set of spaces characters found.
 *
 * \note
 * Any Unicode character viewed as a space is counted as
 * such. This is not limiteed to character 0x20.
 *
 * \return The minimum number of spaces characters.
 */
int64_t policy_t::get_minimum_spaces() const
{
    return f_minimum_spaces;
}


/** \brief The minimum number of special characters.
 *
 * When loading the policy from the database, this is the
 * number of special characters that must exist in the
 * password.
 *
 * When counting the characters of a password, this is the
 * set of special characters found.
 *
 * \return The minimum number of special characters.
 */
int64_t policy_t::get_minimum_special() const
{
    return f_minimum_special;
}


/** \brief The minimum number of unicode characters.
 *
 * When loading the policy from the database, this is the
 * number of Unicode characters that must exist in the
 * password.
 *
 * When counting the characters of a password, this is the
 * set of Unicode characters found (i.e. any character with
 * a code over 0x0100.)
 *
 * \return The minimum number of Unicode characters.
 */
int64_t policy_t::get_minimum_unicode() const
{
    return f_minimum_unicode;
}


/** \brief The minimum number of character sets to use in a password.
 *
 * Instead of forcing your users to have certain types of characters,
 * you may instead ask them to use a certain number of types, of
 * any of the available types.
 *
 * Note that you may force people to use lowercase letters with
 * a minimum there of 1 or 2 and have a minimum variation of 3
 * so another 2 sets of characters need to be used (uppercase
 * and digits, special characters and unicode, etc.)
 *
 * \note
 * It does not make much sense to use 1 as the minimum variation
 * since the whole set of characters is always available anyway.
 *
 * \return The minimum number of variations that must exist in a password.
 */
int64_t policy_t::get_minimum_variation() const
{
    return f_minimum_variation;
}


/** \brief The minimum length of each variation.
 *
 * When using a minimum variation of 1 or more, this minimum length
 * is used to make sure that each concerned variation is long enough.
 *
 * \return The minimum length of each variation.
 */
int64_t policy_t::get_minimum_length_of_variations() const
{
    return f_minimum_length_of_variations;
}


/** \brief Check whether the blacklist should be looked up.
 *
 * This function returns true if the blacklist should be looked up
 * when a new password is being defined by a user. By default this
 * is false, although it certain is a good idea to check because
 * those lists are known by hackers and thus these passwords will
 * be checked against your Snap! websites, over and over again.
 *
 * \return Whether the blacklist should be checked.
 */
bool policy_t::get_check_blacklist() const
{
    return f_check_blacklist;
}


/** \brief Whether we should check if the username is included in a password.
 *
 * This function returns a Levenshtein distance that needs to NOT be matched
 * to prove that the password does not include the username of the user
 * trying to register to this website.
 *
 * This function returns 2 by default. If the check username is set to
 * zero then the test is skipped.
 *
 * \return The Levenshtein distance to check the username in a password.
 */
int64_t policy_t::get_check_username() const
{
    return f_check_username;
}


/** \brief Whether the username test should also be done in reversed order.
 *
 * The username is first checked in the normal order (i.e. "alexis") and
 * when this flag is true, it is also checked in reverse order
 * (i.e. "sixela".) Some people do that thinking it is a good method to
 * make a password quite safe and really it is not.
 *
 * This function returns true by default.
 *
 * Note that if the get_check_username() function returns zero then this
 * flag is ignored.
 *
 * \return true if the reversed username needs to be checked.
 */
bool policy_t::get_check_username_reversed() const
{
    return f_check_username_reversed;
}


/** \brief Prevent old password reuse if true.
 *
 * This function returns true if the administrator requested that
 * old password be forbidden.
 *
 * By default this flag is false as old password are not forbidden.
 *
 * \return true if the old password is forbidden.
 */
bool policy_t::get_prevent_old_passwords() const
{
    return f_prevent_old_passwords;
}


/** \brief Minimum number of old password to keep around.
 *
 * When a user changes his password only at the requested time,
 * the total number of password may be smaller than the minimum
 * you want to forbid. This number is used in that case.
 *
 * Someone who changes their password more often may get older
 * password removed sooner.
 *
 * \return The minimum number of password to keep.
 */
int64_t policy_t::get_minimum_old_passwords() const
{
    return std::max(static_cast<int64_t>(1LL), f_minimum_old_passwords);
}


/** \brief The maximum age of a password to keep around.
 *
 * The system will keep a minimum number of password equal to the
 * get_minimum_old_passwords(), whatever their age. Once the
 * minimum number is reached, password that are older than what
 * this function returns (i.e. start date minus password old age)
 * get removed.
 *
 * \return The maximum age of a password in days.
 */
int64_t policy_t::get_old_passwords_maximum_age() const
{
    return std::max(static_cast<int64_t>(7LL), f_old_passwords_maximum_age);
}


/** \brief Check whether a policy is smaller than the other.
 *
 * This function checks whether the left hand side (this) has
 * any of its minimum parameters which is smaller than the
 * right hand side (rhs) policy. If so, then the function
 * returns an error message in the string.
 *
 * If the left is larger or equal, then the function returns
 * an empty string.
 *
 * This is used to compare  password against a policy loaded
 * from the database.
 *
 * \code
 *      policy_t pp("protected-nodes");
 *
 *      policy_t up;
 *      up.count_password_characters(user_password);
 *
 *      QString const r(up.compare(pp));
 *      if(r.isEmpty())
 *      {
 *          // password characters have the expected mix!
 *      }
 *      else
 *      {
 *          // password strength too weak
 *          // "r" is the message about what is missing
 *      }
 * \endcode
 *
 * \param[in] rhs  The policy to check the password against.
 */
QString policy_t::compare(policy_t const & rhs) const
{
    // enough lowercase letters?
    if(f_minimum_lowercase_letters < rhs.f_minimum_lowercase_letters)
    {
        return "not enough lowercase letter characters";
    }

    // enough uppercase letters?
    if(f_minimum_uppercase_letters < rhs.f_minimum_uppercase_letters)
    {
        return "not enough uppercase letter characters";
    }

    // enough letters?
    if(f_minimum_letters < rhs.f_minimum_letters)
    {
        return "not enough letter characters";
    }

    // enough digits?
    if(f_minimum_digits < rhs.f_minimum_digits)
    {
        return "not enough digit characters";
    }

    // enough spaces?
    if(f_minimum_spaces < rhs.f_minimum_spaces)
    {
        return "not enough space characters";
    }

    // enough special?
    if(f_minimum_special < rhs.f_minimum_special)
    {
        return "not enough special characters";
    }

    // enough unicode?
    if(f_minimum_unicode < rhs.f_minimum_unicode)
    {
        return "not enough unicode characters";
    }

    // variation check?
    if(rhs.f_minimum_variation > 0)
    {
        // variations are not yet calculated
        //
        // here we need to get the f_minimum_variation largest variations
        // and then make sure those are large enough
        //
        std::vector<int64_t> set;
        if(f_minimum_lowercase_letters > 0)
        {
            set.push_back(f_minimum_lowercase_letters);
        }
        if(f_minimum_uppercase_letters > 0)
        {
            set.push_back(f_minimum_uppercase_letters);
        }
        // In this case, this does not make sense because it is going
        // to be lowercase + uppercase letters and not something
        // really distinct
        //if(f_minimum_letters > 0)
        //{
        //    set.push_back(f_minimum_letters);
        //}
        if(f_minimum_digits > 0)
        {
            set.push_back(f_minimum_digits);
        }
        if(f_minimum_spaces > 0)
        {
            set.push_back(f_minimum_spaces);
        }
        if(f_minimum_special > 0)
        {
            set.push_back(f_minimum_special);
        }
        if(f_minimum_unicode > 0)
        {
            set.push_back(f_minimum_unicode);
        }

        // enough variation?
        //
        if(set.size() < static_cast<size_t>(rhs.f_minimum_variation))
        {
            return "not enough different category of characters used";
        }

        // sort so the largest are first
        // (notice the 'r' to reverse the order)
        //
        std::sort(set.rbegin(), set.rend());
        for(size_t idx(0); idx < static_cast<size_t>(rhs.f_minimum_variation); ++idx)
        {
            if(set[idx] < rhs.f_minimum_length_of_variations)
            {
                return "not enough characters in each category of characters";
            }
        }
    }

    // password is all good
    return QString();
}


/** \brief Check whether the user password is blacklisted.
 *
 * Our system maintains a list of words that we want to forbid
 * users from ever entering as passwords because they are known
 * by hackers and thus not useful as a security token.
 *
 * \todo
 * Later we may have degrees of blacklisted password, i.e. we may
 * still authorize some of those if they pass the policy rules.
 *
 * \param[in] user_password  The password to be checked.
 *
 * \return An error message if the password is blacklisted.
 */
QString policy_t::is_blacklisted(QString const & user_password) const
{
    // also check against the blacklist?
    //
    if(f_check_blacklist)
    {
        // the password has to be the row name to be spread on all nodes
        //
        // later we may use columns to define whether a password 100%
        // forbidden (password1,) "mostly" forbidden (complex enough
        // for the current policy,) etc.
        //
        QtCassandra::QCassandraTable::pointer_t table(password::password::instance()->get_password_table());
        if(table->exists(user_password.toLower()))
        {
            return "this password is blacklisted and cannot be used";
        }
    }

    // not black listed
    return QString();
}


SNAP_PLUGIN_EXTENSION_END()

// vim: ts=4 sw=4 et
