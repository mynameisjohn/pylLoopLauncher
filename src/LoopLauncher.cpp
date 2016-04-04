#include "LoopLauncher.h"

#include <algorithm>

#include <pyliason.h>

using Track = LoopLauncher::Track;

// Default constructor sets all pending tracks null
Track::Track() :
	m_nFadeSamples( 0 ),
	m_nSampleCount( 0 ),
	m_pPendingTrack( nullptr ),
	m_pPendingClip( nullptr )
{
}

// Construct with a list of clips (audio files)
// you'd like to associate with this track
Track::Track( std::list<std::string> liFileNames ) :
	Track()
{
	for ( auto& file : liFileNames )
		AddClip( file );
}

// && constructor / operator=
Track::Track( Track&& other ) :
	m_nFadeSamples( other.m_nFadeSamples ),
	m_nSampleCount( other.m_nSampleCount ),
	m_pPendingTrack( other.m_pPendingTrack ),
	m_pPendingClip( other.m_pPendingClip ),
	m_mapClips( std::move( other.m_mapClips ) )
{
}

Track& Track::operator=( Track&& other )
{
	m_nFadeSamples = other.m_nFadeSamples;
	m_nSampleCount = other.m_nSampleCount;
	m_pPendingTrack = other.m_pPendingTrack;
	m_pPendingClip = other.m_pPendingClip;
	m_mapClips = std::move( other.m_mapClips );

	return *this;
}

// The active assumption is that these are the same for all clips
int Track::GetChannelCount() const
{
	return m_mapClips.empty() ? 1 : m_mapClips.begin()->second.getChannelCount();
}

int Track::GetSampleRate() const
{
	return m_mapClips.empty() ? 1 : m_mapClips.begin()->second.getSampleRate();
}

int Track::GetSampleCount() const
{
	return m_mapClips.empty() ? 1 : (int)m_mapClips.begin()->second.getSampleCount();
}

// Use SFML to load audio files
bool Track::AddClip( std::string fileName )
{
	sf::SoundBuffer sBuf;
	if ( sBuf.loadFromFile( fileName ) )
	{
		// Initialize this if it hasn't been set
		// (again assumming that all files have same sample count)
		if ( m_nSampleCount == 0 )
			m_nSampleCount = (int)sBuf.getSampleCount();

		// Set the fade duration - this is a work in progress
		if ( m_nFadeSamples == 0 )
		{
			const float mS = 5.f;
			const float samplesPerMS = sBuf.getSampleRate() / 1000.f;
			m_nFadeSamples = (int) (mS * samplesPerMS);
		}

		// Move the sound buffer into our map
		m_mapClips[fileName] = std::move( sBuf );

		return true;
	}

	return false;
}

// This is invoked via LoopLauncher::postPendingTracks
// which gets called from the audio thread when it detects
// an imminent loop boundary
bool Track::SetPendingTrack( std::string trackName )
{
	auto it = m_mapClips.find( trackName );
	if ( m_mapClips.end() != it )
	{
		m_pPendingClip = &it->second;

		return true;
	}
	else
		m_pPendingClip = nullptr;

	return false;
}

// This gets called from the audio thread and fills the mix buffer
// with nSamplesDesired samples, finding the current sample position within
// the track audio given the global sample pos (nCurSamplePos)
bool Track::GetAudio( sf::Int16 * pMixBuffer, int nSamplesDesired, int nCurSamplePos )
{
	// Volume... a work in progress
	const float vol_1 = 1.f / 2;

	// Pointer check
	if ( pMixBuffer == nullptr )
		return false;

	// If the active clip is null but the pending clip is not
	// assign the active clip to the pending clip, 
	// leaving pending clip as is
	if ( m_pPendingTrack == nullptr && m_pPendingClip != nullptr )
		m_pPendingTrack = m_pPendingClip;
	// If both are null,  return false (silence)
	else if ( m_pPendingTrack == nullptr )
		return false;

	// Create a ref to the active clip
	sf::SoundBuffer& soundBuf = *m_pPendingTrack;
	
	// find the sample offset within the track audio and get its address
	const int sampleOffset = nCurSamplePos % soundBuf.getSampleCount();
	const sf::Int16 * pSoundBuf = &soundBuf.getSamples()[sampleOffset];

	// Determine if we're going to be looping to the pending clip and compute
	// the last sample index we'll be copying
	bool bLoop = (sampleOffset + nSamplesDesired >= soundBuf.getSampleCount());
	int lastSample = bLoop ? nSamplesDesired - m_nFadeSamples : nSamplesDesired;

	// Add values from sound buf to mix buf, scaling by volume
	for ( int i = 0; i < lastSample; i++ )
	{
		pMixBuffer[i] += *pSoundBuf++ * vol_1;
	}

	// If we're looping over
	if ( bLoop )
	{
		// See if a pending track was set, otherwise fade to silence
		// The pending track is set via ::setPendingTrack, which is called by
		// the LoopLauncher in LoopLauncher::updatePendingTracks at the end of 
		// its longest loop cycle in onGetData, so I guess this is thread safe
		sf::Int16 nextSample( 0 );
		if ( m_pPendingClip )
			nextSample = *(m_pPendingClip->getSamples());
		
		// Crossfade the current buffer with the first sample of the pending buffer
		// This is a work in progress, it still pops a bit
		for ( int i = lastSample, j = 0; i < nSamplesDesired; i++ )
		{
			float a = 1.f - float( ++j ) / float( m_nFadeSamples );
			sf::Int16 val = *pSoundBuf++ * vol_1;
			sf::Int16 iVal = (sf::Int16)ceilf( a * val + (1.f - a) * nextSample );
			pMixBuffer[i] += iVal;
		}

		// Assign the active clip to the pending clip, leaving pending clip as is
		m_pPendingTrack = m_pPendingClip;
	}

	return true;
}

