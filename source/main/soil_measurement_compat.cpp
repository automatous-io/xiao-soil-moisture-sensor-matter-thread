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

// soil_measurement_compat.cpp - see header for why this exists.

#include "soil_measurement_compat.h"

#include <unordered_map>

#include <app/clusters/soil-measurement-server/soil-measurement-cluster.h>
#include <app/server-cluster/ServerClusterInterfaceRegistry.h>
#include <data_model_provider/esp_matter_data_model_provider.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;

namespace {

// Mirrors esp-matter main's soil_measurement/integration.cpp, adapted to
// the v1.5.1 registry and callback signatures.
std::unordered_map<EndpointId, LazyRegisteredServerCluster<SoilMeasurementCluster>> gServers;
std::unordered_map<EndpointId, SoilMeasurement::Attributes::SoilMoistureMeasurementLimits::TypeInfo::Type> gLimits;

void soil_measurement_init_cb(uint16_t endpoint_id)
{
    const EndpointId endpointId = endpoint_id;
    if (gServers[endpointId].IsConstructed()) {
        return;
    }
    VerifyOrDieWithMsg(gLimits.find(endpointId) != gLimits.end(), AppServer,
                       "Please set the limits for SoilMeasurementCluster on Endpoint 0x%" PRIx16, endpointId);
    gServers[endpointId].Create(endpointId, gLimits[endpointId]);

    CHIP_ERROR err =
        esp_matter::data_model::provider::get_instance().registry().Register(gServers[endpointId].Registration());
    if (err != CHIP_NO_ERROR) {
        ChipLogError(AppServer, "Failed to register SoilMeasurement - Error: %" CHIP_ERROR_FORMAT, err.Format());
    }
}

void soil_measurement_shutdown_cb(uint16_t endpoint_id)
{
    const EndpointId endpointId = endpoint_id;
    CHIP_ERROR err =
        esp_matter::data_model::provider::get_instance().registry().Unregister(&gServers[endpointId].Cluster());
    if (err != CHIP_NO_ERROR) {
        ChipLogError(AppServer, "SoilMeasurement unregister error: %" CHIP_ERROR_FORMAT, err.Format());
    }
    gServers[endpointId].Destroy();
}

} // namespace

namespace chip::app::Clusters::SoilMeasurement {

CHIP_ERROR SetSoilMoistureMeasuredValue(
    EndpointId endpointId, const Attributes::SoilMoistureMeasuredValue::TypeInfo::Type &soilMoistureMeasuredValue)
{
    if (gServers[endpointId].IsConstructed()) {
        return gServers[endpointId].Cluster().SetSoilMoistureMeasuredValue(soilMoistureMeasuredValue);
    }
    return CHIP_ERROR_INCORRECT_STATE;
}

void SetSoilMoistureLimits(EndpointId endpointId,
                           const Attributes::SoilMoistureMeasurementLimits::TypeInfo::Type &soilMoistureLimits)
{
    gLimits[endpointId] = soilMoistureLimits;
}

} // namespace chip::app::Clusters::SoilMeasurement

namespace esp_matter::endpoint::soil_sensor {

static constexpr uint32_t k_device_type_id = 0x0045;  // Matter 1.5 Soil Sensor
static constexpr uint8_t k_device_type_version = 1;
static constexpr uint16_t k_cluster_revision = 1;

endpoint_t *create(node_t *node, config_t *config, uint8_t flags, void *priv_data)
{
    endpoint_t *endpoint = esp_matter::endpoint::create(node, flags, priv_data);
    VerifyOrReturnValue(endpoint != nullptr, nullptr);
    VerifyOrReturnValue(add_device_type(endpoint, k_device_type_id, k_device_type_version) == ESP_OK, nullptr);

    VerifyOrReturnValue(cluster::identify::create(endpoint, &config->identify, CLUSTER_FLAG_SERVER) != nullptr,
                        nullptr);

    cluster_t *cluster = esp_matter::cluster::create(endpoint, SoilMeasurement::Id, CLUSTER_FLAG_SERVER);
    VerifyOrReturnValue(cluster != nullptr, nullptr);

    // Shell attributes only: reads are served by the registered
    // SoilMeasurementCluster via the provider registry.
    cluster::global::attribute::create_feature_map(cluster, 0);
    cluster::global::attribute::create_cluster_revision(cluster, k_cluster_revision);
    attribute::create(cluster, SoilMeasurement::Attributes::SoilMoistureMeasurementLimits::Id,
                      ATTRIBUTE_FLAG_MANAGED_INTERNALLY, esp_matter_array(NULL, 0, 0));
    attribute::create(cluster, SoilMeasurement::Attributes::SoilMoistureMeasuredValue::Id,
                      ATTRIBUTE_FLAG_MANAGED_INTERNALLY | ATTRIBUTE_FLAG_NULLABLE,
                      esp_matter_nullable_uint8(nullable<uint8_t>()));

    cluster::set_init_and_shutdown_callbacks(cluster, soil_measurement_init_cb, soil_measurement_shutdown_cb);

    return endpoint;
}

} // namespace esp_matter::endpoint::soil_sensor
