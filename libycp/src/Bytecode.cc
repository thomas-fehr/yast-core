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

   File:       Bytecode.cc

   Author:	Klaus Kaempf <kkaempf@suse.de>
   Maintainer:	Klaus Kaempf <kkaempf@suse.de>

$Id$
/-*/

// MAJOR and MINOR number must the the same in header, RELEASE is assumed to 
// provide a backward compatibility
#define YaST_BYTECODE_HEADER "YaST bytecode "
#define YaST_BYTECODE_MAJOR "1"
#define YaST_BYTECODE_MINOR "1"
#define YaST_BYTECODE_RELEASE "1"

#include "ycp/Bytecode.h"
#include "YCP.h"
#include "ycp/YCode.h"
#include "ycp/YExpression.h"
#include "ycp/YStatement.h"
#include "ycp/YBlock.h"

#include "y2/Y2Namespace.h"

#include "ycp/y2log.h"
#include "ycp/pathsearch.h"

#include <fstream>
#include <errno.h>
#include <string.h>
#include <ctype.h>

int Bytecode::m_block_nesting_level = -1;
int Bytecode::m_block_nesting_array_size = 0;
int Bytecode::m_block_tare_level = 0;
const Y2Namespace **Bytecode::m_block_nesting_array = 0;

// ------------------------------------------------------------------
// bool I/O

std::ostream & 
Bytecode::writeBool (std::ostream & str, const bool value)
{
    return str.put ((char)(value?'\x01':'\x00'));
}


bool
Bytecode::readBool (std::istream & str)
{
    char c;
    str.get (c);
//    y2debug ("Bytecode::readBool 0x%02x", (unsigned int)c);

    if (c != 0)
    {
	return true;
    }
    return false;
}


// ------------------------------------------------------------------
// u_int32_t I/O

std::ostream & 
Bytecode::writeInt32 (std::ostream & str, const u_int32_t value)
{
    str.put ((char)4);
    str.put ((char)(value & 0xff));
    str.put ((char)(value>>8 & 0xff));
    str.put ((char)(value>>16 & 0xff));
    return str.put ((char)(value>>24 & 0xff));
}


u_int32_t
Bytecode::readInt32 (std::istream & str)
{
    char c;
    str.get (c);
    if (c != 4)
    {
	return false;
    }
    u_int32_t cv, value = 0;
    str.get (c); cv = (unsigned char)c; value = cv;
    str.get (c); cv = (unsigned char)c; cv <<= 8; value |= cv;
    str.get (c); cv = (unsigned char)c; cv <<= 16; value |= cv;
    str.get (c); cv = (unsigned char)c; cv <<= 24; value |= cv;
    return value;
}


// ------------------------------------------------------------------
// string I/O

std::ostream & 
Bytecode::writeString (std::ostream & streamref, const string & stringref)
{
    u_int32_t len = stringref.size();

    writeInt32 (streamref, len);
    return streamref.write (stringref.c_str(), len);
}


bool
Bytecode::readString (std::istream & streamref, string & stringref)
{
    bool ret = false;
    stringref.erase();
    u_int32_t len = readInt32 (streamref);
    if (len > 0)
    {
	char *buf = new char [len+1];
	if (streamref.read (buf, len))
	{
	    buf[len] = 0;
	    stringref = string (buf);
	    ret = true;
	}
	delete [] buf;
    }
    return ret;
}


// ------------------------------------------------------------------
// char * I/O

std::ostream & 
Bytecode::writeCharp (std::ostream & str, const char * charp)
{
    u_int32_t len = strlen (charp);
    writeInt32 (str, len);
    return str.write (charp, len);
}


char *
Bytecode::readCharp (std::istream & str)
{
    u_int32_t len = readInt32 (str);
    if (str.good())
    {
	char *buf = new char [len+1];
	if (str.read (buf, len))
	{
	    buf[len] = 0;
	    return buf;
	}
	delete [] buf;
    }
    return 0;
}


