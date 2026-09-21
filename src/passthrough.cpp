// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// passthrough.cpp — minimal demo plugin: republishes the simulated particles
// unchanged. Useful as a phlex wiring smoke test and as a template for new
// modules.

#include "phlex/core/product_selector.hpp"
#include "phlex/module.hpp"

#include <SHiP/SimParticle.hpp>
#include <memory>
#include <string>
#include <vector>

PHLEX_REGISTER_ALGORITHMS(m, config) {
    auto const layer = config.get<std::string>("layer");

    m.transform(
         "passthrough",
         [](std::shared_ptr<std::vector<SHiP::SimParticle>> const& particles)
             -> std::vector<SHiP::SimParticle> {
             return particles ? *particles : std::vector<SHiP::SimParticle>{};
         },
         phlex::concurrency::unlimited)
        .input_family(phlex::product_selector{
            .creator = "rntuple_source", .layer = layer, .suffix = "sim_particles"})
        .output_product_suffixes("processed_value");
}
