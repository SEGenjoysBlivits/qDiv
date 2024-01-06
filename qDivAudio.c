#include<stdint.h>
#include<stdbool.h>
#include<string.h>
#define MINIAUDIO_IMPLEMENTATION
#include"include/miniaudio.h"
#define QDIV_AUDIO_DECODERS 64
#define QDIV_AUDIO_FORMAT ma_format_f32
#define QDIV_AUDIO_CHANNELS 1
#define QDIV_AUDIO_RATE 48000
#define QDIV_AUDIO_BUFFER_SIZE 4096

static ma_decoder_config audioDecoderConfig;
static ma_decoder audioDecoder[QDIV_AUDIO_DECODERS];
static ma_device_config audioDeviceConfig;
static ma_device audioDevice;
static bool audioTask[QDIV_AUDIO_DECODERS] = {false};

int32_t clampInt(int32_t valMin, int32_t valIQ, int32_t valMax);

// Callback
static void callback_audio(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
	int32_t decoderSL = 0;
	float* outputFL = (float*)pOutput;
	float outputTMP[QDIV_AUDIO_BUFFER_SIZE];
	while(decoderSL < QDIV_AUDIO_DECODERS) {
		if(audioTask[decoderSL]) {
			memset(outputTMP, 0x00, sizeof(outputTMP));
			ma_uint64 frameRD;
			ma_uint32 frameSL;
			ma_uint32 frameDC = 0;
			while(frameDC < frameCount) {
				ma_uint32 frameTR = clampInt(0, frameCount - frameDC, ma_countof(outputTMP));
				if(ma_decoder_read_pcm_frames(audioDecoder + decoderSL, outputTMP, frameTR, &frameRD) != MA_SUCCESS || frameRD == 0) break;
				frameSL = 0;
				while(frameSL < frameRD) {
					outputFL[frameDC + frameSL] += outputTMP[frameSL];
					frameSL++;
				}
				frameDC += frameRD;
			}
			if(frameDC < frameCount) audioTask[decoderSL] = false;
    	}
    	decoderSL++;
    }
}

void playSound(const uint8_t* file, size_t fileSZ) {
	int32_t decoderSL = 0;
	while(decoderSL < QDIV_AUDIO_DECODERS) {
		if(!audioTask[decoderSL]) {
			ma_decoder_init_memory((const void*)file, fileSZ, &audioDecoderConfig, audioDecoder + decoderSL);
			audioTask[decoderSL] = true;
			break;
		}
		decoderSL++;
	}
}

void qDivAudioInit() {
	audioDecoderConfig = ma_decoder_config_init(QDIV_AUDIO_FORMAT, QDIV_AUDIO_CHANNELS, QDIV_AUDIO_RATE);
	audioDeviceConfig = ma_device_config_init(ma_device_type_playback);
    audioDeviceConfig.playback.format = QDIV_AUDIO_FORMAT;
    audioDeviceConfig.playback.channels = QDIV_AUDIO_CHANNELS;
    audioDeviceConfig.sampleRate = QDIV_AUDIO_RATE;
    audioDeviceConfig.dataCallback = callback_audio;
    audioDeviceConfig.pUserData = NULL;
    ma_device_init(NULL, &audioDeviceConfig, &audioDevice);
    ma_device_start(&audioDevice);
}

void qDivAudioStop() {
	ma_device_uninit(&audioDevice);
	for(int32_t decoderSL = 0; decoderSL < QDIV_AUDIO_DECODERS; decoderSL++) ma_decoder_uninit(audioDecoder + decoderSL);
}
