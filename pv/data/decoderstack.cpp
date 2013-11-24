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

#include <libsigrokdecode/libsigrokdecode.h>

#include <boost/thread/thread.hpp>

#include <stdexcept>

#include <QDebug>

#include "decoderstack.h"

#include <pv/data/logic.h>
#include <pv/data/logicsnapshot.h>
#include <pv/view/logicsignal.h>
#include <pv/view/decode/annotation.h>

using namespace boost;
using namespace std;

namespace pv {
namespace data {

const double DecoderStack::DecodeMargin = 1.0;
const double DecoderStack::DecodeThreshold = 0.2;
const int64_t DecoderStack::DecodeChunkLength = 4096;

mutex DecoderStack::_global_decode_mutex;

DecoderStack::DecoderStack(const srd_decoder *const dec) :
	_decoder(dec),
	_options(g_hash_table_new_full(g_str_hash,
		g_str_equal, g_free, (GDestroyNotify)g_variant_unref))
{
}

DecoderStack::~DecoderStack()
{
	_decode_thread.interrupt();
	_decode_thread.join();

	g_hash_table_destroy(_options);
}

const srd_decoder* DecoderStack::decoder() const
{
	return _decoder;
}

const map<const srd_probe*, shared_ptr<view::LogicSignal> >&
DecoderStack::probes() const
{
	return _probes;
}

void DecoderStack::set_probes(std::map<const srd_probe*,
	boost::shared_ptr<view::LogicSignal> > probes)
{
	_probes = probes;
	begin_decode();
}

const GHashTable* DecoderStack::options() const
{
	return _options;
}

void DecoderStack::set_option(const char *id, GVariant *value)
{
	g_variant_ref(value);
	g_hash_table_replace(_options, (void*)g_strdup(id), value);
	begin_decode();
}

const vector< shared_ptr<view::decode::Annotation> >
	DecoderStack::annotations() const
{
	lock_guard<mutex> lock(_mutex);
	return _annotations;
}

QString DecoderStack::error_message()
{
	lock_guard<mutex> lock(_mutex);
	return _error_message;
}

void DecoderStack::begin_decode()
{
	_decode_thread.interrupt();
	_decode_thread.join();

	_annotations.clear();

	if (_probes.empty())
		return;

	// Get the samplerate and start time
	shared_ptr<pv::view::LogicSignal> logic_signal =
		dynamic_pointer_cast<pv::view::LogicSignal>(
			(*_probes.begin()).second);
	if (logic_signal) {
		shared_ptr<pv::data::Logic> data(
			logic_signal->data());
		if (data) {
			_start_time = data->get_start_time();
			_samplerate = data->get_samplerate();
			if (_samplerate == 0.0)
				_samplerate = 1.0;
		}
	}

	// We get the logic data of the first probe in the list.
	// This works because we are currently assuming all
	// LogicSignals have the same data/snapshot
	shared_ptr<pv::view::LogicSignal> sig = (*_probes.begin()).second;
	assert(sig);
	shared_ptr<data::Logic> data = sig->data();

	_decode_thread = boost::thread(&DecoderStack::decode_proc, this,
		data);
}

void DecoderStack::clear_snapshots()
{
}

void DecoderStack::decode_proc(shared_ptr<data::Logic> data)
{
	srd_session *session;
	uint8_t chunk[DecodeChunkLength];

	assert(data);

	const deque< shared_ptr<pv::data::LogicSnapshot> > &snapshots =
		data->get_snapshots();
	if (snapshots.empty())
		return;

	const shared_ptr<pv::data::LogicSnapshot> &snapshot =
		snapshots.front();
	const int64_t sample_count = snapshot->get_sample_count() - 1;

	// Create the session
	srd_session_new(&session);
	assert(session);

	srd_session_metadata_set(session, SRD_CONF_SAMPLERATE,
		g_variant_new_uint64((uint64_t)_samplerate));

	srd_pd_output_callback_add(session, SRD_OUTPUT_ANN,
		DecoderStack::annotation_callback, this);

	// Create the decoder instance
	srd_decoder_inst *const decoder_inst = srd_inst_new(
		session, _decoder->id, _options);
	if(!decoder_inst) {
		_error_message = tr("Failed to initialise decoder");
		return;
	}

	// Setup the probes
	GHashTable *const probes = g_hash_table_new_full(g_str_hash,
		g_str_equal, g_free, (GDestroyNotify)g_variant_unref);

	for(map<const srd_probe*, shared_ptr<view::LogicSignal> >::
		const_iterator i = _probes.begin();
		i != _probes.end(); i++)
	{
		shared_ptr<view::Signal> signal((*i).second);
		GVariant *const gvar = g_variant_new_int32(
			signal->probe()->index);
		g_variant_ref_sink(gvar);
		g_hash_table_insert(probes, (*i).first->id, gvar);
	}

	srd_inst_probe_set_all(decoder_inst, probes);

	// Start the session
	srd_session_start(session);

	for (int64_t i = 0;
		!this_thread::interruption_requested() && i < sample_count;
		i += DecodeChunkLength)
	{
		lock_guard<mutex> decode_lock(_global_decode_mutex);

		const int64_t chunk_end = min(
			i + DecodeChunkLength, sample_count);
		snapshot->get_samples(chunk, i, chunk_end);

		if (srd_session_send(session, i, i + sample_count,
				chunk, chunk_end - i) != SRD_OK) {
			_error_message = tr("Failed to initialise decoder");
			break;
		}
	}

	// Destroy the session
	srd_session_destroy(session);
}

void DecoderStack::annotation_callback(srd_proto_data *pdata, void *decoder)
{
	using namespace pv::view::decode;

	assert(pdata);
	assert(decoder);

	DecoderStack *const d = (DecoderStack*)decoder;

	shared_ptr<Annotation> a(new Annotation(pdata));
	lock_guard<mutex> lock(d->_mutex);
	d->_annotations.push_back(a);

	d->new_decode_data();
}

} // namespace data
} // namespace pv
