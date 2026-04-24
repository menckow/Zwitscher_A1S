#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <vector>
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/CoreAudio/VolumeStream.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"

enum class PlaybackState {
    INITIALIZING,
    IDLE,
    PLAYING_INTRO,
    PLAYING_RANDOM,
    STANDBY
};

class AudioEngine {
private:
    PlaybackState currentState;
    unsigned long playbackStartTime;
    unsigned long lastPirActivityTime;

    std::vector<String> directoryList;
    std::vector<String> currentMp3Files;
    int currentDirectoryIndex;
    String introFileName;

    float currentVolume = 0.6; 

    void loadFilesFromCurrentDirectory();

public:
    AudioEngine();
    ~AudioEngine() = default;

    void init();
    void update(); 
    
    void findMp3Directories(File dir);
    void playIntro();
    void playRandomTrack();
    void stopPlayback();

    void onAudioEof();
    
    void checkPirAndTimeout();
    
    // Audio Kit Buttons
    void increaseVolume();
    void decreaseVolume();
    void nextDirectory();

    PlaybackState getState() const { return currentState; }
    void setState(PlaybackState state) { currentState = state; }

    unsigned long getLastPirActivityTime() const { return lastPirActivityTime; }
    void updatePirActivity() { lastPirActivityTime = millis(); }
};
