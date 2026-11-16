// Copyright (C) 2019 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

#ifndef NATIVEMIDITESTBED_MIDISPEC_H

//
// MIDI Messages
//
// Channel Commands
static const uint8_t kMIDIChanCmd_NoteOff = 8;
static const uint8_t kMIDIChanCmd_NoteOn = 9;
static const uint8_t kMIDIChanCmd_PolyPress = 10;
static const uint8_t kMIDIChanCmd_Control = 11;
static const uint8_t kMIDIChanCmd_ProgramChange = 12;
static const uint8_t kMIDIChanCmd_ChannelPress = 13;
static const uint8_t kMIDIChanCmd_PitchWheel = 14;
// System Commands
static const uint8_t kMIDISysCmdChan = 0xF0;
static const uint8_t kMIDISysCmd_SysEx = 0xF0;
static const uint8_t kMIDISysCmd_EndOfSysEx = 0xF7;
static const uint8_t kMIDISysCmd_ActiveSensing = 0xFE;
static const uint8_t kMIDISysCmd_Reset = 0xFF;

#define NATIVEMIDITESTBED_MIDISPEC_H
#endif
