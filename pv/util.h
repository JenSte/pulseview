/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef PULSEVIEW_UTIL_H
#define PULSEVIEW_UTIL_H

#include <glib.h>
#include <math.h>

#include <QString>

namespace pv {
namespace util {

extern const int FirstSIPrefixPower;

/**
 * Formats a given time value with the specified SI prefix.
 * @param t The time value in seconds to format.
 * @param prefix The number of the prefix, from 0 for 'femto' up to
 *               8 for 'giga'.
 * @parma precision The number of digits after the decimal separator.
 * @param sign Whether or not to add a sign also for positive numbers.
 *
 * @return The formated value.
 */
QString format_time(
	double t, unsigned int prefix,
	unsigned precision = 0, bool sign = true);

/**
 * Formats a given time value with a SI prefix so that the
 * value is between 1 and 999.
 * @param second The time value in seconds to format.
 *
 * @return The formated value.
 */
QString format_second(double second);

/**
 * STL adapter for glibs GSList data type.
 *
 * Note that this class doesn't take ownership of the list, it just provides
 * (input) iterators to use the list in STL algorithms.
 */
template<typename T>
class GSL_range {
	const GSList *_list;

public:
	class It : public std::iterator<std::input_iterator_tag, T> {
		const GSList *_l;

	public:
		It(const GSList *l) :
			_l(l)
		{ }

		T operator*() const
		{
			return static_cast<T>(_l->data);
		}

		const It& operator++()
		{
			_l = _l->next;
			return *this;
		}

		bool operator==(const It& other)
		{
			return _l == other._l;
		}

		bool operator!=(const It& other)
		{
			return _l != other._l;
		}
	};

	GSL_range(const GSList *list) :
		_list(list)
	{ }

	It begin() { return { _list }; }
	It end() { return { nullptr }; }
};

} // namespace util
} // namespace pv

#endif // PULSEVIEW_UTIL_H
