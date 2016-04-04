#pragma once

// We override sf::SoundStream
#include <SFML/Audio/SoundStream.hpp>
#include <SFML/Audio/SoundBuffer.hpp>

// Each track has an atomic "activeTrack" pointer
#include <mutex>

#include <list>
#include <vector>
#include <map>
#include <string>

// LoopLauncher
// Our sf::SoundStream override class that pushes buffers
// of audio for SFML to play. 
// The LoopLauncher owns a container of tracks; on every getData
// call each track is given the chance to push its audio onto the
// buffer. It's up to the track to handle things like loop crossfade.
class LoopLauncher : public sf::SoundStream
{
public:
	// LoopLauncher::Track
	// Think of these like loops slots; each track owns a set of
	// audio clips and designates an active track (the one that
	// gets pushed onto SFML's audio buffer.) A pending track can
	// also be set; if the pending track is set, the track crossfades
	// the end and beginning of buffers that cross the clip transition
	// so as to avoid the pop associated with audio loops
	class Track
	{
	public:
		// I may make a class out of this at some point
		using Clip = sf::SoundBuffer;

		// I need the rvalue functions because sf::SoundBuffer
		// deletes its copy constructor and assignment operator
		Track();
		Track( std::list<std::string> liFileNames );
		Track( Track&& );
		Track& operator=( Track&& );
		
		// Some useful gets
		int GetChannelCount() const;
		int GetSampleRate() const;
		int GetSampleCount() const;
		bool HasClip( std::string clipName ) const;
	
		// Add a track to the map of clips
		bool AddClip( std::string fileName );

		// Set the pending track (atomically)
		bool SetPendingTrack( std::string trackName );

		// Write nSamplesDesired samples into pMixBuffer, given nCurSamplePos
		bool GetAudio( sf::Int16 * pMixBuffer, int nSamplesDesired, int nCurSamplePos );

	private:
		int m_nFadeSamples;
		int m_nSampleCount;
		std::map<std::string, Clip> m_mapClips;
		Clip * m_pPendingTrack;
		Clip * m_pPendingClip;
	};

public:

	// I need these for the same reason Track does
	LoopLauncher();
	LoopLauncher( LoopLauncher&& );
	LoopLauncher& operator=( LoopLauncher&& );

	// Call into the sf::SoundStream::initialize function
	bool Initialize( std::map<std::string, std::list<std::string>> mapTracks );

	bool NeedsAudio();

	LoopLauncher::Track * GetTrack( std::string trackName ) const;

	bool AddTrack( std::string trackName, std::list<std::string> liFileNames );

	//bool UpdatePendingTracks( std::map<std::string, std::string> mapNewActiveClips, bool bPost = false );
	bool UpdatePendingClips( std::list<std::string> liNewActiveClips );

	// This calls teh sf::SoundStream::play function
	// after flushing any pending clips
	void Play();

	// sf::SoundStream overrides
protected:
	bool onGetData( sf::SoundStream::Chunk& ) override;
	void onSeek( sf::Time ) override;

private:
	int m_nLastSamplePos;
	int m_nMaxSampleCount;
	std::map<std::string, LoopLauncher::Track> m_mapTracks;
	std::vector<sf::Int16> m_vMixBuffer;

	// These are the things shared between the audio thread and others
	// Protected by the mutex, the needsAudio bool gets set after the 
	// full loop cycle (meaning the longest track) repeats, meaning that
	// is the "trigger resolution" of loops. 
	// The map is used to store pending tracks, which get updated in the 
	// tracks while the mutex is locked
	std::mutex m_muTrackUpdate;
	bool m_bNeedsAudio;
	std::map<std::string, std::string> m_mapPendingTracks;
	void postPendingTracks();

	// PyLiaison static init func
public:
	static bool PylInit();
};

