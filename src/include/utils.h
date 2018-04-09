/*
 * Copyright (C) 2012  www.scratchapixel.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <algorithm>

#ifndef M_PI
#define M_PI (3.14159265358979323846f)
#endif

namespace
{
	inline float mix(const float &a, const float &b, const float &mix)
	{
		return b * mix + a * (1 - mix);
	}

	inline float clamp(const float &lo, const float &hi, const float &v)
	{
		return std::max(lo, std::min(hi, v));
	}

	inline float deg2rad(const float &deg)
	{
		return deg * M_PI / 180;
	}
}