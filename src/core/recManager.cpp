/* -----------------------------------------------------------------------------
 *
 * Giada - Your Hardcore Loopmachine
 *
 * -----------------------------------------------------------------------------
 *
 * Copyright (C) 2010-2019 Giovanni A. Zuliani | Monocasual
 *
 * This file is part of Giada - Your Hardcore Loopmachine.
 *
 * Giada - Your Hardcore Loopmachine is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * Giada - Your Hardcore Loopmachine is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Giada - Your Hardcore Loopmachine. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * -------------------------------------------------------------------------- */


#include "gui/dispatcher.h"
#include "core/channels/channel.h"
#include "core/model/model.h"
#include "core/types.h"
#include "core/clock.h"
#include "core/kernelAudio.h"
#include "core/conf.h"
#include "core/mixer.h"
#include "core/mixerHandler.h"
#include "core/midiDispatcher.h"
#include "core/recorder.h"
#include "core/recorderHandler.h"
#include "core/recManager.h"


namespace giada {
namespace m {
namespace recManager
{
namespace
{
void setRecordingAction_(bool v)
{
	model::onSwap(model::recorder, [&](model::Recorder& r)
	{
		r.isRecordingAction = v;
	});
}


void setRecordingInput_(bool v)
{
	model::onSwap(model::recorder, [&](model::Recorder& r)
	{
		r.isRecordingInput = v;
	});
}


/* -------------------------------------------------------------------------- */


bool startActionRec_()
{
	if (!kernelAudio::isReady())
		return false;
	clock::setStatus(ClockStatus::RUNNING);
	m::mh::startSequencer();
	setRecordingAction_(true);
	return true;
}


/* -------------------------------------------------------------------------- */


bool startInputRec_()
{
	if (!kernelAudio::isReady() || !mh::hasRecordableSampleChannels())
		return false;
	mixer::startInputRec();
	mh::startSequencer();
	setRecordingInput_(true);
	return true;
}
} // {anonymous}


/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */


void init()
{
}


/* -------------------------------------------------------------------------- */


bool isRecording()
{ 
	return isRecordingAction() || isRecordingInput();
}


bool isRecordingAction()
{ 
	model::RecorderLock lock(model::recorder); 

	bool isRecording = model::recorder.get()->isRecordingAction;
	bool isWaiting   = clock::getStatus() == ClockStatus::WAITING;
	
	return isRecording || (!isRecording && isWaiting); 
}


bool isRecordingInput()
{ 
	model::RecorderLock lock(model::recorder); 
	return model::recorder.get()->isRecordingInput; 
}


/* -------------------------------------------------------------------------- */


void startActionRec(RecTriggerMode mode)
{
	if (mode == RecTriggerMode::NORMAL)
		startActionRec_();
	else
	if (mode == RecTriggerMode::SIGNAL) {
		clock::setStatus(ClockStatus::WAITING);
		clock::rewind();
		m::midiDispatcher::setSignalCallback(startActionRec_);
		v::dispatcher::setSignalCallback(startActionRec_);
	}
}


/* -------------------------------------------------------------------------- */


void stopActionRec()
{
	setRecordingAction_(false);

	/* If you stop the Action Recorder in SIGNAL mode before any actual 
	recording: just clean up everything and return. */

	if (clock::getStatus() == ClockStatus::WAITING)	{
		clock::setStatus(ClockStatus::STOPPED);
		m::midiDispatcher::setSignalCallback(nullptr);
		v::dispatcher::setSignalCallback(nullptr);
		return;
	}

	std::unordered_set<ID> channels = recorderHandler::consolidate();

	/* Enable reading actions for Channels that have just been filled with 
	actions. Start reading right away, without checking whether 
	conf::treatRecsAsLoops is enabled or not. */

	for (ID id : channels)
		m::model::onGet(m::model::channels, id, [](Channel& c)
		{
			c.startReadingActions(/*treatRecsAsLoops=*/false, /*recsStopOnChanHalt=*/false);
		});
}


/* -------------------------------------------------------------------------- */


void toggleActionRec(RecTriggerMode m)
{
	isRecordingAction() ? stopActionRec() : startActionRec(m);
}


/* -------------------------------------------------------------------------- */


bool startInputRec(RecTriggerMode mode)
{
	if (mode == RecTriggerMode::NORMAL)
		startInputRec_();
	else
	if (mode == RecTriggerMode::SIGNAL) {
		if (!mh::hasRecordableSampleChannels())
			return false;
		clock::setStatus(ClockStatus::WAITING);
		clock::rewind();
		mixer::setSignalCallback(startInputRec_);
		setRecordingInput_(true);
	}
	return isRecordingInput();
}


/* -------------------------------------------------------------------------- */


void stopInputRec()
{
	setRecordingInput_(false);

	mixer::stopInputRec();
	
	/* If you stop the Input Recorder in SIGNAL mode before any actual 
	recording: just clean up everything and return. */

	if (clock::getStatus() == ClockStatus::WAITING) {
		clock::setStatus(ClockStatus::STOPPED);
		mixer::setSignalCallback(nullptr);
	}
	else
		mh::finalizeInputRec();
}


/* -------------------------------------------------------------------------- */


bool toggleInputRec(RecTriggerMode m)
{
	if (isRecordingInput()) {
		stopInputRec();
		return true;
	}
	return startInputRec(m);
}
}}} // giada::m::recManager
