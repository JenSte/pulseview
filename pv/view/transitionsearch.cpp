/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2014 Jens Steinhauser <jens.steinhauser@gmail.com>
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

#include "transitionsearch.h"
#include "logicsignal.h"

#include "pv/data/logic.h"
#include "pv/data/logicsnapshot.h"

using std::deque;
using std::shared_ptr;

namespace pv {
namespace view {

const size_t TransitionSearch::SamplesPerRound = 1000;

const double TransitionSearch::SampleNeighbourhood = 1e-16;

TransitionSearch::TransitionSearch(
	QObject *parent,
	const std::shared_ptr<view::LogicSignal> &sig,
	double start,
	Direction direction,
	Edge edge) :
	QObject(parent),
	_direction(direction),
	_edge(edge),
	_snapshot(get_snapshot(sig)),
	_samplerate(get_samplerate(sig)),
	_start_time(sig->logic_data()->get_start_time()),
	_signal_mask(1ULL << sig->probe()->index)
{
	// do the rest of the initialization (that may emit a signal) later,
	// so it's possible to connect to the signal first
	QMetaObject::invokeMethod(this, "start_search",
		Qt::QueuedConnection, Q_ARG(double, start));
}

std::shared_ptr<pv::data::LogicSnapshot> TransitionSearch::get_snapshot(
	const std::shared_ptr<view::LogicSignal> &sig)
{
	const deque<shared_ptr<pv::data::LogicSnapshot>> &snapshots =
		sig->logic_data()->get_snapshots();
	if (!snapshots.empty()) {
		return snapshots[0];
	}

	return NULL;
}

double TransitionSearch::get_samplerate(
	const std::shared_ptr<view::LogicSignal> &sig)
{
	double samplerate = sig->logic_data()->samplerate();
	if (samplerate == 0.0)
		samplerate = 1.0;

	return samplerate;
}

uint64_t TransitionSearch::get_sample(int64_t number)
{
	return _snapshot->get_sample(number) & _signal_mask;
}

int64_t TransitionSearch::time_to_sample_number(double t)
{
	return std::fabs((t - _start_time) * _samplerate);
}

double TransitionSearch::sample_number_to_time(int64_t n)
{
	return _start_time + n / _samplerate;
}

void TransitionSearch::start_search(double start)
{
	if (!_snapshot) {
		emit done(false, 0.0);
		return;
	}

	// calculate the number of the sample where the start time falls into,
	// and the distance to the exact times of the previous and the next sample
	_position = time_to_sample_number(start);
	const double dist_p = std::fabs(start - sample_number_to_time(_position));
	const double dist_n = std::fabs(start - sample_number_to_time(_position + 1));

	const int64_t last = _snapshot->get_sample_count() - 1;
	if (((_direction == Direction::previous) && (_position < 1)) ||
		((_direction == Direction::next    ) && (_position > last - 1))) {
		emit done(false, 0.0);
		return;
	}

	// Because of the rounding error, if this function is invoked multiple
	// times, we detect the same transition over and over again. To avoid this
	// we skip one sample if we are already very close to a sample.
	if (_direction == Direction::previous) {
		if (dist_p < SampleNeighbourhood) {
			_position--;
		}
	} else {
		if (dist_n < SampleNeighbourhood) {
			_position++;
		}
	}

	continue_search();
}

void TransitionSearch::continue_search(void)
{
	uint64_t current = get_sample(_position);

	if (_direction == Direction::next) {
		const int64_t last_sample = _snapshot->get_sample_count() - 1;
		const int64_t limit =
			std::min<int64_t>(_position + SamplesPerRound, last_sample);

		while (_position < limit) {
			uint64_t next = get_sample(_position + 1);

			if (current != next) {
				if ( (_edge == Edge::any    ) ||
					((_edge == Edge::rising ) && (0 == current)) ||
					((_edge == Edge::falling) && (0 != current))) {
					emit done(true, sample_number_to_time(_position + 1));
					return;
				}
			}

			current = next;
			_position++;
		}

		if (_position >= last_sample) {
			// We reached the end of the signal.
			emit done(false, 0.0);
			return;
		}
	} else {
		while (0 < _position) {
			uint64_t prev = get_sample(_position - 1);

			if (current != prev) {
				if ( (_edge == Edge::any    ) ||
					((_edge == Edge::rising ) && (0 != current)) ||
					((_edge == Edge::falling) && (0 == current))) {
					emit done(true, sample_number_to_time(_position));
					return;
				}
			}

			current = prev;
			_position--;
		}

		if (_position <= 0) {
			// We reached the start of the signal.
			emit done(false, 0.0);
			return;
		}
	}

	// Don't block the event loop for too long.
	QMetaObject::invokeMethod(this, "continue_search", Qt::QueuedConnection);
}

} // namespace view
} // namespace pv
