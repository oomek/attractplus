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
		std::string slot;
		bool use_callback;
		bool inertia;
		bool running;
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
		float x1;
		float y1;
		float x2;
		float y2;
		float steps;
		int jump;
		bool repeat;
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
		( sizeof( ease_functions ) / sizeof( ease_functions[0] )) == EaseBezier,
		"Ease enum and ease function table are out of sync" );

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
		if (( ease < 0 ) || ( ease >= EaseBezier ))
			return NULL;

		return ease_functions[ease];
	}

	bool is_valid_ease( int ease )
	{
		return ( ease >= 0 ) && ( ease < EaseCount );
	}

	bool is_valid_jump( int jump )
	{
		return ( jump == JumpStart )
			|| ( jump == JumpEnd )
			|| ( jump == JumpNone )
			|| ( jump == JumpBoth );
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

	FeAnimationState *find_animation( FeBasePresentable *drawable, const std::string &property_name )
	{
		for ( FeAnimationState &animation : animations() )
			if (( animation.drawable == drawable ) && ( animation.property->name == property_name ))
				return &animation;

		return NULL;
	}

	void set_animation_ease( FeAnimationState &animation, int ease_id )
	{
		if ( !is_valid_ease( ease_id ))
			return;

		animation.ease = get_ease_function( ease_id );
		animation.ease_id = ease_id;
		animation.inertia = ease_id == EaseInertia;
		animation.use_callback = false;
		animation.slot.clear();
	}

	float apply_ease( const FeAnimationState &animation, float t, float b, float c, float d )
	{
		switch ( animation.ease_id )
		{
			case EaseBezier:
				return SqEase::cubic_bezier( t, b, c, d, animation.x1, animation.y1, animation.x2, animation.y2 );

			case EaseSteps:
				return SqEase::steps( t, b, c, d, animation.steps, animation.jump );

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

	void set_animation_value( FeAnimationState &animation, float value, bool snap=false )
	{
		if ( !animation.drawable->set_animated_property( animation.property->name, value, snap ))
			animation.property->set( animation.drawable, value );
	}

	void prepare_animated_property( FeBasePresentable *drawable, const FeAnimateProperty *property )
	{
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
	}

	FeAnimationState &create_animation_state(
		FeBasePresentable *drawable,
		const FeAnimateProperty *property )
	{
		std::vector<FeAnimationState> &list = animations();
		float prop_val = property->get( drawable );
		float anim_val = drawable->snap_grid_destination_to_pixels( property->name, prop_val );

		FeAnimationState animation;
		animation.id = new_animation_id();
		animation.drawable = drawable;
		animation.property = property;
		animation.ease = ease_functions[ EaseInertia ];
		animation.ease_id = EaseInertia;
		animation.use_callback = false;
		animation.inertia = true;
		animation.running = false;
		animation.anim_start_val = anim_val;
		animation.anim_dest_val = anim_val;
		animation.prop_dest_val = prop_val;
		animation.prop_last_val = prop_val;
		animation.duration_ms = 1000.0f;
		animation.start_ms = 0;
		animation.current_val = anim_val;
		animation.mass = 1.0f;
		animation.period = 0.0f;
		animation.amplitude = 0.0f;
		animation.strength = 0.0f;
		animation.x1 = 0.0f;
		animation.y1 = 0.0f;
		animation.x2 = 1.0f;
		animation.y2 = 1.0f;
		animation.steps = 1.0f;
		animation.jump = JumpEnd;
		animation.repeat = false;
		animation.period_set = false;
		animation.amplitude_set = false;
		animation.strength_set = false;
		animation.buffer[0] = anim_val;
		animation.buffer[1] = anim_val;
		animation.buffer[2] = anim_val;

		list.push_back( animation );
		return list.back();
	}

	FeAnimationState &get_animation_state(
		FeBasePresentable *drawable,
		const FeAnimateProperty *property )
	{
		FeAnimationState *animation = find_animation( drawable, property->name );
		if ( animation )
			return *animation;

		return create_animation_state( drawable, property );
	}

	void reset_animation(
		FeAnimationState &animation,
		float prop_dest_val,
		int now_ms,
		float anim_start_val )
	{
		float anim_dest_val = animation.drawable->snap_grid_destination_to_pixels( animation.property->name, prop_dest_val );

		animation.id = new_animation_id();
		animation.running = false;
		animation.anim_start_val = anim_start_val;
		animation.anim_dest_val = anim_dest_val;
		animation.prop_dest_val = prop_dest_val;
		animation.start_ms = now_ms;
		animation.current_val = anim_start_val;
		animation.buffer[0] = anim_start_val;
		animation.buffer[1] = anim_start_val;
		animation.buffer[2] = anim_start_val;

		set_animation_value( animation, anim_start_val );
		animation.prop_last_val = animation.property->get( animation.drawable );

		if (( anim_start_val == anim_dest_val ) || ( animation.duration_ms <= 0.0f ))
		{
			set_animation_value( animation, prop_dest_val, true );
			animation.current_val = anim_dest_val;
			animation.prop_last_val = animation.property->get( animation.drawable );
		}
		else
			animation.running = true;

		FePresent::script_flag_redraw();
	}

	int start_animation( FeAnimationState &animation, float prop_dest_val, int now_ms, bool force=false )
	{
		if ( force )
		{
			FeAnimationState replacement = animation;
			FeBasePresentable *drawable = animation.drawable;
			const FeAnimateProperty *property = animation.property;
			float prop_start_val = property->get( drawable );
			float anim_start_val = prop_start_val == animation.prop_last_val
				? animation.current_val
				: drawable->snap_grid_destination_to_pixels( property->name, prop_start_val );

			prepare_animated_property( drawable, property );
			FeAnimate::remove( drawable, property->name.c_str() );

			animations().push_back( replacement );
			reset_animation( animations().back(), prop_dest_val, now_ms, anim_start_val );
			return animations().back().id;
		}

		if (( animation.prop_dest_val == prop_dest_val )
				&& ( animation.property->get( animation.drawable ) == animation.prop_last_val ))
			return animation.id;

		prepare_animated_property( animation.drawable, animation.property );

		float prop_start_val = animation.property->get( animation.drawable );
		bool continuing = !force && animation.running && ( prop_start_val == animation.prop_last_val );
		float anim_start_val = continuing
			? animation.current_val
			: animation.drawable->snap_grid_destination_to_pixels( animation.property->name, prop_start_val );
		float anim_dest_val = animation.drawable->snap_grid_destination_to_pixels( animation.property->name, prop_dest_val );

		animation.anim_start_val = anim_start_val;
		animation.anim_dest_val = anim_dest_val;
		animation.prop_dest_val = prop_dest_val;
		animation.prop_last_val = prop_start_val;
		animation.start_ms = now_ms;
		animation.current_val = anim_start_val;

		if ( !( animation.inertia && continuing ))
		{
			animation.buffer[0] = anim_start_val;
			animation.buffer[1] = anim_start_val;
			animation.buffer[2] = anim_start_val;
		}

		if (( anim_start_val == anim_dest_val ) || ( animation.duration_ms <= 0.0f ))
		{
			set_animation_value( animation, prop_dest_val, true );
			animation.current_val = anim_dest_val;
			animation.prop_last_val = animation.property->get( animation.drawable );
			animation.running = false;
		}
		else
			animation.running = true;

		FePresent::script_flag_redraw();
		return animation.id;
	}
}

FeAnimation::FeAnimation( int id )
	: m_id( id ),
	m_to( 0.0f ),
	m_time( 1000.0f ),
	m_ease( EaseInertia ),
	m_mass( 1.0f ),
	m_period( 0.0f ),
	m_amplitude( 0.0f ),
	m_strength( 0.0f ),
	m_x1( 0.0f ),
	m_y1( 0.0f ),
	m_x2( 1.0f ),
	m_y2( 1.0f ),
	m_steps( 1.0f ),
	m_jump( JumpEnd ),
	m_repeat( false ),
	m_period_set( false ),
	m_amplitude_set( false ),
	m_strength_set( false )
{
}

float FeAnimation::get_to() const
{
	FeAnimationState *animation = find_animation( m_id );
	return animation ? animation->prop_dest_val : m_to;
}

void FeAnimation::set_to( float value )
{
	if ( !std::isfinite( value ))
		return;

	m_to = value;

	FeAnimationState *animation = find_animation( m_id );
	if ( animation )
	{
		FePresent *fep = FePresent::script_get_fep();
		int now_ms = fep ? fep->get_layout_ms() : 0;
		start_animation( *animation, m_to, now_ms );
	}
}

float FeAnimation::get_time() const
{
	FeAnimationState *animation = find_animation( m_id );
	return animation ? animation->duration_ms : m_time;
}

void FeAnimation::set_time( float value )
{
	if ( !std::isfinite( value ))
		return;

	m_time = std::max( 0.0f, value );

	FeAnimationState *animation = find_animation( m_id );
	if ( animation )
		animation->duration_ms = m_time;
}

int FeAnimation::get_ease() const
{
	FeAnimationState *animation = find_animation( m_id );
	return animation ? animation->ease_id : m_ease;
}

void FeAnimation::set_ease( int value )
{
	if ( !is_valid_ease( value ))
		return;

	m_ease = value;

	FeAnimationState *animation = find_animation( m_id );
	if ( animation )
		set_animation_ease( *animation, m_ease );
}

float FeAnimation::get_mass() const
{
	FeAnimationState *animation = find_animation( m_id );
	return animation ? animation->mass : m_mass;
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
	FeAnimationState *animation = find_animation( m_id );
	return animation ? animation->period : m_period;
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
	FeAnimationState *animation = find_animation( m_id );
	return animation ? animation->amplitude : m_amplitude;
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
	FeAnimationState *animation = find_animation( m_id );
	return animation ? animation->strength : m_strength;
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

float FeAnimation::get_x1() const
{
	FeAnimationState *animation = find_animation( m_id );
	return animation ? animation->x1 : m_x1;
}

void FeAnimation::set_x1( float value )
{
	if ( !std::isfinite( value ))
		return;

	m_x1 = std::clamp( value, 0.0f, 1.0f );

	FeAnimationState *animation = find_animation( m_id );
	if ( animation )
		animation->x1 = m_x1;
}

float FeAnimation::get_y1() const
{
	FeAnimationState *animation = find_animation( m_id );
	return animation ? animation->y1 : m_y1;
}

void FeAnimation::set_y1( float value )
{
	if ( !std::isfinite( value ))
		return;

	m_y1 = value;

	FeAnimationState *animation = find_animation( m_id );
	if ( animation )
		animation->y1 = m_y1;
}

float FeAnimation::get_x2() const
{
	FeAnimationState *animation = find_animation( m_id );
	return animation ? animation->x2 : m_x2;
}

void FeAnimation::set_x2( float value )
{
	if ( !std::isfinite( value ))
		return;

	m_x2 = std::clamp( value, 0.0f, 1.0f );

	FeAnimationState *animation = find_animation( m_id );
	if ( animation )
		animation->x2 = m_x2;
}

float FeAnimation::get_y2() const
{
	FeAnimationState *animation = find_animation( m_id );
	return animation ? animation->y2 : m_y2;
}

void FeAnimation::set_y2( float value )
{
	if ( !std::isfinite( value ))
		return;

	m_y2 = value;

	FeAnimationState *animation = find_animation( m_id );
	if ( animation )
		animation->y2 = m_y2;
}

float FeAnimation::get_steps() const
{
	FeAnimationState *animation = find_animation( m_id );
	return animation ? animation->steps : m_steps;
}

void FeAnimation::set_steps( float value )
{
	if ( !std::isfinite( value ))
		return;

	m_steps = std::max( 1.0f, value );

	FeAnimationState *animation = find_animation( m_id );
	if ( animation )
		animation->steps = m_steps;
}

int FeAnimation::get_jump() const
{
	FeAnimationState *animation = find_animation( m_id );
	return animation ? animation->jump : m_jump;
}

void FeAnimation::set_jump( int value )
{
	if ( !is_valid_jump( value ))
		return;

	m_jump = value;

	FeAnimationState *animation = find_animation( m_id );
	if ( animation )
		animation->jump = m_jump;
}

bool FeAnimation::get_repeat() const
{
	FeAnimationState *animation = find_animation( m_id );
	return animation ? animation->repeat : m_repeat;
}

void FeAnimation::set_repeat( bool value )
{
	m_repeat = value;

	FeAnimationState *animation = find_animation( m_id );
	if ( animation )
		animation->repeat = m_repeat;
}

bool FeAnimation::get_running() const
{
	FeAnimationState *animation = find_animation( m_id );
	return animation ? animation->running : false;
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

SQInteger FeAnimate::script_move( HSQUIRRELVM vm )
{
	SQInteger arg_count = sq_gettop( vm );
	if (( arg_count < 2 ) || ( arg_count > 7 ))
		return sq_throwerror( vm, _SC("move() requires property") );

	FeBasePresentable *drawable = Sqrat::ClassType<FeBasePresentable>::GetInstance( vm, 1 );
	if ( !drawable )
	{
		if ( Sqrat::Error::Instance().Occurred( vm ) )
			return sq_throwerror( vm, Sqrat::Error::Instance().Message( vm ).c_str() );

		return sq_throwerror( vm, _SC("move() must be called on a drawable") );
	}

	const SQChar *property_name = NULL;
	if (( sq_gettype( vm, 2 ) != OT_STRING )
			|| SQ_FAILED( sq_getstring( vm, 2, &property_name ))
			|| !property_name
			|| ( property_name[0] == '\0' ))
		return sq_throwerror( vm, _SC("move() property must be a non-empty string") );

	bool force_restart = false;
	SQInteger value_arg_count = arg_count;
	if (( value_arg_count >= 6 ) && ( sq_gettype( vm, value_arg_count ) == OT_BOOL ))
	{
		SQBool force = SQFalse;
		sq_getbool( vm, value_arg_count, &force );
		force_restart = force != SQFalse;
		--value_arg_count;
	}

	if ( value_arg_count > 6 )
		return sq_throwerror( vm, _SC("move() force flag must be a bool") );

	bool has_destination = value_arg_count >= 3;
	float prop_dest_val = 0.0f;
	if ( has_destination && !get_numeric_arg( vm, 3, prop_dest_val ))
		return sq_throwerror( vm, _SC("move() destination must be numeric") );

	bool has_time = value_arg_count >= 4;
	float duration_ms = 1000.0f;
	if ( has_time )
	{
		if ( !get_numeric_arg( vm, 4, duration_ms ))
			return sq_throwerror( vm, _SC("move() invalid time value") );

		duration_ms = std::max( 0.f, duration_ms );
	}

	bool has_ease = false;
	int ease_id = EaseInertia;
	Sqrat::Object obj;
	std::string slot;
	bool use_callback = false;

	if ( value_arg_count == 5 )
	{
		if ( sq_gettype( vm, 5 ) == OT_INTEGER )
		{
			SQInteger ease_value = 0;
			sq_getinteger( vm, 5, &ease_value );
			ease_id = static_cast<int>( ease_value );
			if ( !is_valid_ease( ease_id ))
				return sq_throwerror( vm, _SC("move() ease is not a supported Ease value") );

			has_ease = true;
		}
		else if ( sq_gettype( vm, 5 ) == OT_STRING )
		{
			const SQChar *slot_name = NULL;
			sq_getstring( vm, 5, &slot_name );
			obj = Sqrat::RootTable( vm );
			slot = slot_name ? slot_name : "";
			use_callback = true;
		}
		else
			return sq_throwerror( vm, _SC("move() invalid ease or callback") );
	}

	if ( value_arg_count == 6 )
	{
		if ( sq_gettype( vm, 6 ) != OT_STRING )
			return sq_throwerror( vm, _SC("move() callback name must be a string") );

		const SQChar *slot_name = NULL;
		HSQOBJECT po;
		sq_getstackobj( vm, 5, &po );
		sq_getstring( vm, 6, &slot_name );
		obj = po;
		slot = slot_name ? slot_name : "";
		use_callback = true;
	}

	if ( use_callback )
	{
		Sqrat::Function cb = Sqrat::Function( obj, slot.c_str() );

		if ( cb.IsNull() )
			return sq_throwerror( vm, _SC("move() invalid callback found") );
	}

	bool property_name_found = false;
	const FeAnimateProperty *property = find_property( property_name, drawable, property_name_found );
	if ( !property )
	{
		std::string message;
		if ( property_name_found )
		{
			message = "move() property '";
			message += property_name;
			message += "' is not supported for this drawable";
		}
		else
		{
			message = "move() unknown property: ";
			message += property_name;
		}
		return sq_throwerror( vm, message.c_str() );
	}

	FeAnimationState &animation = get_animation_state( drawable, property );
	float prop_value = animation.property->get( animation.drawable );

	if ( has_destination
			&& !force_restart
			&& ( animation.prop_dest_val == prop_dest_val )
			&& ( prop_value == animation.prop_last_val ))
	{
		Sqrat::ClassType<FeAnimation>::PushInstanceCopy( vm, FeAnimation( animation.id ) );
		return 1;
	}

	if ( has_time )
		animation.duration_ms = duration_ms;

	if ( has_ease )
		set_animation_ease( animation, ease_id );

	if ( use_callback )
	{
		animation.obj = obj;
		animation.slot = slot;
		animation.use_callback = true;
		animation.inertia = false;
	}

	int result_id = animation.id;

	if ( has_destination )
	{
		FePresent *fep = FePresent::script_get_fep();
		int now_ms = fep ? fep->get_layout_ms() : 0;
		result_id = start_animation( animation, prop_dest_val, now_ms, force_restart );
	}

	Sqrat::ClassType<FeAnimation>::PushInstanceCopy( vm, FeAnimation( result_id ) );
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
		if ( !animation.running )
		{
			++i;
			continue;
		}

		float prop_value = animation.property->get( animation.drawable );
		if ( prop_value != animation.prop_last_val )
		{
			animation.running = false;
			++i;
			continue;
		}

		float elapsed = static_cast<float>( now_ms - animation.start_ms );
		if ( elapsed < 0.0f )
			elapsed = 0.0f;

		if ( elapsed >= animation.duration_ms )
		{
			float next_val = animation.repeat ? animation.anim_start_val : animation.prop_dest_val;
			set_animation_value( animation, next_val, true );
			animation.current_val = animation.repeat ? animation.anim_start_val : animation.anim_dest_val;
			animation.prop_last_val = animation.property->get( animation.drawable );
			animation.running = animation.repeat;
			if ( animation.repeat )
			{
				animation.start_ms = now_ms;
				animation.buffer[0] = animation.anim_start_val;
				animation.buffer[1] = animation.anim_start_val;
				animation.buffer[2] = animation.anim_start_val;
			}

			redraw = true;
			++i;
			continue;
		}

		float t = elapsed;
		float b = animation.anim_start_val;
		float c = animation.anim_dest_val - animation.anim_start_val;
		float d = animation.duration_ms;

		// TODO: cache the function like FeCallback does, or store in place of ease
		float current_val = animation.use_callback
			? Sqrat::Function( animation.obj, animation.slot.c_str() ).Evaluate<float>( t, b, c, d )
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
