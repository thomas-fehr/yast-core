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

  File:		YMultiProgressMeter.cc

  Author:	Stefan Hundhammer <sh@suse.de>

/-*/


#define y2log_component "ui"
#include <ycp/y2log.h>

#include "YUISymbols.h"
#include "YMultiProgressMeter.h"


struct YMultiProgressMeterPrivate
{
    YMultiProgressMeterPrivate( YUIDimension 		dim,
				const vector<float> &	maxValues )
	: dim( dim )
	, maxValues( maxValues )
    {
	// Make currentValues as large as maxValues
	// and initialize each element with 0 
	currentValues = vector<float>( maxValues.size(), 0.0 );    
    }
    
    
    YUIDimension	dim;
    vector<float>	maxValues;
    vector<float>	currentValues;
};




YMultiProgressMeter::YMultiProgressMeter( YWidget *		parent,
					  YUIDimension		dim,
					  const vector<float> &	maxValues )
    : YWidget( parent )
    , priv( new YMultiProgressMeterPrivate( dim, maxValues ) )
{
    YUI_CHECK_NEW( priv );

    setDefaultStretchable( YD_HORIZ, dim == YD_HORIZ );
    setDefaultStretchable( YD_VERT,  dim == YD_VERT  );
}


YMultiProgressMeter::~YMultiProgressMeter()
{
    // NOP
}


YUIDimension
YMultiProgressMeter::dimension() const
{
    return priv->dim;
}


bool YMultiProgressMeter::horizontal() const
{
    return priv->dim == YD_HORIZ;
}


bool YMultiProgressMeter::vertical() const
{
    return priv->dim == YD_VERT;
}


int YMultiProgressMeter::segments() const
{
    return (int) priv->maxValues.size();
}


float YMultiProgressMeter::maxValue( int segment ) const
{
    YUI_CHECK_INDEX( segment, 0, (int) priv->maxValues.size() );

    return priv->maxValues[ segment ];
}


float YMultiProgressMeter::currentValue( int segment ) const
{
    YUI_CHECK_INDEX( segment, 0, (int) priv->currentValues.size() );

    return priv->currentValues[ segment ];
}


void YMultiProgressMeter::setCurrentValue( int segment, float value )
{
    YUI_CHECK_INDEX( segment, 0, (int) priv->currentValues.size() );
	
    if ( value < 0.0 )			// Allow -1 etc. as marker values
	value = 0.0;
	
    if ( value > maxValue( segment ) ) // Don't complain (beware of rounding errors)
	value = maxValue( segment );

    priv->currentValues[ segment ] = value;
}


void YMultiProgressMeter::setCurrentValues( const vector<float> & values )
{
    for ( int i=0; i < (int) values.size(); i++ )
    {
	setCurrentValue( i, values[i] );
    }

    doUpdate();
}


const YPropertySet &
YMultiProgressMeter::propertySet()
{
    static YPropertySet propSet;

    if ( propSet.isEmpty() )
    {
	/*
	 * @property list<integer> Values the current values for all segments
	 */
	propSet.add( YProperty( YUIProperty_Values,	YOtherProperty	) );
	propSet.add( YWidget::propertySet() );
    }

    return propSet;
}


bool
YMultiProgressMeter::setProperty( const string & propertyName, const YPropertyValue & val )
{
    propertySet().check( propertyName, val.type() ); // throws exceptions if not found or type mismatch

    if ( propertyName == YUIProperty_Values )	return false; // need special processing
    else
    {
	YWidget::setProperty( propertyName, val );
    }

    return true; // success -- no special handling necessary
}


YPropertyValue
YMultiProgressMeter::getProperty( const string & propertyName )
{
    propertySet().check( propertyName ); // throws exceptions if not found

    if ( propertyName == YUIProperty_Values ) 	return YPropertyValue( YOtherProperty );
    else
    {
	return YWidget::getProperty( propertyName );
    }
}




#if 0
    //
    // SetProp
    //

    /**
     * @property string Values the current values for all segments
     */
    if ( s == YUIProperty_Values )
    {
	if ( newValue->isList() )
	{
	    YCPList valList = newValue->asList();

	    if ( valList->size() == segments() )
	    {
		for ( int i=0; i < segments(); i++ )
		{
		    setCurrentValue( i, valList->value( i )->asInteger()->value() );
		}

		y2debug( "Setting values: %s", valList->toString().c_str() );
		doUpdate();	// notify derived classes
		return YCPBoolean( true );
	    }
	    else
	    {
		y2error( "Expected %d values for MultiProgressMeter, not %d (%s)",
			 segments(), valList->size(), valList->toString().c_str() );
	    }
	    
	    return YCPBoolean( false );
	}
	else
	{
	    y2error( "Expected list<integer> for MultiProgressMeter values, not %s",
		     newValue->toString().c_str() );
	    return YCPBoolean( false );
	}
    }

    //
    // GetProp
    //

    string s = property->symbol();
    if ( s == YUIProperty_Values )
    {
	YCPList values;

	for ( int i=0; i < segments(); i++ )
	{
	    val = ( i < (int) _currentValues.size() ) ? _currentValues[ i ] : -1;
	    values->add( YCPInteger( val ) );
	}

	return values;
    }

#endif