// ------------------------------------------------------------------
// bytecode I/O

std::ostream &
Bytecode::writeBytep (std::ostream & str, const unsigned char * bytep, unsigned int len)
{
    writeInt32 (str, len);
    return str.write ((char *)bytep, len);
}


unsigned char *
Bytecode::readBytep (std::istream & str)
{
    u_int32_t len = readInt32 (str);
    if (str.good())
    {
	unsigned char *buf = new unsigned char [len];
	if (str.read ((char *)buf, len))
	{
	    return buf;
	}
	delete [] buf;
    }
    return 0;
}


// ------------------------------------------------------------------
// Type I/O

std::ostream &
Bytecode::writeType (std::ostream & str, constTypePtr type)
{
    return type->toStream (str);
}


TypePtr
Bytecode::readType (std::istream & str)
{
    int kind = readInt32 (str);

    switch ((Type::tkind)kind)
    {
	case Type::UnspecT:
	case Type::ErrorT:
	case Type::AnyT:
	case Type::BooleanT:
	case Type::ByteblockT:
	case Type::FloatT:
	case Type::IntegerT:
	case Type::LocaleT:
	case Type::PathT:
	case Type::StringT:
	case Type::SymbolT:
	case Type::TermT:
	case Type::VoidT:
	case Type::WildcardT:
	case Type::FlexT:
	case Type::NilT:
	    return TypePtr ( new Type ((Type::tkind)kind, str) );
	break;

	case Type::VariableT:	return TypePtr ( new VariableType (str) ); break;
	case Type::BlockT:	return TypePtr ( new BlockType (str) ); break;
	case Type::ListT:	return TypePtr ( new ListType (str) ); break;
	case Type::MapT:	return TypePtr ( new MapType (str) ); break;
	case Type::TupleT:	return TypePtr ( new TupleType (str) ); break;
	case Type::FunctionT:	return TypePtr ( new FunctionType (str) ); break;
    }
    y2error ("Unhandled type kind %d", kind);
    return Type::Error->clone();
}

// ------------------------------------------------------------------
// YCPValue I/O


/*
 * write value to stream
 *
 */

std::ostream &
Bytecode::writeValue (std::ostream & str, const YCPValue value)
{
    if (value.isNull())
    {
	y2error ("writeValue (NULL)");
	str.setstate (std::ostream::failbit);
    }
    else if (str.put ((char)value->valuetype()))
    {
	return value->toStream (str);
    }
    return str;
}


/*
 * read value from stream
 *
 */

YCPValue
Bytecode::readValue (std::istream & str)
{
    char vt;
    if (str.get (vt))
    {
	switch (vt)
	{
	    case YT_VOID:
	    {
		return YCPVoid (str);
	    }
	    break;
	    case YT_BOOLEAN:
	    {
		return YCPBoolean (str);
	    }
	    break;
	    case YT_INTEGER:
	    {
		return YCPInteger (str);
	    }
	    break;
	    case YT_FLOAT:
	    {
		return YCPFloat (str);
	    }
	    break;
	    case YT_STRING:
	    {
		return YCPString (str);
	    }
	    break;
	    case YT_BYTEBLOCK:
	    {
		return YCPByteblock (str);
	    }
	    break;
	    case YT_PATH:
	    {
		return YCPPath (str);
	    }
	    break;
	    case YT_SYMBOL:
	    {
		return YCPSymbol (str);
	    }
	    break;
	    case YT_LIST:
	    {
		return YCPList (str);
	    }
	    break;
	    case YT_MAP:
	    {
		return YCPMap (str);
	    }
	    break;
	    case YT_TERM:
	    {
		return YCPTerm (str);
	    }
	    break;
	    case YT_CODE:
	    {
		return YCPCode (str);
	    }
	    break;
	    default:
	    {
		y2error ("readValue stream code %d", vt);
		break;
	    }
	}
    }
    else
    {
	y2warning ("readValue(%d:%s) NIL", (int)vt, YCode::toString((YCode::ykind)vt).c_str());
    }

    return YCPNull();
}


