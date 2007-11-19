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

  File:		YWidget.cc

  Author:	Stefan Hundhammer <sh@suse.de>

/-*/


#include <signal.h>

#include <ycp/YCPBoolean.h>
#include <ycp/YCPInteger.h>
#include <ycp/YCPSymbol.h>
#include <ycp/YCPString.h>
#include <ycp/YCPTerm.h>
#include <ycp/YCPVoid.h>
#define y2log_component "ui"
#include <ycp/y2log.h>

#include "YUISymbols.h"
#include "YShortcut.h"
#include "YWidget.h"
#include "YDialog.h"
#include "YUIComponent.h"
#include "YUI.h"
#include "YDialog.h"
#include "YUIException.h"
#include "YWidgetID.h"
#include "YBothDim.h"

#include "YChildrenManager.h"

#define MAX_DEBUG_LABEL_LEN	50
#define YWIDGET_MAGIC		42

#define CHECK_FOR_DUPLICATE_CHILDREN	1


using std::string;

struct YWidgetPrivate
{
    /**
     * Constructor
     **/
    YWidgetPrivate( YWidgetChildrenManager * manager, YWidget * parentWidget = 0 )
	: magic( YWIDGET_MAGIC )
	, childrenManager( manager )
	, parent( parentWidget )
	, beingDestroyed( false )
	, enabled( true )
	, notify( false )
	, sendKeyEvents( false )
	, autoShortcut( false )
	, oldStyleConstructor( false )
	, toolkitWidgetRep( 0 )
	, id( 0 )
	, functionKey( 0 )
    {
	stretch.hor	= false;
	stretch.vert	= false;
	weight.hor	= 0;
	weight.vert	= 0;
    }

    //
    // Data members
    //

    int 			magic;
    YWidgetChildrenManager *	childrenManager;
    YWidget *			parent;
    bool			beingDestroyed;
    bool			enabled;
    bool			notify;
    bool 			sendKeyEvents;
    bool 			autoShortcut;
    bool			oldStyleConstructor;
    void *			toolkitWidgetRep;
    YWidgetID *			id;
    YBothDim<bool>		stretch;
    YBothDim<int>		weight;
    int				functionKey;
};




bool YWidget::_usedOperatorNew = false;

// FIXME: Obsolete
// FIXME: Obsolete
// FIXME: Obsolete
YWidget::YWidget( const YWidgetOpt & opt )
    : priv( new YWidgetPrivate( new YWidgetChildrenRejector( this ) ) )
{
    YUI_CHECK_NEW( priv );
    priv->oldStyleConstructor = true;
}
// FIXME: Obsolete
// FIXME: Obsolete
// FIXME: Obsolete



YWidget::YWidget( YWidget * parent )
    : priv( new YWidgetPrivate( new YWidgetChildrenRejector( this ), parent ) )
{
    YUI_CHECK_NEW( priv );
    YUI_CHECK_NEW( priv->childrenManager );

    if ( ! _usedOperatorNew )
    {
	y2error( "FATAL: Widget at %p not created with operator new !", this );
	y2error( "Check core dump for a backtrace." );
	abort();
    }

    _usedOperatorNew = false;

    if ( parent )
	parent->addChild( this );
}


void * YWidget::operator new( size_t size )
{
    _usedOperatorNew = true;
    return ::operator new( size );
}


YWidget::~YWidget()
{
    YUI_CHECK_WIDGET( this );
    setBeingDestroyed();
    // y2debug( "Destructor of YWidget %p", this );

    deleteChildren();

    if ( parent() && ! parent()->beingDestroyed() )
	parent()->removeChild( this );

    delete priv->childrenManager;

    if ( priv->id )
	delete priv->id;

    invalidate();
}


YWidgetChildrenManager *
YWidget::childrenManager() const
{
    return priv->childrenManager;
}


void
YWidget::setChildrenManager( YWidgetChildrenManager * newChildrenManager )
{
    YUI_CHECK_PTR( newChildrenManager );

    delete priv->childrenManager;
    priv->childrenManager = newChildrenManager;
}


void
YWidget::addChild( YWidget * child )
{
#if CHECK_FOR_DUPLICATE_CHILDREN
    if ( child && childrenManager()->contains( child ) )
    {
	y2error( "%s at %p already contains %s at %p!",
		 widgetClass(), this, child->widgetClass(), child );
	YUI_THROW( YUIInvalidChildException<YWidget>( this, child ) );
    }
#endif

    childrenManager()->add( child );
}


