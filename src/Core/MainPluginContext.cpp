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

#include "MainPluginContext.h"

#include <future>
#include <stdexcept>
#include <thread>

#include <obs-module.h>
#include <obs-frontend-api.h>

#include <vosk_api.h>

#include <GsUnique.hpp>
#include <ILogger.hpp>
#include <ObsUnique.hpp>

using namespace KaitoTokyo::BridgeUtils;

namespace KaitoTokyo {
namespace LiveTranscribeFine {

namespace {

inline obs_audio_info getOutputAudioInfo()
{
	obs_audio_info oai;
	obs_get_audio_info(&oai);
	return oai;
}

} // anonymous namespace

MainPluginContext::MainPluginContext(obs_data_t *const settings, obs_source_t *const _source,
				     const BridgeUtils::ILogger &_logger,
				     std::shared_future<std::string> _latestVersionFuture)
	: source{_source},
	  logger(_logger),
	  latestVersionFuture(_latestVersionFuture),
	  outputAudioInfo{getOutputAudioInfo()},
	  recognitionContext(std::make_unique<RecognitionContext>(logger, "/Users/umireon/vosk-models/vosk-model-ja-0.22", (float)outputAudioInfo.samples_per_sec))
{
	update(settings);
}

void MainPluginContext::startup() noexcept {}

void MainPluginContext::shutdown() noexcept {}

MainPluginContext::~MainPluginContext() noexcept {}

std::uint32_t MainPluginContext::getWidth() const noexcept
{
	return 0;
}

std::uint32_t MainPluginContext::getHeight() const noexcept
{
	return 0;
}

void MainPluginContext::getDefaults(obs_data_t *data)
{
	UNUSED_PARAMETER(data);
}

obs_properties_t *MainPluginContext::getProperties()
{
	obs_properties_t *props = obs_properties_create();
	return props;
}

void MainPluginContext::update(obs_data_t *settings)
{
	UNUSED_PARAMETER(settings);
}

obs_audio_data *MainPluginContext::filterAudio(obs_audio_data *audio)
try {
	if (recognitionContext) {
		return recognitionContext->filterAudio(audio);
	} else {
		return audio;
	}
} catch (const std::exception &e) {
	logger.error("Failed to filter audio: {}", e.what());
	return audio;
} catch (...) {
	logger.error("Failed to filter audio: unknown error");
	return audio;
}

} // namespace LiveTranscribeFine
} // namespace KaitoTokyo