// Returns true of the clip name exists in the map
bool Track::HasClip( std::string clipName ) const
{
	return (m_mapClips.find( clipName ) != m_mapClips.end());
}

// Set needsAudio to true (?)
LoopLauncher::LoopLauncher() :
	sf::SoundStream(),
	m_nLastSamplePos( 0 ),
	m_nMaxSampleCount( 0 ),
	m_bNeedsAudio( true )
{
}

// Because these own Tracks, which own sf::SoundBuffers,
// we need the && constructor and operator=
LoopLauncher::LoopLauncher( LoopLauncher&& other ) :
	m_nLastSamplePos( other.m_nLastSamplePos ),
	m_nMaxSampleCount( other.m_nMaxSampleCount ),
	m_mapTracks( std::move( other.m_mapTracks ) ),
	m_vMixBuffer( other.m_vMixBuffer ),
	m_bNeedsAudio( other.m_bNeedsAudio )
{
}

LoopLauncher& LoopLauncher::operator=( LoopLauncher&& other )
{
	m_nLastSamplePos = other.m_nLastSamplePos;
	m_nMaxSampleCount = other.m_nMaxSampleCount;
	m_mapTracks = std::move( other.m_mapTracks );
	m_vMixBuffer = other.m_vMixBuffer;
	m_bNeedsAudio = other.m_bNeedsAudio;

	return *this;
}

// This invokes sf::SoundStream::initialize, but not before setting the track map
// This was done for python, it should be optional
bool LoopLauncher::Initialize( std::map<std::string, std::list<std::string>> mapTracks )
{
	// Construct tracks given the input (dangerous)
	for ( auto& it : mapTracks )
		m_mapTracks.emplace( it.first, it.second );

	// If we still have no tracks, get out
	if ( m_mapTracks.empty() )
		return false;

	// Find the min and max sample count
	int nMinSampleCount = INT_MAX;
	for ( auto& track : m_mapTracks )
	{
		m_nMaxSampleCount = std::max( m_nMaxSampleCount, track.second.GetSampleCount() );
		nMinSampleCount = std::min( nMinSampleCount, track.second.GetSampleCount() );
	}

	// Each sf::SoundStream::onGetData call pushes 1/64th of the 
	// smallest clip onto the buffer... for no real reason
	if ( m_mapTracks.empty() == false )
		m_vMixBuffer.resize( nMinSampleCount / 64 );

	// Assuming these are all the same...
	// initialize with channel count and sample rate
	Track& t = m_mapTracks.begin()->second;
	initialize( t.GetChannelCount(), t.GetSampleRate() );

	return true;
}

// Return a pointer to an existing track by name, if it exists
Track * LoopLauncher::GetTrack( std::string trackName ) const
{
	auto it = m_mapTracks.find( trackName );
	if ( it == m_mapTracks.end() )
		return nullptr;

	return (Track *) &it->second;
}

// Construct a track given the name and clip list
bool LoopLauncher::AddTrack( std::string trackName, std::list<std::string> liFileNames )
{
	bool ret = true;
	
	m_mapTracks.emplace( trackName, liFileNames );

	return ret;
}

// Flush pending clips and invoke sf::SoundStream::play
void LoopLauncher::Play()
{
	postPendingTracks();
	sf::SoundStream::play();
}

