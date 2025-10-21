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

#include <GsUnique.hpp>
#include <ILogger.hpp>
#include <ObsUnique.hpp>

using namespace KaitoTokyo::BridgeUtils;

namespace KaitoTokyo {
namespace LiveTranscribeFine {

MainPluginContext::MainPluginContext(obs_data_t *const settings, obs_source_t *const _source,
				     const BridgeUtils::ILogger &_logger,
				     std::shared_future<std::string> _latestVersionFuture)
	: source{_source},
	  logger(_logger),
	  latestVersionFuture(_latestVersionFuture)
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

void MainPluginContext::activate() {}

void MainPluginContext::deactivate() {}

void MainPluginContext::show() {}

void MainPluginContext::hide() {}

void MainPluginContext::videoTick(float seconds)
{
	UNUSED_PARAMETER(seconds);
}

void MainPluginContext::videoRender()
{
	obs_source_skip_video_filter(source);
}

obs_source_frame *MainPluginContext::filterVideo(struct obs_source_frame *frame)
try {
	return frame;
} catch (const std::exception &e) {
	logger.error("Failed to create rendering context: {}", e.what());
	return frame;
} catch (...) {
	logger.error("Failed to create rendering context: unknown error");
	return frame;
}

} // namespace LiveTranscribeFine
} // namespace KaitoTokyo
