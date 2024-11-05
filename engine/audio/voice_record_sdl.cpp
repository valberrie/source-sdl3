//========= Copyright 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//
//=============================================================================//
// This module implements the voice record and compression functions

//#include "audio_pch.h"
//#include "voice.h"
#include "tier0/platform.h"
#include "ivoicerecord.h"
#include "tier0/dbg.h"
#include "tier0/threadtools.h"

#include <assert.h>
#include "SDL3/SDL_audio.h"

#define RECORDING_BUFFER_SECONDS 3
#define SAMPLE_COUNT 2048

// ------------------------------------------------------------------------------
// VoiceRecord_SDL
// ------------------------------------------------------------------------------

struct AudioBuf
{
	int Read(char *out, int len)
	{
		int nAvalible = (size + (writePtr - readPtr)) % size;

		if( nAvalible == 0 )
			return 0;

		if( len > nAvalible ) len = nAvalible;

		int diff = (data + size) - readPtr;


		if( len > diff )
		{
			memcpy(out, readPtr, diff );
			memcpy(out+diff, data, len-diff );
		} else memcpy(out, readPtr, len);

		readPtr += len;

		if( readPtr >= data + size )
			readPtr -= size;

		return len;
	}

	void Write(char *in, int len)
	{
		int diff = (data + size) - writePtr;

		if( len > diff )
		{
			memcpy(writePtr, in, diff );
			memcpy(data, in+diff, len-diff );
		} else memcpy(writePtr, in, len);

		writePtr += len;

		if (writePtr >= (data + size))
			writePtr -= size;
	}

	int size;
	char *data;
	char *readPtr;
	char *writePtr;
};

class VoiceRecord_SDL : public IVoiceRecord
{
protected:
	virtual				~VoiceRecord_SDL();
public:
	VoiceRecord_SDL();
	virtual void		Release();
	virtual bool		RecordStart();
	virtual void		RecordStop();

	// Initialize. The format of the data we expect from the provider is
	// 8-bit signed mono at the specified sample rate.
	virtual bool		Init(int sampleRate);
	virtual void		Idle() {}; // Stub
	void RenderBuffer( char *pszBuf, int size );

	// Get the most recent N samples.
	virtual int			GetRecordedData(short *pOut, int nSamplesWanted );

	SDL_AudioSpec m_ReceivedRecordingSpec;
	int m_BytesPerSample; // Да кому нужна эта ваша инкапсуляция?
	int m_nSampleRate;
private:
	bool				InitalizeInterfaces();	// Initialize the openal capture buffers and other interfaces
	void				ReleaseInterfaces();	// Release openal buffers and other interfaces
	void				ClearInterfaces();				// Clear members.
private:

	SDL_AudioDeviceID m_Device;
	AudioBuf m_AudioBuffer;
};

void audioRecordingCallback( void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount )
{
    VoiceRecord_SDL *voice = (VoiceRecord_SDL*)userdata;
    if (additional_amount > 0) {
        Uint8 *data = SDL_stack_alloc(Uint8, additional_amount);
        if (data) {
            voice->RenderBuffer( (char*)data, additional_amount );
            SDL_GetAudioStreamData(stream, data, additional_amount);
            SDL_stack_free(data);
        }
    }
}

VoiceRecord_SDL::VoiceRecord_SDL() :
m_nSampleRate( 0 ) ,m_Device( 0 )
{
	m_AudioBuffer.data = NULL;
	m_AudioBuffer.readPtr = NULL;
	m_AudioBuffer.writePtr = NULL;

	ClearInterfaces();
}

VoiceRecord_SDL::~VoiceRecord_SDL()
{
	ReleaseInterfaces();
	ClearInterfaces();
}

void VoiceRecord_SDL::Release()
{
	ReleaseInterfaces();
	ClearInterfaces();
	delete this;
}

