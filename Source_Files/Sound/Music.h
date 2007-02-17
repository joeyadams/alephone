#ifndef __MUSIC_H
#define __MUSIC_H

/*

	Copyright (C) 1991-2001 and beyond by Bungie Studios, Inc.
	and the "Aleph One" developers.
 
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	This license is contained in the file "COPYING",
	which is included with this source code; it is available online at
	http://www.gnu.org/licenses/gpl.html

	Handles both intro and level music

*/

#include "cseries.h"
#include "FileHandler.h"
#include <vector>

#ifdef HAVE_SDL_SOUND
#include "SDL_sound.h"
#endif

class Music
{
public:
	static Music *instance() { 
		if (!m_instance) m_instance = new Music(); return m_instance; 
	}

	bool SetupIntroMusic(FileSpecifier &File);
	void RestartIntroMusic();

	void Open(FileSpecifier *file);
	void FadeOut(short duration);
	void Close();
	void Pause();
	void Play();
	bool Playing();
	void Rewind();
	void Restart();
#ifdef __MACOS__
	bool InterruptFillBuffer();
#endif
	bool FillBuffer();

	void Idle();

	bool Initialized() { return music_initialized; }

	void PreloadLevelMusic();
	void LoadLevelMusic();
	void StopLevelMusic();

private:
	Music();
	bool Load(FileSpecifier &file);
	static Music *m_instance;

	static const int MUSIC_BUFFER_SIZE = 1024;

#ifdef HAVE_SDL_SOUND
	Sound_Sample* music_sample;
#else
	uint32 music_data_length;
	uint32 music_data_remaining;
	uint32 music_data_offset;
	std::vector<uint8> music_buffer;
#endif

	SDL_RWops* music_rw;

	// info about the music's format
	bool sixteen_bit;
	bool stereo;
	bool signed_8bit;
	int bytes_per_frame;
	_fixed rate;

	FileSpecifier music_file;
	FileSpecifier music_intro_file;

	bool music_initialized;
	bool music_play;
	bool music_prelevel;
	bool music_level;
	bool music_fading;
	uint32 music_fade_start;
	uint32 music_fade_duration;
	bool music_intro;

#ifdef __MACOS__
	bool macos_file_done;
	bool macos_read_more;
	uint32 macos_buffer_length;
	std::vector<uint8> macos_music_buffer;
#endif
};

#endif