// Called from main thread, sets the loop launcher's pending clips 
// which, when needed, will be posted to the audio thread and played
bool LoopLauncher::UpdatePendingClips( std::list<std::string> liNewActiveClips )
{
	std::lock_guard<std::mutex> lg( m_muTrackUpdate );

	// It's unfortunate that I have to do an O(n**2) search
	// here; in the python world I could cache pointers to Track
	// objects via GetTrack and set them directly, but this was simpler
	for ( auto& clip : liNewActiveClips )
	{
		for ( auto& itTrack : m_mapTracks )
		{
			if ( itTrack.second.HasClip( clip ) )
			{
				// Set the entry in the map
				m_mapPendingTracks[itTrack.first] = clip;

				// We no longer need audio
				m_bNeedsAudio = false;

				break;
			}
		}
	}

	// Returns true if we actually set a pending clip
	return (m_bNeedsAudio == false);
}

// Thread safe access to m_bNeedsAudio
bool LoopLauncher::NeedsAudio()
{
	std::lock_guard<std::mutex> lg( m_muTrackUpdate );

	return m_bNeedsAudio;
}

// LoopLauncher::postPendingTracks is called from the audio thread
void LoopLauncher::postPendingTracks()
{
	std::lock_guard<std::mutex> lg( m_muTrackUpdate );

	// I don't like doing this, but it clears out
	// any clips that won't be playing next
	for ( auto& track : m_mapTracks )
		track.second.SetPendingTrack( "silence" );

	// Call SetPendingTrack on each of the tracks
	for ( auto& itTrack : m_mapPendingTracks )
	{
		auto it = m_mapTracks.find( itTrack.first );
		if ( it != m_mapTracks.end() )
		{
			it->second.SetPendingTrack( itTrack.second );
		}
	}

	// Clear out any pending tracks
	m_mapPendingTracks.clear();

	// We now need audio
	m_bNeedsAudio = true;	
}

// LoopLauncher::onGetData is called from the audio thread
bool LoopLauncher::onGetData( sf::SoundStream::Chunk& c )
{
	// This shouldn't happen
	if ( m_vMixBuffer.empty() )
		return false;

	// Zero out the buffer
	m_vMixBuffer.assign( m_vMixBuffer.size(), 0 );

	// Assign the chunk values now
	c.sampleCount = m_vMixBuffer.size();
	c.samples = m_vMixBuffer.data();

	// If we're going to be looping, post pending tracks
	bool bLoop = c.sampleCount + m_nLastSamplePos >= m_nMaxSampleCount;
	if ( bLoop )
	{
		postPendingTracks();
	}

	// Ask each track to add its audio to the mix buffer
	for ( auto& track : m_mapTracks )
		track.second.GetAudio( m_vMixBuffer.data(), m_vMixBuffer.size(), m_nLastSamplePos );

	// Incremement sample pos by the size of the mix buffer
	m_nLastSamplePos += m_vMixBuffer.size();

	// Set to zero if we're going over m_nMaxSampleCount
	if ( m_nLastSamplePos >= m_nMaxSampleCount )
		m_nLastSamplePos = 0;

	// For me this always returns true
	return true;
}

// Not sure what this should do yet
void LoopLauncher::onSeek( sf::Time t )
{
	// NYI
}

