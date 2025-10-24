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

#include <stdint.h>

#include <obs.h>

#ifdef __cplusplus

#include <future>
#include <memory>

#include <vosk_api.h>

#include <ILogger.hpp>

#include "PluginProperty.hpp"
#include "RecognitionContext.hpp"

namespace KaitoTokyo {
namespace LiveTranscribeFine {

class MainPluginContext : public std::enable_shared_from_this<MainPluginContext> {
public:
	obs_source_t *const source;
	const BridgeUtils::ILogger &logger;

private:
	std::shared_future<std::string> latestVersionFuture;

	PluginProperty pluginProperty;

	std::unique_ptr<RecognitionContext> recognitionContext = nullptr;

public:
	MainPluginContext(obs_data_t *const settings, obs_source_t *const source, const BridgeUtils::ILogger &logger,
			  std::shared_future<std::string> latestVersionFuture);

	void shutdown() noexcept;
	~MainPluginContext() noexcept;

	static void getDefaults(obs_data_t *data);

	obs_properties_t *getProperties();
	void update(obs_data_t *settings);

	obs_audio_data *filterAudio(obs_audio_data *audio);
};

} // namespace LiveTranscribeFine
} // namespace KaitoTokyo

extern "C" {
#endif // __cplusplus

bool main_plugin_context_module_load(void);

const char *main_plugin_context_get_name(void *type_data);
void *main_plugin_context_create(obs_data_t *settings, obs_source_t *source);
void main_plugin_context_destroy(void *data);
void main_plugin_context_get_defaults(obs_data_t *data);
obs_properties_t *main_plugin_context_get_properties(void *data);
void main_plugin_context_update(void *data, obs_data_t *settings);
struct obs_audio_data *main_plugin_context_filter_audio(void *data, struct obs_audio_data *audio);

#ifdef __cplusplus
}
#endif // __cplusplus