// ------------------------------------------------------------------
// ycodelist_t * I/O

std::ostream &
Bytecode::writeYCodelist (std::ostream & str, const ycodelist_t *codelist)
{
    u_int32_t count = 0;
    const ycodelist_t *codep = codelist;

    while (codep)
    {
	count++;
	codep = codep->next;
    }

    y2debug ("Bytecode::writeYCodelist %d entries", count);

    if (Bytecode::writeInt32 (str, count))
    {
	codep = codelist;
	while (codep)
	{
	    if (!codep->code->toStream (str))
	    {
		y2error ("Error writing codelist");
		break;
	    }
	    codep = codep->next;
	}
    }

    return str;
}


bool
Bytecode::readYCodelist (std::istream & str, ycodelist_t **anchor, ycodelist_t **last)
{
    u_int32_t count = readInt32 (str);

    y2debug ("Bytecode::readYCodelist %d entries", count);

    while (count-- > 0)
    {
	ycodelist_t *element = new ycodelist_t;

	element->code = Bytecode::readCode (str);
	element->next = 0;

	if (element->code == 0)
	{
	    y2error ("Bytecode::readYCodelist failed");
	    delete element;
	    return false;
	}

	if (*anchor == 0)		// anchor undefined
	{
	    *anchor = element;
	}
	else if (last != 0)		// last given
	{
	    (*last)->next = element;
	}

	if (last != 0)
	{
	    *last = element;
	}
    }

    return str.good();
}


// ------------------------------------------------------------------
// block stack handling

// find Id matching block
int
Bytecode::blockId (const Y2Namespace *block)
{
    for (int i = m_block_tare_level; i <= m_block_nesting_level; i++)
    {
	if (m_block_nesting_array[i] == block)
	{
	    return i - m_block_tare_level;
	}
    }
    y2debug ("No ID for %p, level %d", block, m_block_nesting_level);
    return -1;
}


// retrieve block for ID
const Y2Namespace *
Bytecode::blockPtr (int block_id)
{
    // for entries without a block (foreach)
    if (block_id < 0) return 0;

    block_id += m_block_tare_level;
    if (block_id <= m_block_nesting_level)	// local block
    {
	return m_block_nesting_array[block_id];
    }
    y2error ("Block id %d > nesting_level %d", block_id - m_block_tare_level, m_block_nesting_level - m_block_tare_level);
    return 0;
}


// push block to stack
//  the stack resembles the nesting of blocks
int
Bytecode::pushBlock (const Y2Namespace *block)
{
    if (block == 0)
    {
	y2error ("Bytecode::pushBlock (%p) NULL", block);
	return -1;
    }

    m_block_nesting_level++;
    if (m_block_nesting_array_size <= m_block_nesting_level)
    {
	m_block_nesting_array_size++;
	m_block_nesting_array = (const Y2Namespace **)realloc (m_block_nesting_array, sizeof (Y2Namespace *) * m_block_nesting_array_size);
    }
    y2debug ("Bytecode::pushBlock (%p), level %d, size %d, tare %d", block, m_block_nesting_level, m_block_nesting_array_size, m_block_tare_level);
    m_block_nesting_array[m_block_nesting_level] = block;
    return m_block_nesting_level-m_block_tare_level;
}


// pop block from stack
//  the stack resembles the nesting of blocks
int
Bytecode::popBlock (const Y2Namespace *block)
{
    y2debug ("Bytecode::popBlock (%p), level %d, size %d, tare %d", block, m_block_nesting_level, m_block_nesting_array_size, m_block_tare_level);
    if (block == 0)
    {
	y2error ("Bytecode::popBlock (%p) NULL", block);
	return -1;
    }

    if (m_block_nesting_level-m_block_tare_level < 0)
    {
	y2error ("Bytecode::popBlock (%p) empty stack", block);
    }
    else if (m_block_nesting_array[m_block_nesting_level] != block)
    {
	y2error ("Bytecode::popBlock (%p) not top of stack [%d]%p", block, m_block_nesting_level, m_block_nesting_array[m_block_nesting_level]);
    }
    else
    {
	m_block_nesting_level--;
    }
    return 0;
}


