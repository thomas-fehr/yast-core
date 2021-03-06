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

   File:       Xmlcode.h

   Primitive xmlcode I/O functions.
   Acts as a namespace wrapper.

   Author:     Klaus Kaempf <kkaempf@suse.de>
   Maintainer: Klaus Kaempf <kkaempf@suse.de>

/-*/
// -*- c++ -*-

#ifndef Xmlcode_h
#define Xmlcode_h

#include "ycp/YCPValue.h"
#include "ycp/YCode.h"
#include "ycp/YStatement.h"
#include "ycp/YBlock.h"
#include "ycp/Type.h"

class Y2Namespace;

#include <iosfwd>
#include <string>
#include <map>

#include <fstream>

/// An istream that remembers some data about the xmlcode.
class xmlcodeistream : public std::ifstream
{
	int m_major, m_minor, m_release;
    public:
	xmlcodeistream (string filename);
	bool isVersion (int major, int minor, int revision);
	bool isVersionAtMost (int major, int minor, int revision);
	
	int major () const { return m_major; }
	int minor () const { return m_minor; }
	int release () const { return m_release; }
};

/// *.ybc I/O
class Xmlcode {
    static int m_namespace_nesting_level;
    static int m_namespace_nesting_array_size;
    static int m_namespace_tare_level;
    
    /// references to namespaces
    struct namespaceentry_t {
	const Y2Namespace *name_space;
	bool with_xrefs;	///< external references... ???
    };
    static namespaceentry_t *m_namespace_nesting_array;
    static map<string, YBlockPtr>* m_xmlcodeCache;

    public:
    /** Thrown instead of unsetting YCode::valid */
    class Invalid {};

	static string spaces( int count );

	static string xmlify( const string & s );

	// bytepointer I/O
	static std::ostream & writeBytep (std::ostream & streamref, const unsigned char * bytep, unsigned int len);
	static unsigned char * readBytep (xmlcodeistream & streamref);

	// Type I/O
	static std::ostream & writeType (std::ostream & str, constTypePtr type);
	static TypePtr readType (xmlcodeistream & str);

	// ycodelist_t * I/O
	static std::ostream & writeYCodelist (std::ostream & str, const ycodelist_t *codelist);
	static bool readYCodelist (xmlcodeistream & str, ycodelist_t **anchor);

	//-----------------------------------------------------------
	// block nesting handling
	//
	static void namespaceInit ();
	// retrieve ID (nesting level) for namespace
	static int namespaceId (const Y2Namespace *name_space);
	// retrieve namespace for ID
	static const Y2Namespace *namespacePtr (int namespace_id);

	// push given namespace to id stack, return new id, -1 on error
	static int pushNamespace (const Y2Namespace *name_space, bool with_xrefs = false);

	// pop given namespace from id stack, return namespace id, -1 on error
	static int popNamespace (const Y2Namespace *name_space);

	// pop all from id stack until given namespace is reached and popped too
	static void popUptoNamespace (const Y2Namespace *name_space);

	// reset current namespace stack to 'empty' for module loading
	//   returns a tare id needed later
	static int tareStack ();
	static void untareStack (int tare_id);

	//-----------------------------------------------------------
	// SymbolEntry pointer (!) handling
	//   the SymbolEntries itself are 'owned' by Y2Namespace (YBlock in YCP) and referenced via pointers
	//   to SymbolEntry. These functions handle stream I/O for SymbolEntry pointers.
	static std::ostream &writeEntry (std::ostream & str, const SymbolEntryPtr entry);
	static SymbolEntryPtr readEntry (xmlcodeistream & str);

	//-----------------------------------------------------------
	// YCode read.
	// Must be implemented outside of YCode since we have derived classes to allocate...
	// see YCode for write
	static YCodePtr readCode (xmlcodeistream & str);

	// File I/O
	// reading and writing complete files is done via separate functions
	// which add/check a 'magic value' header denoting "YCode" and its version.

	// read YCode from file in Module path, return YBlock (NULL in case of error)
	static YBlockPtr readModule (const string & mname);

	// read YCode from file, return YCode (YError in case of failure)
	static YCodePtr readFile (const string & filename);

	// write YCode to file, return true on success (check errno for errors)
	static bool writeFile (const YCodePtr code, const string & filename);
};

#endif // Xmlcode_h
