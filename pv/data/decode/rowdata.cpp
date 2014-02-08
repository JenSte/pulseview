/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include <cassert>

#include <boost/thread.hpp>

#include "rowdata.h"

using boost::thread;
using std::inserter;
using std::vector;

namespace pv {
namespace data {
namespace decode {

RowData::RowData()
{
}

uint64_t RowData::get_max_sample() const
{
	if (_annotations.empty())
		return 0;
	return _annotations.back().end_sample();
}

void RowData::get_annotation_subset(
	vector<pv::data::decode::Annotation> &dest,
	uint64_t start_sample, uint64_t end_sample) const
{
	const size_t ThreadCount = 4;
	vector<Annotation> sections[ThreadCount];
	thread threads[ThreadCount];

	const size_t section_length = (_annotations.size() + ThreadCount - 1) /
		ThreadCount;

	for (size_t i = 0; i < ThreadCount; i++) {
		const vector<Annotation>::const_iterator first =
			_annotations.begin() + section_length * i;
		const vector<Annotation>::const_iterator last =
			_annotations.begin() +
			std::min(section_length * (i+1), _annotations.size());

		threads[i] = thread(RowData::search_annotation_subset,
			first, last, start_sample, end_sample, sections + i);
	}

	for (size_t i = 0; i < ThreadCount; i++) {
		threads[i].join();
		copy(sections[i].begin(), sections[i].end(),
			back_inserter(dest));
	}
}

void RowData::search_annotation_subset(
	vector<Annotation>::const_iterator first,
	vector<Annotation>::const_iterator last,
	uint64_t start_sample, uint64_t end_sample,
	vector<Annotation> *const dest)
{
	assert(dest);
	while(first != last) {
		if ((*first).end_sample() > start_sample &&
			(*first).start_sample() <= end_sample)
			dest->push_back(*first);
		first++;
	}
}

void RowData::push_annotation(const Annotation &a)
{
	_annotations.push_back(a);
}

} // decode
} // data
} // pv
