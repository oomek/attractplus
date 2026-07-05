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
#include "fe_image.hpp"
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

	struct FeAnimationState
	{
		int id;
		FeBasePresentable *drawable;
		const FeAnimateProperty *property;
		FeEaseFunc ease;
		int ease_id;
		Sqrat::Object obj;
		const SQChar *slot;
		bool inertia;
		float anim_start_val;
		float anim_dest_val;
		float prop_dest_val;
		float prop_last_val;
		float duration_ms;
		int start_ms;
		float current_val;
		float mass;
		float period;
		float amplitude;
		float strength;
		bool period_set;
		bool amplitude_set;
		bool strength_set;
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

	std::vector<FeAnimationState> &animations()
	{
		static std::vector<FeAnimationState> list;
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

	int new_animation_id()
	{
		static int next_id = 1;
		return next_id++;
	}

	FeAnimationState *find_animation( int id )
	{
		for ( FeAnimationState &animation : animations() )
			if ( animation.id == id )
				return &animation;

		return NULL;
	}

	float apply_ease( const FeAnimationState &animation, float t, float b, float c, float d )
	{
		switch ( animation.ease_id )
		{
			case EaseInBack:
				if ( animation.strength_set )
					return SqEase::in_back( t, b, c, d, animation.strength );
				break;
			case EaseOutBack:
				if ( animation.strength_set )
					return SqEase::out_back( t, b, c, d, animation.strength );
				break;
			case EaseInOutBack:
				if ( animation.strength_set )
					return SqEase::in_out_back( t, b, c, d, animation.strength );
				break;
			case EaseOutInBack:
				if ( animation.strength_set )
					return SqEase::out_in_back( t, b, c, d, animation.strength );
				break;

			case EaseInBounce2:
				if ( animation.period_set )
					return SqEase::in_bounce2( t, b, c, d, animation.period );
				break;
			case EaseOutBounce2:
				if ( animation.period_set )
					return SqEase::out_bounce2( t, b, c, d, animation.period );
				break;
			case EaseInOutBounce2:
				if ( animation.period_set )
					return SqEase::in_out_bounce2( t, b, c, d, animation.period );
				break;
			case EaseOutInBounce2:
				if ( animation.period_set )
					return SqEase::out_in_bounce2( t, b, c, d, animation.period );
				break;

			case EaseInElastic:
				if ( animation.amplitude_set && animation.period_set )
					return SqEase::in_elastic( t, b, c, d, animation.amplitude, animation.period );
				break;
			case EaseOutElastic:
				if ( animation.amplitude_set && animation.period_set )
					return SqEase::out_elastic( t, b, c, d, animation.amplitude, animation.period );
				break;
			case EaseInOutElastic:
				if ( animation.amplitude_set && animation.period_set )
					return SqEase::in_out_elastic( t, b, c, d, animation.amplitude, animation.period );
				break;
			case EaseOutInElastic:
				if ( animation.amplitude_set && animation.period_set )
					return SqEase::out_in_elastic( t, b, c, d, animation.amplitude, animation.period );
				break;

			case EaseInElastic2:
				if ( animation.period_set )
					return SqEase::in_elastic2( t, b, c, d, animation.period );
				break;
			case EaseOutElastic2:
				if ( animation.period_set )
					return SqEase::out_elastic2( t, b, c, d, animation.period );
				break;
			case EaseInOutElastic2:
				if ( animation.period_set )
					return SqEase::in_out_elastic2( t, b, c, d, animation.period );
				break;
			case EaseOutInElastic2:
				if ( animation.period_set )
					return SqEase::out_in_elastic2( t, b, c, d, animation.period );
				break;

			default:
				break;
		}

		return animation.ease( t, b, c, d );
	}

	float apply_inertia( FeAnimationState &animation, float current_val, float elapsed )
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

	FeAnimation replace_animation(
		FeBasePresentable *drawable,
		const FeAnimateProperty *property,
		FeEaseFunc ease,
		int ease_id,
		Sqrat::Object &obj,
		const SQChar *slot,
		bool inertia,
		float prop_dest_val,
		float duration_ms,
		int now_ms )
	{
		std::vector<FeAnimationState> &list = animations();
		float prop_start_val = property->get( drawable );
		float anim_start_val = drawable->snap_grid_destination_to_pixels( property->name, prop_start_val );
		float anim_dest_val = drawable->snap_grid_destination_to_pixels( property->name, prop_dest_val );

		if (( property->name == "width" ) || ( property->name == "height" ))
		{
			if ( FeImage *image = dynamic_cast<FeImage *>( drawable ))
			{
				if ( property->name == "width" )
					image->set_auto_width( false );
				else
					image->set_auto_height( false );
			}
		}

		FeAnimationState animation;
		animation.id = new_animation_id();
		animation.drawable = drawable;
		animation.property = property;
		animation.ease = ease;
		animation.ease_id = ease_id;
		animation.obj = obj;
		animation.slot = slot;
		animation.inertia = inertia;
		animation.anim_start_val = anim_start_val;
		animation.anim_dest_val = anim_dest_val;
		animation.prop_dest_val = prop_dest_val;
		animation.duration_ms = duration_ms;
		animation.start_ms = now_ms;
		animation.current_val = animation.anim_start_val;
		animation.prop_last_val = prop_start_val;
		animation.mass = 1.0f;
		animation.period = 0.0f;
		animation.amplitude = 0.0f;
		animation.strength = 0.0f;
		animation.period_set = false;
		animation.amplitude_set = false;
		animation.strength_set = false;
		animation.buffer[0] = animation.anim_start_val;
		animation.buffer[1] = animation.anim_start_val;
		animation.buffer[2] = animation.anim_start_val;

		for ( std::vector<FeAnimationState>::iterator it = list.begin(); it != list.end(); ++it )
		{
			if (( it->drawable != drawable ) || ( it->property->name != property->name ))
				continue;

			if ( property->get( drawable ) != it->prop_last_val )
			{
				list.erase( it );
				break;
			}

			if ( it->prop_dest_val == prop_dest_val )
				return FeAnimation( it->id, it->mass );

			if ( inertia && it->inertia )
			{
				animation.anim_start_val = it->current_val;
				animation.current_val = it->current_val;
				animation.prop_last_val = it->prop_last_val;
				animation.mass = it->mass;
				animation.buffer[0] = it->buffer[0];
				animation.buffer[1] = it->buffer[1];
				animation.buffer[2] = it->buffer[2];
			}

			list.erase( it );
			break;
		}

		if ( prop_start_val == animation.prop_dest_val )
		{
			if ( !drawable->set_animated_property( property->name, animation.prop_dest_val, true ))
				property->set( drawable, animation.prop_dest_val );

			return FeAnimation();
		}

		list.push_back( animation );
		return FeAnimation( animation.id, animation.mass );
	}

	void set_animation_value( FeAnimationState &animation, float value, bool snap=false )
	{
		if ( !animation.drawable->set_animated_property( animation.property->name, value, snap ))
			animation.property->set( animation.drawable, value );
	}
}

FeAnimation::FeAnimation()
	: m_id( 0 ),
	m_mass( 1.0f ),
	m_period( 0.0f ),
	m_amplitude( 0.0f ),
	m_strength( 0.0f ),
	m_period_set( false ),
	m_amplitude_set( false ),
	m_strength_set( false )
{
}

FeAnimation::FeAnimation( int id, float mass )
	: m_id( id ),
	m_mass( mass ),
	m_period( 0.0f ),
	m_amplitude( 0.0f ),
	m_strength( 0.0f ),
	m_period_set( false ),
	m_amplitude_set( false ),
	m_strength_set( false )
{
}

float FeAnimation::get_mass() const
{
	return m_mass;
}

void FeAnimation::set_mass( float value )
{
	if ( !std::isfinite( value ))
		return;

	m_mass = std::clamp( value, 0.0f, 1.0f );

	FeAnimationState *animation = find_animation( m_id );
	if ( animation )
		animation->mass = m_mass;
}

float FeAnimation::get_period() const
{
	return m_period;
}

void FeAnimation::set_period( float value )
{
	if ( !std::isfinite( value ))
		return;

	m_period = value;
	m_period_set = true;

	FeAnimationState *animation = find_animation( m_id );
	if ( animation )
	{
		animation->period = m_period;
		animation->period_set = true;
	}
}

float FeAnimation::get_amplitude() const
{
	return m_amplitude;
}

void FeAnimation::set_amplitude( float value )
{
	if ( !std::isfinite( value ))
		return;

	m_amplitude = value;
	m_amplitude_set = true;

	FeAnimationState *animation = find_animation( m_id );
	if ( animation )
	{
		animation->amplitude = m_amplitude;
		animation->amplitude_set = true;
	}
}

float FeAnimation::get_strength() const
{
	return m_strength;
}

void FeAnimation::set_strength( float value )
{
	if ( !std::isfinite( value ))
		return;

	m_strength = value;
	m_strength_set = true;

	FeAnimationState *animation = find_animation( m_id );
	if ( animation )
	{
		animation->strength = m_strength;
		animation->strength_set = true;
	}
}

bool FeAnimation::get_running() const
{
	return find_animation( m_id ) != NULL;
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

	float prop_dest_val = 0.0f;
	if ( !get_numeric_arg( vm, 3, prop_dest_val ))
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
	FeAnimation animation = replace_animation( drawable, property, ease, ease_id, obj, slot, ease_id == EaseInertia, prop_dest_val, duration_ms, now_ms );
	FePresent::script_flag_redraw();
	Sqrat::ClassType<FeAnimation>::PushInstanceCopy( vm, animation );

	return 1;
}

bool FeAnimate::tick( int now_ms )
{
	std::vector<FeAnimationState> &list = animations();
	if ( list.empty() )
		return false;

	bool redraw = false;

	for ( std::vector<FeAnimationState>::size_type i=0; i < list.size(); )
	{
		FeAnimationState &animation = list[i];
		if ( animation.property->get( animation.drawable ) != animation.prop_last_val )
		{
			list.erase( list.begin() + i );
			continue;
		}

		float elapsed = static_cast<float>( now_ms - animation.start_ms );
		if ( elapsed < 0.0f )
			elapsed = 0.0f;

		if ( elapsed >= animation.duration_ms )
		{
			set_animation_value( animation, animation.prop_dest_val, true );
			list.erase( list.begin() + i );
			redraw = true;
			continue;
		}

		float t = elapsed;
		float b = animation.anim_start_val;
		float c = animation.anim_dest_val - animation.anim_start_val;
		float d = animation.duration_ms;

		// TODO: cache the function like FeCallback does, or store in place of ease
		float current_val = animation.slot
			? Sqrat::Function( animation.obj, animation.slot ).Evaluate<float>( t, b, c, d )
			: apply_ease( animation, t, b, c, d );

		if ( animation.inertia )
			current_val = apply_inertia( animation, current_val, elapsed );

		animation.current_val = current_val;
		set_animation_value( animation, current_val );
		animation.prop_last_val = animation.property->get( animation.drawable );
		redraw = true;
		++i;
	}

	if ( redraw )
		FePresent::script_flag_redraw();

	return redraw;
}

void FeAnimate::remove( FeBasePresentable *drawable, const SQChar *property_name )
{
	std::vector<FeAnimationState> &list = animations();
	list.erase(
		std::remove_if(
			list.begin(),
			list.end(),
			[drawable, property_name]( const FeAnimationState &animation )
			{
				if ( animation.drawable != drawable )
					return false;

				return !property_name || ( animation.property->name == property_name );
			}),
		list.end() );
}

void FeAnimate::clear()
{
	animations().clear();
}
