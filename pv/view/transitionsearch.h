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

#ifndef PULSEVIEW_PV_VIEW_TRANSITIONSEARCH_H
#define PULSEVIEW_PV_VIEW_TRANSITIONSEARCH_H

#include <memory>
#include <vector>

#include <QObject>

namespace pv {

namespace data {
class LogicSnapshot;
}

namespace view {

class LogicSignal;

/**
 * Class to seach a transition on a logic signal.
 */
class TransitionSearch : public QObject
{
	Q_OBJECT

	/**
	 * Number of samples that are searched at once, so
	 * that the event loop is not blocked for too long.
	 */
	static const size_t SamplesPerRound;

	/**
	 * Distance around the exact sample time that is rounded to the sample.
	 */
	static const double SampleNeighbourhood;

public:
	enum class Direction {
		previous, next
	};

	enum class Edge {
		any,
		rising,
		falling
	};

	/**
	 * Creates a new TransitionSearch object.
	 * @param parent The parent object.
	 * @param sig The signal to search through.
	 * @param start The time where the search should start.
	 * @param direction The direction in which to search.
	 * @param edge The edge to search for.
	 */
	TransitionSearch(
		QObject *parent,
		const std::shared_ptr<view::LogicSignal> &sig,
		double start,
		Direction direction,
		Edge edge);

signals:
	/**
	 * Signal emitted when the search finished.
	 * @param success 'true' if a requested edge is found,
	 *                'false' otherwise.
	 * @param where If success is 'true', the time where the edge is.
	 */
	void done(bool success, double where);

private slots:
	void start_search(double start);

	void continue_search();

private:
	// helper functions for the initialization list
	std::shared_ptr<pv::data::LogicSnapshot> get_snapshot(
		const std::shared_ptr<view::LogicSignal> &sig);
	double get_samplerate(
		const std::shared_ptr<view::LogicSignal> &sig);

	// returns the sample from the underlying snapshot
	// with the signal mask applied
	uint64_t get_sample(int64_t number);

	// calculates the index of the sample where the time 't' falls into
	int64_t time_to_sample_number(double t);

	// calculate the time of the sample 'n'
	double sample_number_to_time(int64_t n);

	// the direction we search in
	Direction _direction;

	// the type of edge we search for
	Edge _edge;

	// the number of the sample where we stand at the moment
	int64_t _position;

	// the data we are searching through
	const std::shared_ptr<pv::data::LogicSnapshot> _snapshot;

	// the samplerate of the data
	const double _samplerate;

	// the start time of the data
	const double _start_time;

	// the signal bitmask of the channel
	const uint64_t _signal_mask;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_TRANSITIONSEARCH_H
