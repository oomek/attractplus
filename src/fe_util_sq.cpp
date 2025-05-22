/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2014-15 Andrew Mickelson
 *
 *  This file is part of Attract-Mode.
 *
 *  Attract-Mode is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Attract-Mode is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Attract-Mode.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "fe_util_sq.hpp"
#include "fe_util.hpp" // perform_substitution
#include <sqrat.h>

bool fe_get_object_string(
	HSQUIRRELVM vm,
	const HSQOBJECT &obj,
	std::string &out_string )
{
	sq_pushobject(vm, obj);
	sq_tostring(vm, -1);
	const SQChar *s;
	SQRESULT res = sq_getstring(vm, -1, &s);
	bool r = SQ_SUCCEEDED(res);
	if (r)
		out_string = s;

	sq_pop(vm,2);
	return r;
}

bool fe_get_attribute_string(
	HSQUIRRELVM vm,
	const HSQOBJECT &obj,
	const std::string &key,
	const std::string &attribute,
	std::string & out_string )
{
	sq_pushobject(vm, obj);

	if ( key.empty() )
		sq_pushnull(vm);
	else
		sq_pushstring(vm, key.c_str(), key.size());

	if ( SQ_FAILED( sq_getattributes(vm, -2) ) )
	{
		sq_pop(vm, 2);
		return false;
	}

	HSQOBJECT att_obj;
	sq_resetobject( &att_obj );
	sq_getstackobj( vm, -1, &att_obj );

	Sqrat::Object attTable( att_obj, vm );
	sq_pop( vm, 2 );

	if ( attTable.IsNull() )
		return false;

	Sqrat::Object attVal = attTable.GetSlot( attribute.c_str() );
	if ( attVal.IsNull() )
		return false;

	return fe_get_object_string( vm, attVal.GetObject(), out_string );
}

bool fe_get_attribute_bool(
	HSQUIRRELVM vm,
	const HSQOBJECT &obj,
	const std::string &key,
	const std::string &attribute,
	bool & out_bool )
{
	std::string attVal;
	if ( !fe_get_attribute_string( vm, obj, key, attribute, attVal ) ) return false;
	out_bool = config_str_to_bool( attVal );
	return true;
}

bool fe_get_attribute_int(
	HSQUIRRELVM vm,
	const HSQOBJECT &obj,
	const std::string &key,
	const std::string &attribute,
	int & out_int )
{
	std::string attVal;
	if ( !fe_get_attribute_string( vm, obj, key, attribute, attVal ) ) return false;
	out_int = as_int( attVal );
	return true;
}

int fe_obj_compare(
	HSQUIRRELVM vm,
	const HSQOBJECT &obj1,
	const HSQOBJECT &obj2 )
{
	sq_pushobject(vm, obj2);
	sq_pushobject(vm, obj1);
	int retval = sq_cmp( vm );
	sq_pop( vm, 2 );
	return retval;
}

int fe_get_num_params(
	HSQUIRRELVM vm,
	const HSQOBJECT &func,
	const HSQOBJECT &env )
{
	sq_pushobject( vm, func );
	sq_pushobject( vm, env );

	SQUnsignedInteger nparams( 0 );
	SQUnsignedInteger nfreevars( 0 );
	sq_getclosureinfo( vm, -2, &nparams, &nfreevars );

	sq_pop(vm, 2);

	return nparams-1;
}

void fe_register_global_func(
	HSQUIRRELVM vm,
	SQFUNCTION f,
	const char *name )
{
        sq_pushroottable( vm );
        sq_pushstring( vm, name, -1 );
        sq_newclosure( vm, f, 0 );
        sq_newslot( vm, -3, SQFalse );
        sq_pop( vm, 1 ); // pops the root table
}

// Return escaped squirrel special char
std::string escape_char( char c )
{
	switch (c) {
		case 9: return "\\t";
		case 7: return "\\a";
		case 8: return "\\b";
		case 10: return "\\n";
		case 13: return "\\r";
		case 11: return "\\v";
		case 12: return "\\f";
		case 0: return "\\0";
		case 92: return "\\\\";
		case 34: return "\\\"";
		case 39: return "\\'";
		default: return {c};
	}
}

// Escape all squirrel special chars in given value
void escape_string( std::string &value )
{
	std::string out = "";
	for (char c: value) out += escape_char( c );
	value = out;
}

std::string fe_to_json_string( HSQOBJECT obj, int indent )
{
	std::string retval;
	Sqrat::Object sobj( obj );

	switch ( sobj.GetType() )
	{
	case OT_NULL:
		retval = "null";
		break;

	case OT_BOOL:
	case OT_INTEGER:
	case OT_FLOAT:
		fe_get_object_string( Sqrat::DefaultVM::Get(),
			obj, retval );
		break;

	case OT_STRING:
		retval = "\"";
		{
			std::string obj_string;
			fe_get_object_string( Sqrat::DefaultVM::Get(),
				obj, obj_string );

			escape_string( obj_string );
			retval += obj_string;
		}
		retval += "\"";
		break;

	case OT_TABLE:
		retval += "{\n";
		{
			for ( int i=0; i<indent; i++ )
				retval += "\t";

			Sqrat::Object::iterator it;

			bool got_obj = sobj.Next( it );
			while ( got_obj )
			{
				std::string val = fe_to_json_string( it.getValue(),
						indent+1 );

				//
				// Ignore completely if val comes back empty
				//
				if ( val.empty() )
				{
					got_obj = sobj.Next( it );
					continue;
				}

				std::string key = fe_to_json_string( it.getKey(), 0 );

				retval += "\t";
				retval += key;
				retval += ": ";
				retval += val;

				got_obj = sobj.Next( it );
				if ( got_obj )
					retval += ",";

				retval += "\n";
				for ( int i=0; i<indent; i++ )
					retval += "\t";
			}
		}
		retval += "}";
		break;

	case OT_ARRAY:
		retval += "[";
		{
			Sqrat::Object::iterator it;
			bool first=true;
			while ( sobj.Next( it ) )
			{
				std::string val = fe_to_json_string( it.getValue(),
						indent );

				if ( !val.empty() )
				{
					if ( first )
						first=false;
					else
						retval += ",";

					retval += val;
				}
			}
		}
		retval += "]";
		break;

	case OT_USERDATA:
	case OT_CLOSURE:
	case OT_NATIVECLOSURE:
	case OT_GENERATOR:
	case OT_USERPOINTER:
	case OT_CLASS:
	case OT_INSTANCE:
	case OT_WEAKREF:
	default:
		// We return empty for these on purpose
		break;
	}

	return retval;
}