// Initialize all the functions I'd like to be able to call from python
/*static*/ bool LoopLauncher::PylInit()
{
	using namespace pyl;

	ModuleDef * pLLModDef = ModuleDef::CreateModuleDef<struct stLoopLauncherModule>( "pylLoopLauncher" );
	if ( pLLModDef == nullptr )
		return false;

	pLLModDef->RegisterClass<LoopLauncher>( "LoopLauncher" );
	pLLModDef->RegisterClass<Track>( "Track" );

	// Really need to make that macro....
	{
		std::function<bool(LoopLauncher *, std::map<std::string, std::list<std::string>> )> fnLLInitialize = &LoopLauncher::Initialize;
		pLLModDef->RegisterMemFunction<LoopLauncher, struct st_fnLLInitialize>( "Initialize", fnLLInitialize );
	}
	{
		std::function<void( LoopLauncher * )> fnLoopLauncher_play = &LoopLauncher::Play;
		pLLModDef->RegisterMemFunction<LoopLauncher, struct fnLoopLauncher_st_play>( "Play", fnLoopLauncher_play, "Start or resume playing the audio stream. " );
	}
	{
		std::function<Track *(LoopLauncher *, std::string)> fnLLGetTrack = &LoopLauncher::GetTrack;
		pLLModDef->RegisterMemFunction<LoopLauncher, struct st_fnLLGetTrack>( "GetTrack", fnLLGetTrack );
	}
	{
		std::function<bool( LoopLauncher *, std::string, std::list<std::string> )> fnLLAddTrack = &LoopLauncher::AddTrack;
		pLLModDef->RegisterMemFunction<LoopLauncher, struct st_fnLLAddTrack>( "AddTrack", fnLLAddTrack );
	}
	{
		std::function<bool( LoopLauncher * )> fnLLNeedsAudio = &LoopLauncher::NeedsAudio;
		pLLModDef->RegisterMemFunction<LoopLauncher, struct st_fnLLNeedsAudio>( "NeedsAudio", fnLLNeedsAudio );
	}
	{
		std::function<bool( LoopLauncher *, std::list<std::string> liNewActiveClips )> fnLLUpdatePendingClips = &LoopLauncher::UpdatePendingClips;
		pLLModDef->RegisterMemFunction<LoopLauncher, struct st_fnUpdatePendingClips>( "UpdatePendingClips", fnLLUpdatePendingClips );
	}
	{
		std::function<bool( Track *, std::string )> fnTAddClip = &Track::AddClip;
		pLLModDef->RegisterMemFunction<Track, struct st_fnTAddClip>( "AddClip", fnTAddClip );
	}
	{
		std::function<bool( Track *, std::string )> fnTSetPendingTrack = &Track::SetPendingTrack;
		pLLModDef->RegisterMemFunction<Track, struct st_fnTSetPendingTrack>( "SetPendingTrack", fnTSetPendingTrack );
	}

	// These are all the sf::SoundStream functions I'd like to be able to call from python
	// I don't expose sf::SoundStream::play because I gave LoopLauncher its own ::Play function
	std::function<void( LoopLauncher * )> fnLoopLauncher_pause = &sf::SoundStream::pause;
	pLLModDef->RegisterMemFunction<LoopLauncher, struct fnLoopLauncher_st_pause>( "Pause", fnLoopLauncher_pause, "Pause the audio stream. " );

	std::function<void( LoopLauncher * )> fnLoopLauncher_stop = &sf::SoundStream::stop;
	pLLModDef->RegisterMemFunction<LoopLauncher, struct fnLoopLauncher_st_stop>( "Stop", fnLoopLauncher_stop, "Stop playing the audio stream. " );

	std::function<unsigned int( LoopLauncher * )> fnLoopLauncher_getChannelCount = &sf::SoundStream::getChannelCount;
	pLLModDef->RegisterMemFunction<LoopLauncher, struct fnLoopLauncher_st_getChannelCount>( "GetChannelCount", fnLoopLauncher_getChannelCount, "Return the number of channels of the stream. " );

	std::function<unsigned int( LoopLauncher * )> fnLoopLauncher_getSampleRate = &sf::SoundStream::getSampleRate;
	pLLModDef->RegisterMemFunction<LoopLauncher, struct fnLoopLauncher_st_getSampleRate>( "GetSampleRate", fnLoopLauncher_getSampleRate, "Get the stream sample rate of the stream. " );

	std::function<bool( LoopLauncher * )> fnLoopLauncher_getLoop = &sf::SoundStream::getLoop;
	pLLModDef->RegisterMemFunction<LoopLauncher, struct fnLoopLauncher_st_getLoop>( "GetLoop", fnLoopLauncher_getLoop, "Tell whether or not the stream is in loop mode. " );

	std::function<void( LoopLauncher *, bool )> fnLoopLauncher_setLoop = &sf::SoundStream::setLoop;
	pLLModDef->RegisterMemFunction<LoopLauncher, struct fnLoopLauncher_st_setLoop>( "SetLoop", fnLoopLauncher_setLoop, "Set whether or not the stream should loop after reaching the end. " );

	std::function<float( LoopLauncher * )> fnLoopLauncher_getVolume = &sf::SoundStream::getVolume;
	pLLModDef->RegisterMemFunction<LoopLauncher, struct fnLoopLauncher_st_getVolume>( "GetVolume", fnLoopLauncher_getVolume, "Get the volume of the sound. " );

	std::function<void( LoopLauncher *, float )> fnLoopLauncher_setVolume = &sf::SoundStream::setVolume;
	pLLModDef->RegisterMemFunction<LoopLauncher, struct fnLoopLauncher_st_setVolume>( "SetVolume", fnLoopLauncher_setVolume, "Set the volume of the sound. " );

	return true;
}