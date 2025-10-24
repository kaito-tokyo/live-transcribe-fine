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

#include <stdexcept>

#include <obs-module.h>

#include <ObsLogger.hpp>
#include <UpdateChecker.hpp>

#include "PluginConfig.hpp"

using namespace KaitoTokyo::BridgeUtils;
using namespace KaitoTokyo::LiveTranscribeFine;

namespace {

std::shared_future<std::string> latestVersionFuture;

inline const ILogger &logger()
{
	static const ObsLogger instance("[" PLUGIN_NAME "] ");
	return instance;
}

} // namespace

bool main_plugin_context_module_load()
try {
	curl_global_init(CURL_GLOBAL_DEFAULT);
	latestVersionFuture =
		std::async(std::launch::async, [] {
			PluginConfig pluginConfig(PluginConfig::load());
			return KaitoTokyo::UpdateChecker::fetchLatestVersion(pluginConfig.latestVersionURL);
		}).share();
	return true;
} catch (const std::exception &e) {
	logger().logException(e, "Failed to load main plugin context");
	return false;
} catch (...) {
	logger().error("Failed to load main plugin context: unknown error");
	return false;
}

const char *main_plugin_context_get_name(void *)
{
	return obs_module_text("pluginName");
}

void *main_plugin_context_create(obs_data_t *settings, obs_source_t *source)
try {
	auto self = std::make_shared<MainPluginContext>(settings, source, logger(), latestVersionFuture);
	return new std::shared_ptr<MainPluginContext>(self);
} catch (const std::exception &e) {
	logger().logException(e, "Failed to create main plugin context");
	return nullptr;
} catch (...) {
	logger().error("Failed to create main plugin context: unknown error");
	return nullptr;
}

void main_plugin_context_destroy(void *data)
try {
	if (!data) {
		logger().error("main_plugin_context_destroy called with null data");
		return;
	}

	auto self = static_cast<std::shared_ptr<MainPluginContext> *>(data);
	self->get()->shutdown();
	delete self;

} catch (const std::exception &e) {
	logger().logException(e, "Failed to destroy main plugin context");
} catch (...) {
	logger().error("Failed to destroy main plugin context: unknown error");
}

void main_plugin_context_get_defaults(obs_data_t *data)
{
	MainPluginContext::getDefaults(data);
}

obs_properties_t *main_plugin_context_get_properties(void *data)
try {
	if (!data) {
		logger().error("main_plugin_context_get_properties called with null data");
		return obs_properties_create();
	}

	auto self = static_cast<std::shared_ptr<MainPluginContext> *>(data);
	return self->get()->getProperties();
} catch (const std::exception &e) {
	logger().logException(e, "Failed to get properties");
	return obs_properties_create();
} catch (...) {
	logger().error("Failed to get properties: unknown error");
	return obs_properties_create();
}

void main_plugin_context_update(void *data, obs_data_t *settings)
try {
	if (!data) {
		logger().error("main_plugin_context_update called with null data");
		return;
	}

	auto self = static_cast<std::shared_ptr<MainPluginContext> *>(data);
	self->get()->update(settings);
} catch (const std::exception &e) {
	logger().logException(e, "Failed to update main plugin context");
} catch (...) {
	logger().error("Failed to update main plugin context: unknown error");
}

obs_audio_data *main_plugin_context_filter_audio(void *data, obs_audio_data *audio)
try {
	if (!data) {
		logger().error("main_plugin_context_filter_audio called with null data");
		return audio;
	}

	auto self = static_cast<std::shared_ptr<MainPluginContext> *>(data);
	return self->get()->filterAudio(audio);
} catch (const std::exception &e) {
	logger().logException(e, "Failed to filter audio in main plugin context");
	return audio;
} catch (...) {
	logger().error("Failed to filter audio in main plugin context: unknown error");
	return audio;
}
