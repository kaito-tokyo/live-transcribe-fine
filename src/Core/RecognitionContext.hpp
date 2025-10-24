/*
Live Transcribe Fine
Copyright (C) 2025 Kaito Udagawa umireon@kaito.tokyo

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include <memory>

#include <ILogger.hpp>
#include <ObsUnique.hpp>

#include <vosk_api.h>

namespace KaitoTokyo {
namespace LiveTranscribeFine {

using UniqueVoskModel = std::unique_ptr<VoskModel, decltype(&vosk_model_free)>;
using UniqueVoskRecognizer = std::unique_ptr<VoskRecognizer, decltype(&vosk_recognizer_free)>;

class RecognitionContext {
private:
    const BridgeUtils::ILogger &logger;
    UniqueVoskModel voskModel;
    UniqueVoskRecognizer voskRecognizer;

public:
    RecognitionContext(const BridgeUtils::ILogger &_logger, const char *voskModelPath, float sampleRate) :
    logger(_logger),
    voskModel([voskModelPath](){
        VoskModel *voskModel = vosk_model_new(voskModelPath);
        if (!voskModel) {
            throw std::runtime_error("Failed to load Vosk model from path: " + std::string(voskModelPath));
        }
        return voskModel;
    }(), vosk_model_free),
    voskRecognizer([rawVoskModel = voskModel.get(), sampleRate]() {
        VoskRecognizer *voskRecognizer = vosk_recognizer_new(rawVoskModel, sampleRate);
        if (!voskRecognizer) {
            throw std::runtime_error("Failed to create Vosk recognizer");
        }
        return voskRecognizer;
    }(), vosk_recognizer_free) {}

    obs_audio_data *filterAudio(obs_audio_data *audio) {
        if (!audio) {
            logger.error("Invalid audio data");
            return nullptr;
        }

        if (!voskModel) {
            logger.error("Vosk model is not initialized");
            return audio;
        }

        if (!voskRecognizer) {
            logger.error("Vosk recognizer is not initialized");
            return audio;
        }

        int sampleCount = audio->frames;
        const float *pcm_float = reinterpret_cast<const float *>(audio->data[0]);
        std::vector<int16_t> pcm_int16(sampleCount);
        for (int i = 0; i < sampleCount; ++i) {
            float sample = pcm_float[i];
            if (sample > 1.0f) sample = 1.0f;
            if (sample < -1.0f) sample = -1.0f;
            pcm_int16[i] = static_cast<int16_t>(sample * 32767.0f);
        }

        int accept_result = vosk_recognizer_accept_waveform_s(voskRecognizer.get(), pcm_int16.data(), sampleCount);
        const char *result_json = nullptr;
        if (accept_result) {
            result_json = vosk_recognizer_result(voskRecognizer.get());
            logger.info("Vosk transcription result: {}", result_json);
        } else {
            result_json = vosk_recognizer_partial_result(voskRecognizer.get());
        }
        return audio;
    }
};

} // namespace LiveTranscribeFine
} // namespace KaitoTokyo
