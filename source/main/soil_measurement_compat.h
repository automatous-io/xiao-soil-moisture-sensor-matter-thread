//
// Copyright 2026 AUTOMATOUS.IO
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// soil_measurement_compat.h
//
// Compatibility layer for the espressif/esp_matter 1.5.1 managed component,
// which ships the SoilMeasurement server cluster but not the soil sensor
// endpoint helper or SetSoilMoisture* glue that esp-matter main added after
// the v1.5 branch cut. Mirrors those upstream pieces (Apache 2.0). Delete
// when a component release includes them.

#pragma once

#include <esp_matter.h>

#include <clusters/SoilMeasurement/Attributes.h>
#include <clusters/SoilMeasurement/Ids.h>

namespace chip::app::Clusters::SoilMeasurement {

// Same API as esp-matter main's <clusters/soil_measurement/integration.h>.
CHIP_ERROR SetSoilMoistureMeasuredValue(
    EndpointId endpointId,
    const Attributes::SoilMoistureMeasuredValue::TypeInfo::Type &soilMoistureMeasuredValue);

void SetSoilMoistureLimits(
    EndpointId endpointId,
    const Attributes::SoilMoistureMeasurementLimits::TypeInfo::Type &soilMoistureLimits);

} // namespace chip::app::Clusters::SoilMeasurement

namespace esp_matter::endpoint::soil_sensor {

typedef struct config {
    esp_matter::cluster::identify::config_t identify;
} config_t;

// Same shape as esp-matter main's endpoint::soil_sensor::create: Soil Sensor
// device type (0x0045) with Identify and a registry-managed SoilMeasurement
// cluster.
endpoint_t *create(node_t *node, config_t *config, uint8_t flags, void *priv_data);

} // namespace esp_matter::endpoint::soil_sensor