void
YWidget::removeChild( YWidget * child )
{
    if ( ! beingDestroyed() )
    {
	// y2debug( "Removing widget at %p from %s", child, widgetClass() );
	childrenManager()->remove( child );
    }
}


void
YWidget::deleteChildren()
{
    YWidgetList::const_iterator it = childrenBegin();

    while ( it != childrenEnd() )
    {
	YWidget * child = *it;
	++it;

	if ( child->isValid() )
	{
	    // y2debug( "Deleting %s at %p", child->widgetClass(), child );
	    delete child;
	}
    }

    childrenManager()->clear();
}


string
YWidget::debugLabel()
{
    string label = YShortcut::cleanShortcutString( YShortcut::getShortcutString( this ) );

    if ( label.size() > MAX_DEBUG_LABEL_LEN )
    {
	label.resize( MAX_DEBUG_LABEL_LEN );
	label.append( "..." );
    }

    return label;
}


bool
YWidget::isValid() const
{
    return priv->magic == YWIDGET_MAGIC;
}


void
YWidget::invalidate()
{
    priv->magic = 0;
}


bool
YWidget::beingDestroyed() const
{
    return priv->beingDestroyed;
}

void
YWidget::setBeingDestroyed()
{
    priv->beingDestroyed = true;
}


YWidget *
YWidget::parent() const
{
    if ( oldStyleConstructor() && ! priv->parent )
	YUI_THROW( YUIException( string( "No parent set yet for " ) + widgetClass() ) );

    return priv->parent;
}


bool
YWidget::hasParent() const
{
    return priv->parent;
}


void
YWidget::setParent( YWidget * newParent )
{
    if ( newParent && priv->parent )
    {
	YDialog::currentDialog()->dumpWidgetTree();
	y2warning( "%s::setParent( %p ) this: %p   old parent: %p",
		   widgetClass(), newParent, this, priv->parent );
	YUI_THROW( YUIException( string( widgetClass() ) + " already has a parent!" ) );
    }

    priv->parent = newParent;
}


bool
YWidget::oldStyleConstructor() const
{
    return priv->oldStyleConstructor;
}


bool YWidget::sendKeyEvents() const
{
     return priv->sendKeyEvents;
}


void YWidget::setSendKeyEvents( bool doSend )
{
     priv->sendKeyEvents = doSend;
}


bool YWidget::autoShortcut() const
{
     return priv->autoShortcut;
}


void YWidget::setAutoShortcut( bool newAutoShortcut )
{
     priv->autoShortcut = newAutoShortcut;
}


int YWidget::functionKey() const
{
    return priv->functionKey;
}


bool YWidget::hasFunctionKey() const
{
    return priv->functionKey > 0;
}


void YWidget::setFunctionKey( int fkey_no )
{
    priv->functionKey = fkey_no;
}


YWidgetID *
YWidget::id() const
{
    return priv->id;
}


void
YWidget::setId( YWidgetID * newId )
{
    if ( priv->id )
	delete priv->id;

    priv->id = newId;
}


bool
YWidget::hasId() const
{
    return priv->id != 0;
}


YDialog * YWidget::findDialog()
{
    YWidget * widget = this;

    while ( widget )
    {
	YDialog * dialog = dynamic_cast<YDialog *> (widget);

	if ( dialog )
	    return dialog;
	else
	    widget = widget->parent();
    }

    return 0;
}


const YPropertySet &
YWidget::propertySet()
{
    static YPropertySet propSet;

    if ( propSet.isEmpty() )
    {
	/**
	 * @property boolean Enabled 		enabled/disabled state of this widget
	 * @property boolean Notify 		the current notify state (see also `opt( `notify ))
	 * @property string  WidgetClass 	the widget class of this widget (YLabel, YPushButton, ...)
	 * @property string  DebugLabel		(possibly translated) text describing this widget for debugging
	 * @property string  DialogDebugLabel 	(possibly translated) text describing this dialog for debugging
	 **/

	propSet.add( YProperty( YUIProperty_Enabled,		YBoolProperty	) );
	propSet.add( YProperty( YUIProperty_Notify,		YBoolProperty	) );
	propSet.add( YProperty( YUIProperty_WidgetClass,	YStringProperty, true	) ); // read-only
	propSet.add( YProperty( YUIProperty_DebugLabel,		YStringProperty, true	) ); // read-only
	propSet.add( YProperty( YUIProperty_DialogDebugLabel,	YStringProperty, true	) ); // read-only
    }

    return propSet;
}