// reset current block stack to 'empty' for module loading
//   returns a tare id needed later
int
Bytecode::tareStack ()
{
    int tare = m_block_nesting_level - m_block_tare_level + 1;
    y2debug ("Bytecode::tareStack() level %d, size %d, current tare %d, tare_id %d", m_block_nesting_level, m_block_nesting_array_size, m_block_tare_level, tare);
    m_block_tare_level = m_block_nesting_level + 1;
    return tare;
}


void
Bytecode::untareStack (int tare_id)
{
    y2debug ("Bytecode::untareStack() level %d, size %d, current tare %d, tare_id %d", m_block_nesting_level, m_block_nesting_array_size, m_block_tare_level, tare_id);
    m_block_tare_level -= tare_id;
    return;
}

// ------------------------------------------------------------------
// SymbolEntry pointer (!) handling
//   the SymbolEntries itself are 'owned' by YBlock and referenced via pointers
//   to SymbolEntry. These functions handle stream I/O for SymbolEntry pointers.

std::ostream &
Bytecode::writeEntry (std::ostream & str, const SymbolEntry *entry)
{
    y2debug ("Bytecode::writeEntry (%s: blk %d, pos %d)", entry->toString().c_str(), Bytecode::blockId (entry->block()), entry->position());
    Bytecode::writeInt32 (str, Bytecode::blockId (entry->block()));
    return Bytecode::writeInt32 (str, entry->position());
}


SymbolEntry *
Bytecode::readEntry (std::istream & str)
{
    // read reference to blocks (block_id) symbol table (position)
    int block_id = Bytecode::readInt32 (str);
    int position = Bytecode::readInt32 (str);
    y2debug ("Bytecode::readEntry (blk %d, pos %d)", block_id, position);
    
    if (block_id == -1)
    {
	y2debug( "Special entry without block" );
	// FIXME: this may be wrong
	return 0;
    }

    // get block pointer and SymbolEntry within block
    Y2Namespace *block = const_cast<Y2Namespace*>(Bytecode::blockPtr (block_id));
    if (block == 0)
    {
	y2error ("invalid block %d for entry", block_id);
	return 0;
    }
    SymbolEntry *entry = block->symbolEntry (position);
    if (entry == 0)
    {
	y2error ("invalid entry %d for block (%s)", position, block->toString().c_str());
	return 0;
    }
    y2debug ("entry <blk %p[%s]> (%s)", block, block->name().c_str(), entry->toString().c_str());
    return entry;
}

// ------------------------------------------------------------------
// YCode read


