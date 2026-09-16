// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// identity_reco.hpp — shared registration helper for the placeholder
// per-subdetector reconstruction modules (upstream tagger, surround tagger,
// calorimeter, timing detector). Each is currently a passthrough that
// republishes the digitised hits as that detector's "reconstructed" product;
// real reconstruction replaces the lambda body per detector as it lands.

#pragma once

#include "phlex/core/product_selector.hpp"
#include "phlex/module.hpp"

#include <memory>
#include <string>
#include <vector>

template <typename Hit, typename Registrar>
void register_identity_reco(Registrar& m, phlex::configuration const& config,
                            std::string const& name, std::string const& input_suffix) {
    auto const layer = config.template get<std::string>("layer");

    m.transform(
         name,
         [](std::shared_ptr<std::vector<Hit>> const& hits) -> std::vector<Hit> {
             return hits ? *hits : std::vector<Hit>{};
         },
         phlex::concurrency::unlimited)
        .input_family(
            phlex::product_selector{.creator = "rntuple_source",
                                    .layer = phlex::experimental::identifier{layer},
                                    .suffix = phlex::experimental::identifier{input_suffix}})
        .output_product_suffixes(name);
}