bool
YWidget::setProperty( const std::string & propertyName, const YPropertyValue & val )
{
    try
    {
	propertySet().check( propertyName, val.type() ); // throws exceptions if not found or type mismatch
    }
    catch( YUIPropertyException & exception )
    {
	exception.setWidget( this );
	throw;
    }

    if      ( propertyName == YUIProperty_Enabled )	setEnabled( val.boolVal() );
    else if ( propertyName == YUIProperty_Notify  )	setNotify ( val.boolVal() );

    return true; // success -- no special processing necessary
}


YPropertyValue
YWidget::getProperty( const std::string & propertyName )
{
    try
    {
	propertySet().check( propertyName ); // throws exceptions if not found
    }
    catch( YUIPropertyException & exception )
    {
	exception.setWidget( this );
	throw;
    }

    if ( propertyName == YUIProperty_Enabled 		) return YPropertyValue( isEnabled() 	);
    if ( propertyName == YUIProperty_Notify  		) return YPropertyValue( notify()   	);
    if ( propertyName == YUIProperty_WidgetClass	) return YPropertyValue( widgetClass() 	);
    if ( propertyName == YUIProperty_DebugLabel		) return YPropertyValue( debugLabel()	);
    if ( propertyName == YUIProperty_DialogDebugLabel 	)
    {
	YDialog * dialog = findDialog();

	if ( dialog )
	    return YPropertyValue( dialog->debugLabel() );
	else
	    return YPropertyValue( debugLabel() );
    }

    return YPropertyValue( false ); // NOTREACHED
}


void *
YWidget::widgetRep() const
{
    return priv->toolkitWidgetRep;
}


void
YWidget::setWidgetRep( void * rep )
{
    priv->toolkitWidgetRep = rep;
}


void
YWidget::setEnabled( bool enabled )
{
    priv->enabled = enabled;
}


bool
YWidget::isEnabled() const
{
    return priv->enabled;
}


void YWidget::setShortcutString( const std::string & str )
{
    y2error( "Default setShortcutString() method called - "
	     "should be reimplemented in %s", widgetClass() );
}


void YWidget::setNotify( bool notify )
{
    priv->notify = notify;
}


bool YWidget::notify() const
{
    return priv->notify;
}


int YWidget::preferredSize( YUIDimension dim )
{
    switch ( dim )
    {
	case YD_HORIZ:	return preferredWidth();
	case YD_VERT :	return preferredHeight();

	default:
	    YUI_THROW( YUIInvalidDimensionException() );
	    return 0;
    }
}


void YWidget::setStretchable( YUIDimension dim, bool newStretch )
{
    priv->stretch[ dim ] = newStretch;
}


void YWidget::setDefaultStretchable( YUIDimension dim, bool newStretch )
{
    priv->stretch[ dim ] |= newStretch;
}


bool YWidget::stretchable( YUIDimension dim ) const
{
    return priv->stretch[ dim ];
}


int YWidget::weight( YUIDimension dim )
{
    return priv->weight[ dim ];
}


void YWidget::setWeight( YUIDimension dim, int weight )
{
    priv->weight[ dim ] = weight;
}


bool YWidget::hasWeight( YUIDimension dim )
{
    // DO NOT simply return priv->weight[ dim ] here
    // since weight() might be overwritten in derived classes!

    return weight( dim ) > 0;
}


bool YWidget::setKeyboardFocus()
{
    y2warning( "Widget %s cannot accept the keyboard focus.", id()->toString().c_str() );
    return false;
}


YWidget *
YWidget::findWidget( YWidgetID * id, bool doThrow ) const
{
    if ( ! id )
    {
	if ( doThrow )
	    YUI_THROW( YUIWidgetNotFoundException( "Null ID" ) );

	return 0;
    }

    for ( YWidgetListConstIterator it = childrenBegin();
	  it != childrenEnd();
	  ++it )
    {
	YWidget * child = *it;
	YUI_CHECK_WIDGET( child );

	if ( child->id() && child->id()->isEqual( id ) )
	    return child;

	if ( child->hasChildren() )
	{
	    YWidget * found = child->findWidget( id, false );

	    if ( found )
		return found;
	}
    }

    if ( doThrow )
	YUI_THROW( YUIWidgetNotFoundException( id->toString() ) );

    return 0;
}


void YWidget::setChildrenEnabled( bool enabled )
{
    for ( YWidgetListConstIterator it = childrenBegin();
	  it != childrenEnd();
	  ++it )
    {
	YWidget * child = *it;

	if ( child->hasChildren() )
	{
	    // y2debug( "Recursing into %s", child->debugLabel().c_str() );
	    child->setChildrenEnabled( enabled );
	}

	// y2debug( "%s %s", enabled ? "Enabling" : "Disabling", child->debugLabel().c_str() );
	child->setEnabled( enabled );
    }
}