// read code from stream
YCode *
Bytecode::readCode (std::istream & str)
{
    char code;
    if (!str.get (code))
    {
	y2error ("Can't read from stream");
	return 0;
    }
    y2debug ("Bytecode::readCode (%d:%s)", code, YCode::toString ((YCode::ykind)code).c_str());
    if (code < YCode::ycConstant)
    {
	return new YConst ((YCode::ykind)code, str);
    }

    switch (code)
    {
	case YCode::ycConstant:
	{
	    // this constant is a placeholder, typically used by 
	    // language bindings that cannot provide type information
	    y2error ("Unable to read constant, check the compilation of the module");
	    return 0;
	}
	case YCode::ycLocale:
	{
	    return new YLocale (str);
	}
	case YCode::ycFunction:
	{
	    return new YFunction (str);
	}
	case YCode::yePropagate:
	{
	    return new YEPropagate (str);
	}
	break;
	case YCode::yeUnary:
	{
	    return new YEUnary (str);
	}
	break;
	case YCode::yeBinary:
	{
	    return new YEBinary (str);
	}
	break;
	case YCode::yeTriple:
	{
	    return new YETriple (str);
	}
	break;
	case YCode::yeCompare:
	{
	    return new YECompare (str);
	}
	break;
	case YCode::yeLocale:
	{
	    return new YELocale (str);
	}
	break;
	case YCode::yeList:
	{
	    return new YEList (str);
	}
	break;
	case YCode::yeMap:
	{
	    return new YEMap (str);
	}
	break;
	case YCode::yeTerm:
	{
	    return new YETerm (str);
	}
	break;
	case YCode::yeLookup:
	{
	    return new YELookup (str);
	}
	break;
	case YCode::yeIs:
	{
	    return new YEIs (str);
	}
	break;
	case YCode::yeBracket:
	{
	    return new YEBracket (str);
	}
	break;
	case YCode::yeBlock:
	{
	    return new YBlock (str);
	}
	break;
	case YCode::yeReturn:
	{
	    return new YEReturn (str);
	}
	break;
	case YCode::yeVariable:
	{
	    return new YEVariable (str);
	}
	break;
	case YCode::yeReference:
	{
	    return new YEReference (str);
	}
	break;
	case YCode::yeBuiltin:
	{
	    return new YEBuiltin (str);
	}
	break;
	case YCode::yeFunction:
	{
	    return new YEFunction (str);
	}
	break;
	case YCode::ysTypedef:
	{
	    return new YSTypedef (str);
	}
	break;
	case YCode::ysVariable:
	{
	    return new YSAssign (true, str);
	}
	break;
	case YCode::ysFunction:
	{
	    return new YSFunction (str);
	}
	break;
	case YCode::ysAssign:
	{
	    return new YSAssign (false, str);
	}
	break;
	case YCode::ysBracket:
	{
	    return new YSBracket (str);
	}
	break;
	case YCode::ysIf:
	{
	    return new YSIf (str);
	}
	break;
	case YCode::ysWhile:
	{
	    return new YSWhile (str);
	}
	break;
	case YCode::ysDo:
	{
	    return new YSDo (str);
	}
	break;
	case YCode::ysRepeat:
	{
	    return new YSRepeat (str);
	}
	break;
	case YCode::ysExpression:
	{
	    return new YSExpression (str);
	}
	break;
	case YCode::ysReturn:
	{
	    return new YSReturn (str);
	}
	break;
	case YCode::ysBreak:
	{
	    return new YStatement ((YCode::ykind)code, str);
	}
	break;
	case YCode::ysContinue:
	{
	    return new YStatement ((YCode::ykind)code, str);
	}
	break;
	case YCode::ysTextdomain:
	{
	    return new YSTextdomain (str);
	}
	break;
	case YCode::ysInclude:
	{
	    return new YSInclude (str);
	}
	break;
	case YCode::ysFilename:
	{
	    return new YSFilename (str);
	}
	case YCode::ysImport:
	{
	    // check, if the import did not fail
	    YSImport* st = new YSImport (str);
	    if (st->block () != NULL) return st;
	}
	break;
	default:
	{
	    y2error ("Unknown code %d", code);
	}
	break;
    }
    return 0;
}


// ------------------------------------------------------------------
// File I/O


map <string, YBlock*> Bytecode::bytecodeCache;

// read file from module path

YBlock *
Bytecode::readModule (const string & mname, const string & timestamp)
{
    y2debug ("Bytecode::readModule (%s, timestamp %s) ", mname.c_str (), timestamp.c_str ());

    // TODO better error reporting?
    // like: could not find foo.ycp in /modules, /a/modules.
    // It will return an empty string on failure

    string filename = YCPPathSearch::findModule (mname);
    if (filename.empty())
    {
	y2error ("Module '%s' not found", mname.c_str());
	return 0;
    }
    
    // check the cache
    if (bytecodeCache.find (mname) != bytecodeCache.end ())
    {
	y2debug ("Bytecode cache hit: %s", mname.c_str ());

	// FIXME: check timestamp first

	return bytecodeCache.find (mname)->second;
    }

    int tare_id = Bytecode::tareStack ();			// current nesting level is 0 for this module
    YBlock *block = (YBlock *)Bytecode::readFile (filename);
    
    if (block == NULL)
    {
	return NULL;
    }
    
    Bytecode::untareStack (tare_id);
    
    if (!block->isModule())
    {
	y2error ("'%s' is no module", filename.c_str());
	delete block;
	return NULL;
    }

    // check the timestamp
    // FIXME: this could be done earlier    
    if (!timestamp.empty () && block->timestamp () != timestamp)
    {
	return NULL;
    }
    y2debug ("Timestamp match for file %s", filename.c_str ());

    bytecodeCache.insert (std::make_pair (mname, block));

    return block;
}


