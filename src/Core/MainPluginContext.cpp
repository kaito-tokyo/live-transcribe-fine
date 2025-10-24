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

#include <filesystem>
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
	  latestVersionFuture(_latestVersionFuture)
{
	update(settings);
}

void MainPluginContext::shutdown() noexcept {}

MainPluginContext::~MainPluginContext() noexcept {}

void MainPluginContext::getDefaults(obs_data_t *data)
{
	obs_data_set_default_string(data, "voskModelPath", "");
}

obs_properties_t *MainPluginContext::getProperties()
{
	obs_properties_t *props = obs_properties_create();

	obs_properties_add_path(props, "voskModelPath", obs_module_text("voskModelPath"), OBS_PATH_DIRECTORY, nullptr,
				nullptr);

	return props;
}

void MainPluginContext::update(obs_data_t *settings)
{
	bool contextNeedsUpdate = !recognitionContext;

	const char *newVoskModelPath = obs_data_get_string(settings, "voskModelPath");
	if (!std::filesystem::exists(newVoskModelPath)) {
		logger.warn("Vosk model path does not exist: {}", newVoskModelPath);
		recognitionContext.reset();
		return;
	}

	if (pluginProperty.voskModelPath != newVoskModelPath) {
		pluginProperty.voskModelPath = newVoskModelPath;
		contextNeedsUpdate = true;
	}

	if (contextNeedsUpdate) {
		float sampleRate = static_cast<float>(getOutputAudioInfo().samples_per_sec);
		recognitionContext = std::make_unique<RecognitionContext>(logger, newVoskModelPath, sampleRate);
	}
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
