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

#ifndef PULSEVIEW_PV_VIEW_DECODETRACE_H
#define PULSEVIEW_PV_VIEW_DECODETRACE_H

#include "trace.h"

#include <map>

#include <boost/shared_ptr.hpp>

#include <pv/prop/binding/decoderoptions.h>

struct srd_probe;

class QComboBox;

namespace pv {

namespace data {
class DecoderStack;
}

namespace view {

class DecodeTrace : public Trace
{
	Q_OBJECT

private:
	static const QColor DecodeColours[4];
	static const QColor ErrorBgColour;

public:
	DecodeTrace(pv::SigSession &session,
		boost::shared_ptr<pv::data::DecoderStack> decoder_stack,
		int index);

	bool enabled() const;

	const boost::shared_ptr<pv::data::DecoderStack>& decoder() const;

	void set_view(pv::view::View *view);

	/**
	 * Paints the background layer of the trace with a QPainter
	 * @param p the QPainter to paint into.
	 * @param left the x-coordinate of the left edge of the signal.
	 * @param right the x-coordinate of the right edge of the signal.
	 **/
	void paint_back(QPainter &p, int left, int right);

	/**
	 * Paints the mid-layer of the trace with a QPainter
	 * @param p the QPainter to paint into.
	 * @param left the x-coordinate of the left edge of the signal
	 * @param right the x-coordinate of the right edge of the signal
	 **/
	void paint_mid(QPainter &p, int left, int right);

	void populate_popup_form(QWidget *parent, QFormLayout *form);

	QMenu* create_context_menu(QWidget *parent);

	void delete_pressed();

private:
	void draw_error(QPainter &p, const QString &message,
		int left, int right);

	QComboBox* create_probe_selector(
		QWidget *parent, const srd_probe *const probe);

	void commit_probes();

private slots:
	void on_new_decode_data();

	void on_delete();

	void on_probe_selected(int);

private:
	boost::shared_ptr<pv::data::DecoderStack> _decoder_stack;

	uint64_t _decode_start, _decode_end;

	pv::prop::binding::DecoderOptions _binding;

	std::map<const srd_probe*, QComboBox*> _probe_selector_map;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_DECODETRACE_H
