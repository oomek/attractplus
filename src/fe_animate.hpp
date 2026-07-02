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

#ifndef FE_ANIMATE_HPP
#define FE_ANIMATE_HPP

#include <squirrel.h>

#include <functional>
#include <typeinfo>

class FeBasePresentable;

enum FeEase
{
	EaseLinear = 0,
	EaseInQuad,
	EaseOutQuad,
	EaseInOutQuad,
	EaseOutInQuad,
	EaseInCubic,
	EaseOutCubic,
	EaseInOutCubic,
	EaseOutInCubic,
	EaseInQuart,
	EaseOutQuart,
	EaseInOutQuart,
	EaseOutInQuart,
	EaseInQuint,
	EaseOutQuint,
	EaseInOutQuint,
	EaseOutInQuint,
	EaseInSine,
	EaseOutSine,
	EaseInOutSine,
	EaseOutInSine,
	EaseInExpo,
	EaseOutExpo,
	EaseInOutExpo,
	EaseOutInExpo,
	EaseInExpo2,
	EaseOutExpo2,
	EaseInOutExpo2,
	EaseOutInExpo2,
	EaseInCirc,
	EaseOutCirc,
	EaseInOutCirc,
	EaseOutInCirc,
	EaseInBack,
	EaseOutBack,
	EaseInOutBack,
	EaseOutInBack,
	EaseInBack2,
	EaseOutBack2,
	EaseInOutBack2,
	EaseOutInBack2,
	EaseInBounce,
	EaseOutBounce,
	EaseInOutBounce,
	EaseOutInBounce,
	EaseInBounce2,
	EaseOutBounce2,
	EaseInOutBounce2,
	EaseOutInBounce2,
	EaseInElastic,
	EaseOutElastic,
	EaseInOutElastic,
	EaseOutInElastic,
	EaseInElastic2,
	EaseOutElastic2,
	EaseInOutElastic2,
	EaseOutInElastic2,
	EaseCount
};

class FeAnimate
{
public:
	typedef std::function<bool ( FeBasePresentable * )> PropertyMatcher;
	typedef std::function<float ( FeBasePresentable * )> PropertyGetter;
	typedef std::function<void ( FeBasePresentable *, float )> PropertySetter;

	static SQInteger script_animate( HSQUIRRELVM vm );
	static bool tick( int now_ms );
	static void remove( FeBasePresentable *drawable );
	static void clear();

	static void register_property( const SQChar *name, const std::type_info &owner, PropertyMatcher matches, PropertyGetter get, PropertySetter set );

	template<class C, class V, class Getter>
	static const SQChar *animated_property( const SQChar *name, Getter getter, void (C::*setter)(V) )
	{
		register_property(
			name,
			typeid( C ),
			property_matcher<C>(),
			property_getter<C>( getter ),
			property_setter<C, V>( setter ) );

		return name;
	}

private:
	template<class C>
	static PropertyMatcher property_matcher()
	{
		return []( FeBasePresentable *drawable ) -> bool
		{
			return dynamic_cast<C *>( drawable ) != NULL;
		};
	}

	template<class C, class Getter>
	static PropertyGetter property_getter( Getter getter )
	{
		return [getter]( FeBasePresentable *drawable ) -> float
		{
			return get_property<C>( drawable, getter );
		};
	}

	template<class C, class V>
	static PropertySetter property_setter( void (C::*setter)(V) )
	{
		return [setter]( FeBasePresentable *drawable, float value )
		{
			set_property<C, V>( drawable, setter, value );
		};
	}

	template<class C, class Getter>
	static float get_property( FeBasePresentable *drawable, Getter getter )
	{
		return static_cast<float>( std::invoke( getter, static_cast<C *>( drawable )));
	}

	template<class C, class V>
	static void set_property( FeBasePresentable *drawable, void (C::*setter)(V), float value )
	{
		std::invoke( setter, static_cast<C *>( drawable ), static_cast<V>( value ));
	}
};

#endif
