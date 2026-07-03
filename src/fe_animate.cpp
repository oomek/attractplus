/*
 *
 *  Attract-Mode Plus frontend
 *  Copyright (C) 2026 Andrew Mickelson & Radek Dutkiewicz
 *
 *  This file is part of Attract-Mode Plus
 *
 *  Attract-Mode Plus is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Attract-Mode Plus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Attract-Mode Plus.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "fe_animate.hpp"
#include "fe_present.hpp"
#include "fe_presentable.hpp"
#include "sq_ease.hpp"

#include <sqrat.h>

#include <algorithm>
#include <cmath>
#include <utility>
#include <string>
#include <vector>

namespace
{
	typedef float (*FeEaseFunc)( float, float, float, float );

	struct FeAnimateProperty
	{
		std::string name;
		const std::type_info *owner;
		FeAnimate::PropertyMatcher matches;
		FeAnimate::PropertyGetter get;
		FeAnimate::PropertySetter set;
	};

	struct FeAnimation
	{
		FeBasePresentable *drawable;
		const FeAnimateProperty *property;
		FeEaseFunc ease;
		Sqrat::Object obj;
		const SQChar *slot;
		bool inertia;
		float start_val;
		float dest_val;
		float duration_ms;
		int start_ms;
		float current_val;
		float mass;
		float buffer[3];
	};

	const FeEaseFunc ease_functions[] =
	{
		&SqEase::linear,
		&SqEase::out_expo2, // Inertia
		&SqEase::in_quad,
		&SqEase::out_quad,
		&SqEase::in_out_quad,
		&SqEase::out_in_quad,
		&SqEase::in_cubic,
		&SqEase::out_cubic,
		&SqEase::in_out_cubic,
		&SqEase::out_in_cubic,
		&SqEase::in_quart,
		&SqEase::out_quart,
		&SqEase::in_out_quart,
		&SqEase::out_in_quart,
		&SqEase::in_quint,
		&SqEase::out_quint,
		&SqEase::in_out_quint,
		&SqEase::out_in_quint,
		&SqEase::in_sine,
		&SqEase::out_sine,
		&SqEase::in_out_sine,
		&SqEase::out_in_sine,
		&SqEase::in_expo,
		&SqEase::out_expo,
		&SqEase::in_out_expo,
		&SqEase::out_in_expo,
		&SqEase::in_expo2,
		&SqEase::out_expo2,
		&SqEase::in_out_expo2,
		&SqEase::out_in_expo2,
		&SqEase::in_circ,
		&SqEase::out_circ,
		&SqEase::in_out_circ,
		&SqEase::out_in_circ,
		static_cast<FeEaseFunc>( &SqEase::in_back ),
		static_cast<FeEaseFunc>( &SqEase::out_back ),
		static_cast<FeEaseFunc>( &SqEase::in_out_back ),
		static_cast<FeEaseFunc>( &SqEase::out_in_back ),
		&SqEase::in_back2,
		&SqEase::out_back2,
		&SqEase::in_out_back2,
		&SqEase::out_in_back2,
		&SqEase::in_bounce,
		&SqEase::out_bounce,
		&SqEase::in_out_bounce,
		&SqEase::out_in_bounce,
		static_cast<FeEaseFunc>( &SqEase::in_bounce2 ),
		static_cast<FeEaseFunc>( &SqEase::out_bounce2 ),
		static_cast<FeEaseFunc>( &SqEase::in_out_bounce2 ),
		static_cast<FeEaseFunc>( &SqEase::out_in_bounce2 ),
		static_cast<FeEaseFunc>( &SqEase::in_elastic ),
		static_cast<FeEaseFunc>( &SqEase::out_elastic ),
		static_cast<FeEaseFunc>( &SqEase::in_out_elastic ),
		static_cast<FeEaseFunc>( &SqEase::out_in_elastic ),
		static_cast<FeEaseFunc>( &SqEase::in_elastic2 ),
		static_cast<FeEaseFunc>( &SqEase::out_elastic2 ),
		static_cast<FeEaseFunc>( &SqEase::in_out_elastic2 ),
		static_cast<FeEaseFunc>( &SqEase::out_in_elastic2 )
	};

	static_assert(
		( sizeof( ease_functions ) / sizeof( ease_functions[0] )) == EaseCount,
		"Ease enum and easing function table are out of sync" );

	std::vector<FeAnimation> &animations()
	{
		static std::vector<FeAnimation> list;
		return list;
	}

	std::vector<FeAnimateProperty> &properties()
	{
		static std::vector<FeAnimateProperty> list;
		return list;
	}

	bool get_numeric_arg( HSQUIRRELVM vm, SQInteger index, float &value )
	{
		switch ( sq_gettype( vm, index ) )
		{
			case OT_INTEGER:
			{
				SQInteger i;
				sq_getinteger( vm, index, &i );
				value = static_cast<float>( i );
				return std::isfinite( value );
			}
			case OT_FLOAT:
			{
				SQFloat f;
				sq_getfloat( vm, index, &f );
				value = static_cast<float>( f );
				return std::isfinite( value );
			}
			default:
				return false;
		}
	}

	const FeAnimateProperty *find_property(
		const char *name,
		FeBasePresentable *drawable,
		bool &name_found )
	{
		name_found = false;
		const FeAnimateProperty *match = NULL;

		for ( const FeAnimateProperty &property : properties() )
		{
			if ( property.name != name )
				continue;

			name_found = true;
			// Derived class registrations come after base registrations.
			if ( property.matches( drawable ))
				match = &property;
		}

		return match;
	}

	FeEaseFunc get_ease_function( int ease )
	{
		if (( ease < 0 ) || ( ease >= EaseCount ))
			return NULL;

		return ease_functions[ease];
	}

	float apply_inertia( FeAnimation &animation, float current_val, float elapsed )
	{
		FePresent *fep = FePresent::script_get_fep();
		float t = elapsed / animation.duration_ms;

		if ( t < 1.0f )
		{
			t *= t; t *= t; t *= t;
			t = 1.0f - t;

			const float pi = 3.14159265358979323846f;
			float refresh_rate = static_cast<float>( fep->get_refresh_rate() );

			if ( fep->get_layout_frame_time() > 0.0f )
				refresh_rate = 1000.0f / fep->get_layout_frame_time();

			float coeff = std::sin(( 4.0f * pi ) / ( 8.0f + refresh_rate * animation.duration_ms * 0.000825f * animation.mass ));

			animation.buffer[0] += coeff * ( current_val - animation.buffer[0] );
			animation.buffer[1] += coeff * ( animation.buffer[0] - animation.buffer[1] );
			animation.buffer[2] += coeff * ( animation.buffer[1] - animation.buffer[2] );

			animation.buffer[0] = t * ( animation.buffer[0] - current_val ) + current_val;
			animation.buffer[1] = t * ( animation.buffer[1] - current_val ) + current_val;
			animation.buffer[2] = t * ( animation.buffer[2] - current_val ) + current_val;
		}
		else
		{
			animation.buffer[0] = current_val;
			animation.buffer[1] = current_val;
			animation.buffer[2] = current_val;
		}

		return animation.buffer[2];
	}

	void replace_animation(
		FeBasePresentable *drawable,
		const FeAnimateProperty *property,
		FeEaseFunc ease,
		Sqrat::Object &obj,
		const SQChar *slot,
		bool inertia,
		float dest_val,
		float duration_ms,
		int now_ms )
	{
		std::vector<FeAnimation> &list = animations();

		FeAnimation animation;
		animation.drawable = drawable;
		animation.property = property;
		animation.ease = ease;
		animation.obj = obj;
		animation.slot = slot;
		animation.inertia = inertia;
		animation.start_val = property->get( drawable );
		animation.dest_val = dest_val;
		animation.duration_ms = duration_ms;
		animation.start_ms = now_ms;
		animation.current_val = animation.start_val;
		animation.mass = 1.0f;
		animation.buffer[0] = animation.start_val;
		animation.buffer[1] = animation.start_val;
		animation.buffer[2] = animation.start_val;

		for ( std::vector<FeAnimation>::iterator it = list.begin(); it != list.end(); ++it )
		{
			if (( it->drawable != drawable ) || ( it->property->name != property->name ))
				continue;

			if ( inertia && it->inertia )
			{
				animation.start_val = it->current_val;
				animation.current_val = it->current_val;
				animation.mass = it->mass;
				animation.buffer[0] = it->buffer[0];
				animation.buffer[1] = it->buffer[1];
				animation.buffer[2] = it->buffer[2];
			}

			list.erase( it );
			break;
		}

		list.push_back( animation );
	}
}

void FeAnimate::register_property(
	const SQChar *name,
	const std::type_info &owner,
	PropertyMatcher matches,
	PropertyGetter get,
	PropertySetter set )
{
	std::vector<FeAnimateProperty> &list = properties();
	for ( FeAnimateProperty &property : list )
	{
		if (( property.name == name ) && ( *property.owner == owner ))
		{
			property.matches = std::move( matches );
			property.get = std::move( get );
			property.set = std::move( set );
			return;
		}
	}

	FeAnimateProperty property;
	property.name = name;
	property.owner = &owner;
	property.matches = std::move( matches );
	property.get = std::move( get );
	property.set = std::move( set );
	list.push_back( property );
}

SQInteger FeAnimate::script_animate( HSQUIRRELVM vm )
{
	SQInteger arg_count = sq_gettop( vm );
	if ( arg_count < 3 || arg_count > 6 )
		return sq_throwerror( vm, _SC("animate() requires property, destination") );

	FeBasePresentable *drawable = Sqrat::ClassType<FeBasePresentable>::GetInstance( vm, 1 );
	if ( !drawable )
	{
		if ( Sqrat::Error::Instance().Occurred( vm ) )
			return sq_throwerror( vm, Sqrat::Error::Instance().Message( vm ).c_str() );

		return sq_throwerror( vm, _SC("animate() must be called on a drawable") );
	}

	const SQChar *property_name = NULL;
	if (( sq_gettype( vm, 2 ) != OT_STRING )
			|| SQ_FAILED( sq_getstring( vm, 2, &property_name ))
			|| !property_name
			|| ( property_name[0] == '\0' ))
		return sq_throwerror( vm, _SC("animate() property must be a non-empty string") );

	float dest_val = 0.0f;
	if ( !get_numeric_arg( vm, 3, dest_val ))
		return sq_throwerror( vm, _SC("animate() destination must be numeric") );

	float duration_ms = 1000.0f; // Default to 1 second
	if ( arg_count >= 4 )
		if ( !get_numeric_arg( vm, 4, duration_ms ) )
			return sq_throwerror( vm, _SC("animate() invalid time value") );
	duration_ms = std::max( 0.f, duration_ms );

	FeEaseFunc ease = ease_functions[ 0 ]; // Default to Linear
	Sqrat::Object obj;
	const SQChar *slot = NULL;
	int ease_id = EaseLinear;

	if ( arg_count == 5 )
	{
		// Only function name provided
		obj = Sqrat::RootTable( vm );

		if ( sq_gettype( vm, 5 ) == OT_INTEGER )
		{
			SQInteger ease_value = 0;
			sq_getinteger( vm, 5, &ease_value );
			ease_id = static_cast<int>( ease_value );
			ease = get_ease_function( ease_id );
			if ( !ease )
				return sq_throwerror( vm, _SC("animate() easing is not a supported Ease value") );
		}
		else if ( sq_gettype( vm, 5 ) == OT_STRING )
		{
			sq_getstring( vm, 5, &slot );
		}
	}

	if ( arg_count == 6 )
	{
		// Both env and function name provided
		HSQOBJECT po;
		sq_getstackobj( vm, 5, &po );
		sq_getstring( vm, 6, &slot );
		obj = po;
	}

	if ( slot )
	{
		Sqrat::Function cb = Sqrat::Function( obj, slot );

		if ( cb.IsNull() )
			return sq_throwerror( vm, _SC("animate() invalid callback found") );
	}

	bool property_name_found = false;
	const FeAnimateProperty *property = find_property( property_name, drawable, property_name_found );
	if ( !property )
	{
		std::string message;
		if ( property_name_found )
		{
			message = "animate() property '";
			message += property_name;
			message += "' is not supported for this drawable";
		}
		else
		{
			message = "animate() unknown property: ";
			message += property_name;
		}
		return sq_throwerror( vm, message.c_str() );
	}

	FePresent *fep = FePresent::script_get_fep();
	int now_ms = fep ? fep->get_layout_ms() : 0;
	replace_animation( drawable, property, ease, obj, slot, ease_id == EaseInertia, dest_val, duration_ms, now_ms );
	FePresent::script_flag_redraw();

	return 0;
}

bool FeAnimate::tick( int now_ms )
{
	std::vector<FeAnimation> &list = animations();
	if ( list.empty() )
		return false;

	bool redraw = false;

	for ( std::vector<FeAnimation>::size_type i=0; i < list.size(); )
	{
		FeAnimation &animation = list[i];
		float elapsed = static_cast<float>( now_ms - animation.start_ms );
		if ( elapsed < 0.0f )
			elapsed = 0.0f;

		if ( elapsed >= animation.duration_ms )
		{
			animation.property->set( animation.drawable, animation.dest_val );
			list.erase( list.begin() + i );
			redraw = true;
			continue;
		}

		float t = elapsed;
		float b = animation.start_val;
		float c = animation.dest_val - animation.start_val;
		float d = animation.duration_ms;

		// TODO: cache the function like FeCallback does, or store in place of ease
		float current_val = animation.slot
			? Sqrat::Function( animation.obj, animation.slot ).Evaluate<float>(t, b, c, d)
			: animation.ease(t, b, c, d);
		animation.current_val = current_val;

		if ( animation.inertia )
			current_val = apply_inertia( animation, current_val, elapsed );

		animation.property->set( animation.drawable, current_val );
		redraw = true;
		++i;
	}

	if ( redraw )
		FePresent::script_flag_redraw();

	return redraw;
}

void FeAnimate::remove( FeBasePresentable *drawable )
{
	std::vector<FeAnimation> &list = animations();
	list.erase(
		std::remove_if(
			list.begin(),
			list.end(),
			[drawable]( const FeAnimation &animation )
			{
				return animation.drawable == drawable;
			}),
		list.end() );
}

void FeAnimate::clear()
{
	animations().clear();
}
