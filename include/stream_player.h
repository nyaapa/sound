#pragma once

#include <iostream>
#include <unistd.h>
#include <memory>
#include <fstream>
#include <string>
#include <deque>
#include <iterator>
#include <mutex>
#include "portaudio.h"

using namespace std;

int StreamPlayerPACallback(const void*, void*, unsigned long, const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void *);

class StreamPlayer {
    friend int StreamPlayerPACallback(const void*, void*, unsigned long, const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void *);
    deque<char> buffer;
    PaStreamParameters outputParameters;
    PaStream *stream;
    bool started = false;
    mutex bufferSync;
    bool finalized = false;

public:
    StreamPlayer(StreamPlayer&& ) = default;
    StreamPlayer& operator=(StreamPlayer&&) = default;
    explicit StreamPlayer() : stream(nullptr) {
        PaError error;

        /* set the output parameters */
        outputParameters.device = Pa_GetDefaultOutputDevice(); /* use the default device */
        outputParameters.channelCount = 2;//data->sfInfo.channels; /* use the same number of channels as our sound file */ TODO changels
        outputParameters.sampleFormat = paInt32;
        outputParameters.suggestedLatency = 0.2; /* 200 ms ought to satisfy even the worst sound card */
        outputParameters.hostApiSpecificStreamInfo = 0; /* no api specific data */

        /* try to open the output */
        error = Pa_OpenStream(
                &stream,  /* stream is a 'token' that we need to save for future portaudio calls */
                0,  /* no input */
                &outputParameters,
                44100,//sfInfo.samplerate,  /* use the same sample rate as the sound file */ TODO rate
                paFramesPerBufferUnspecified,  /* let portaudio choose the buffersize */
                paNoFlag,  /* no special modes (clip off, dither off) */
                StreamPlayerPACallback,
                this
        ); /* pass in our data structure so the callback knows what's up */

        /* if we can't open it, then bail out */
        if ( error ) {
            printf("error opening output, error code = %i\n", error);
            Pa_Terminate();
            return;
        }
    }

    void addData(char * data, size_t length) {
        {
            lock_guard<mutex> guard(bufferSync);
            std::copy(data, data + length, back_inserter(buffer));
        }
        if ( !started ) {
            started = true;
            Pa_StartStream(stream);
        }
    }

    void finalize() {
        finalized = true;
    }

    bool dead() {
        return finalized && buffer.empty();
    }

    ~StreamPlayer() {
        Pa_StopStream(stream);
    }
};

int StreamPlayerPACallback(const void *input, void *output,
                           unsigned long frameCount,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData)
{
    auto player = static_cast<StreamPlayer*>(userData);

    int thisSize = frameCount*2*sizeof(int); // TODO chanels
    /* are we going to read past the end of the file?*/
    int thisRead;
    auto out = static_cast<char *>(output);
    while ( thisSize > 0 ) {
        thisRead = thisSize;
        if (thisRead > player->buffer.size()) {
            thisRead = player->buffer.size();
        }

        if ( thisRead ) {
            /* since our output format and channel interleaving is the same as sf_readf_int's requirements */
            /* we'll just read straight into the output buffer */
            std::copy(player->buffer.begin(), player->buffer.begin() + thisRead, out);
            {
                lock_guard<mutex> guard(player->bufferSync);
                player->buffer.erase(player->buffer.begin(), player->buffer.begin() + thisRead);
            }
            thisSize -= thisRead;
            out += thisRead;
        } else if ( player->finalized ) {
            return paComplete;
        }
    }

    return paContinue;
}