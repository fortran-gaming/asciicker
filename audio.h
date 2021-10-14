#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

bool InitAudio();
void FreeAudio();

void CallAudio(const uint8_t* data, int size);

// is this right direction?
/*
void SetAudioSteps(int num_steps, int contact);
void SetAudioWingsFreq(int freq);
void SetAudioSwoosh(int speed, int contact);
void SetAudioJump(int contact, bool end);
void SetAudioShoot(int contact);
void SetAudioWater(int height, int speed);
*/

#endif