bool VoiceRecord_SDL::RecordStart()
{
	if ( !m_Device )
		InitalizeInterfaces();

	if ( !m_Device )
		return false;

	SDL_ResumeAudioDevice( m_Device);

	return true;
}


void VoiceRecord_SDL::RecordStop()
{
	// Stop capturing.
	if ( m_Device )
		SDL_PauseAudioDevice( m_Device );

	// Release the capture buffer interface and any other resources that are no
	// longer needed
	ReleaseInterfaces();
}

bool VoiceRecord_SDL::InitalizeInterfaces()
{
    /*
	//Default audio spec
	SDL_AudioSpec desiredRecordingSpec;
	SDL_zero(desiredRecordingSpec);
	desiredRecordingSpec.freq = m_nSampleRate;
	desiredRecordingSpec.format = AUDIO_S16;
	desiredRecordingSpec.channels = 1;
	desiredRecordingSpec.samples = SAMPLE_COUNT;
	desiredRecordingSpec.callback = audioRecordingCallback;

	//Open recording device
	m_Device = SDL_OpenAudioDevice( NULL, SDL_TRUE, &desiredRecordingSpec, &m_ReceivedRecordingSpec, 0 );
	*/
	void* userdata = (void*)this;

	const SDL_AudioSpec spec = { SDL_AUDIO_S16, 1, SAMPLE_COUNT };
    SDL_AudioStream *stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audioRecordingCallback, userdata);
    m_Device = SDL_GetAudioStreamDevice(stream);

	if( m_Device != 0 )
	{
		//Calculate per sample bytes
		m_BytesPerSample = m_ReceivedRecordingSpec.channels * ( SDL_AUDIO_BITSIZE( m_ReceivedRecordingSpec.format ) / 8 );

		//Calculate bytes per second
		int bytesPerSecond = m_ReceivedRecordingSpec.freq * m_BytesPerSample;

		//Allocate and initialize byte buffer
		m_AudioBuffer.size = RECORDING_BUFFER_SECONDS * bytesPerSecond;

		if( !m_AudioBuffer.data )
			m_AudioBuffer.data = (char *)malloc( m_AudioBuffer.size );

		m_AudioBuffer.readPtr = m_AudioBuffer.data;
		m_AudioBuffer.writePtr = m_AudioBuffer.data + SAMPLE_COUNT*m_BytesPerSample*2;

		memset( m_AudioBuffer.data, 0, m_AudioBuffer.size );

		return true;
	}
	else
		return false;
}

bool VoiceRecord_SDL::Init(int sampleRate)
{
	m_nSampleRate = sampleRate;
	ReleaseInterfaces();

	return true;
}


void VoiceRecord_SDL::ReleaseInterfaces()
{
	if( m_Device != 0 )
		SDL_CloseAudioDevice( m_Device );

	m_Device = 0;
}


void VoiceRecord_SDL::ClearInterfaces()
{
	if( m_AudioBuffer.data )
	{
		free( m_AudioBuffer.data );
		m_AudioBuffer.data = NULL;
		m_AudioBuffer.readPtr = NULL;
		m_AudioBuffer.writePtr = NULL;
	}
	m_Device = 0;
}

void VoiceRecord_SDL::RenderBuffer( char *pszBuf, int size )
{
	m_AudioBuffer.Write( pszBuf, size );
}

int VoiceRecord_SDL::GetRecordedData(short *pOut, int nSamples )
{
	if ( !m_AudioBuffer.data || nSamples == 0 )
		return 0;

	int cbSamples = nSamples * m_BytesPerSample;

	return m_AudioBuffer.Read( (char*)pOut, cbSamples )/m_BytesPerSample;
}

IVoiceRecord* CreateVoiceRecord_SDL(int sampleRate)
{
	VoiceRecord_SDL *pRecord = new VoiceRecord_SDL;
	if ( pRecord && pRecord->Init(sampleRate) )
		return pRecord;
	else if( pRecord )
		pRecord->Release();

	return NULL;
}