static int
readInt (std::istream & str)
{
    int i = 0;

    char c;

    for (;;)
    {
	str.get (c);
	if (!isdigit (c))
	    break;
	i *= 10;
	i += (c - '0');
    }

    return i;
}


// read YCode from file, return YCode (YError in case of failure)
YCode *
Bytecode::readFile (const string & filename)
{
    y2debug ("Bytecode::readFile (%s)", filename.c_str());
    std::ifstream instream (filename.c_str());
    if (!instream.is_open ())
    {
	y2error ("Failed to open '%s': %s", filename.c_str(), strerror (errno));
	return 0;
    }
    // check YaST_BYTECODE_HEADER

    int headerlen = strlen (YaST_BYTECODE_HEADER);
    char *header = new char [headerlen + 1];
    instream.read (header, headerlen);
    header[headerlen] = 0;
    if (strcmp (header, YaST_BYTECODE_HEADER) != 0)
    {
	y2error ("Not a bytecode file '%s'[%s]", filename.c_str(), header);
	return 0;
    }

    int major = readInt (instream);
    int minor = readInt (instream);
    int release = readInt (instream);

    if ( (major == atoi (YaST_BYTECODE_MAJOR))
	&& (minor == atoi (YaST_BYTECODE_MINOR))
	&& (release <= atoi (YaST_BYTECODE_RELEASE)))
    {
	y2debug ("Header accepted");
	
	// FIXME: we should check the timestamp and issue a warning on problems
	string timestamp;
	readString (instream, timestamp);
	return readCode (instream);
    }

    y2error ("Unsupported version %d.%d.%d", major, minor, release);
    return 0;
}


// write YCode to file, return errno (i.e. file not existing)
int
Bytecode::writeFile (const YCode *code, const string & filename)
{
    // clear errno first
    errno = 0;
    
    y2debug ("Bytecode::writeFile (%s)", filename.c_str());
    std::ofstream outstream (filename.c_str());
    if (!outstream.is_open ())
    {
	y2error ("Failed to write '%s': %s", filename.c_str(), strerror (errno));
	return errno;
    }
    
    string header =  string (YaST_BYTECODE_HEADER YaST_BYTECODE_MAJOR "." YaST_BYTECODE_MINOR "." YaST_BYTECODE_RELEASE);
    outstream.write (header.c_str(), header.size() + 1);	// including trailing \0
    
    
    if (code->isBlock ())
    {
	// for block use its timestamp
	writeString (outstream, ((YBlock *)code)-> timestamp ());
	y2debug ("Written timestamp %s", ((YBlock *)code)-> timestamp ().c_str());
    }
    else
    {
	// get the original file name based on the ybc name
	string ycp_filename = filename;
	ycp_filename.replace ( filename.size ()-2, 2, "cp" );
	string ts = fileTimestamp (ycp_filename);
	writeString (outstream, ts);
    }

    code->toStream (outstream);

    return outstream.fail ();
}


string
Bytecode::fileTimestamp (const string & filename)
{
    struct stat s;
    if (stat (filename.c_str(), &s) == 0 )
    {
        struct tm *tm_v = localtime ( & s.st_mtime );
        char buf[100];
        strftime (buf, 100, "%s", tm_v);
        return buf;
    }

    return "unknown timestamp";
}