void YWidget::dumpDialogWidgetTree()
{
    YWidget * dialog = findDialog();

    if ( dialog )
	dialog->dumpWidgetTree();
    else
	dumpWidgetTree();
}


void YWidget::dumpWidgetTree( int indentationLevel )
{
    dumpWidget( this, indentationLevel );

    for ( YWidgetListConstIterator it = childrenBegin();
	  it != childrenEnd();
	  ++it )
    {
	YWidget * child = *it;

	if ( child->hasChildren() )
	    child->dumpWidgetTree ( indentationLevel + 1 );
	else
	    dumpWidget( child, indentationLevel + 1 );
    }
}


void YWidget::dumpWidget( YWidget *w, int indentationLevel )
{
    string indentation ( indentationLevel * 4, ' ' );

    string descr( w->debugLabel() );

    if ( ! descr.empty() )
	descr = "\"" + descr + "\"";

    if ( w->hasId() )
    {
	if ( ! descr.empty() )
	    descr += " ";

	descr += "`id( " + w->id()->toString() + " )";
    }

    string stretch;

    if ( w->stretchable( YD_HORIZ ) )	stretch += "hstretch ";
    if ( w->stretchable( YD_VERT  ) )	stretch += "vstretch";

    if ( ! stretch.empty() )
	stretch = "(" + stretch + ") ";

    if ( descr.empty() )
    {
	y2milestone( "Widget tree: %s%s %sat %p",
		     indentation.c_str(), w->widgetClass(), stretch.c_str(), w );
    }
    else
    {
	y2milestone( "Widget tree: %s%s %s %sat %p",
		     indentation.c_str(), w->widgetClass(), descr.c_str(), stretch.c_str(), w );
    }
}


void
YWidget::saveUserInput( YMacroRecorder *macroRecorder )
{
    for ( YWidgetListConstIterator it = childrenBegin();
	  it != childrenEnd();
	  ++it )
    {
	YWidget *widget = *it;

	if ( widget->hasChildren() || widget->hasId() )
	{
	    /*
	     * It wouldn't do any good to save the user input of any widget
	     * that doesn't have an ID since this ID is required to make use of
	     * this saved data later when playing the macro.
	     * Other than that, container widgets need to recurse over all
	     * their children.
	     */

	    widget->saveUserInput( macroRecorder );
	}
    }
}


// FIXME: Obsolete
// FIXME: Obsolete
// FIXME: Obsolete


YCPValue YWidget::changeWidget( const YCPSymbol & property, const YCPValue & newvalue )
{
    string symbol = property->symbol();

    if ( ! isValid() )
    {
	y2error( "YWidget::changeWidget( %s ): ERROR: Invalid widget", symbol.c_str() );

	return YCPBoolean( false );	// Error
    }

    if ( symbol == YUIProperty_Enabled )
    {
	if ( newvalue->isBoolean() )
	{
	    bool e = newvalue->asBoolean()->value();
	    setEnabled(e);
	    priv->enabled = e;
	    return YCPBoolean( true );
	}
	else y2error( "Wrong argument %s for widget property `Enabled - boolean expected.",
		      newvalue->toString().c_str() );
    }

    if ( symbol == YUIProperty_Notify )
    {
	if ( newvalue->isBoolean() )
	{
	    setNotify( newvalue->asBoolean()->value() );
	    return YCPBoolean( true );
	}
	else y2error( "Wrong argument %s for widget property `Notify - boolean expected.",
		      newvalue->toString().c_str() );
    }

    return YCPBoolean( false );
}


YCPValue YWidget::queryWidget( const YCPSymbol & property )
{
    string symbol = property->symbol();

    if ( symbol == YUIProperty_Enabled 		) 	return YCPBoolean( isEnabled() );
    if ( symbol == YUIProperty_Notify 		)	return YCPBoolean( notify() );
    if ( symbol == YUIProperty_WidgetClass 	) 	return YCPString( widgetClass() );
    if ( symbol == YUIProperty_DebugLabel	) 	return YCPString( debugLabel() );
    if ( symbol == YUIProperty_DialogDebugLabel )
    {
	return YDialog::currentDialog()->queryWidget( property );
    }
    else
    {
	y2error( "Couldn't query unkown widget property %s::%s",
		 widgetClass(), symbol.c_str() );
	return YCPVoid();
    }
}


// FIXME: Obsolete
// FIXME: Obsolete
// FIXME: Obsolete
