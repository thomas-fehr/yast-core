/*---------------------------------------------------------------------\
|								       |
|		       __   __	  ____ _____ ____		       |
|		       \ \ / /_ _/ ___|_   _|___ \		       |
|			\ V / _` \___ \ | |   __) |		       |
|			 | | (_| |___) || |  / __/		       |
|			 |_|\__,_|____/ |_| |_____|		       |
|								       |
|				core system			       |
|							 (C) SuSE GmbH |
\----------------------------------------------------------------------/

   File:	YCPString.h

   Author:	Mathias Kettner <kettner@suse.de>
   Maintainer:	Arvin Schnell <arvin@suse.de>

/-*/
// -*- c++ -*-

#ifndef YCPString_h
#define YCPString_h


#include "YCPValue.h"


/**
 * @short YCPValueRep representing a character string of
 * arbitrary length.
 * Whatever internal representation is choosen: It should
 * be possible to switch to 16 bit Unicode strings, when
 * neccessary.
 *
 * YCP Syntax: Doublequoted ASCII string.
 * <pre>"This is a string", "", "08712345"</pre>
 */
class YCPStringRep : public YCPValueRep
{
    string v;

protected:

    friend class YCPString;

    /**
     * Creates a new YCPStringRep from a C++ string.
     * @param s A string that is taken literally as value of the newly
     * create YCPStringRep object. Not expansion of backslashes is done,
     * s is not considered to be enclosed with quotes. If there are quotes,
     * they are considered to be part of the string.
     */
    YCPStringRep (string s);

    /**
     * Creates a new YCPStringRep from a C string.
     * @param s A string that is taken literally as value of the newly
     * create YCPStringRep object. Not expansion of backslashes is done,
     * s is not considered to be enclosed with quotes. If there are quotes,
     * they are considered to be part of the string. NULL is treated as
     * an empty string.
     */
    YCPStringRep (const char* s);

public:

    /**
     * Returns the value of this object in form of a C++ string.
     */
    string value () const;

    /**
     * Returns the value in form of a C const char* string.
     */
    const char* value_cstr () const;

    /**
     * Compares two YCPStrings for equality, greaterness or smallerness.
     * @param v value to compare against
     * @return YO_LESS,    if this is smaller than v,
     *         YO_EQUAL,   if this is equal to v,
     *         YO_GREATER, if this is greater to v
     */
    YCPOrder compare(const YCPString &v) const;

    /**
     * Returns a string representation of the value of this
     * object. It contains enclosing quotes. Newlines and
     * quotes contained in the string itself are quoted with
     * backslashes.
     */
    string toString() const;

    /**
     * Returns YT_STRING. See @ref YCPValueRep#valuetype.
     */
    YCPValueType valuetype() const;
};

/**
 * @short Wrapper for YCPStringRep
 * This class realizes an automatic memory management
 * via @ref YCPElement. Access the functionality of YCPStringRep
 * with the arrow operator. See @ref YCPStringRep.
 */
class YCPString : public YCPValue
{
    DEF_COMMON(String, Value);
public:
    YCPString(string s) : YCPValue(new YCPStringRep(s)) {}
};

#endif   // YCPString_h
