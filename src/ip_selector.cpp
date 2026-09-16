// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// ip_selector.cpp — converts simulated particles to reconstructed particles
// with an impact-parameter estimate attached (currently with respect to the
// origin; a real primary-vertex estimate replaces that later).

#include "phlex/core/product_selector.hpp"
#include "phlex/module.hpp"

#include <SHiP/RecParticle.hpp>
#include <SHiP/SimParticle.hpp>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace {

double calculateIP(const std::array<double, 3>& r, const std::array<double, 3>& p) {
    // placeholder for impact parameter - calculated with respect to the origin
    const double p2 = p[0] * p[0] + p[1] * p[1] + p[2] * p[2];
    if (p2 < 1e-30)
        return 0.0;

    // Cross product r × p
    const double cx = r[1] * p[2] - r[2] * p[1];
    const double cy = r[2] * p[0] - r[0] * p[2];
    const double cz = r[0] * p[1] - r[1] * p[0];

    return std::sqrt((cx * cx + cy * cy + cz * cz) / p2);
}

}  // namespace

PHLEX_REGISTER_ALGORITHMS(m, config) {
    auto const layer = config.get<std::string>("layer");

    m.transform(
         "ip_selector",
         [](std::shared_ptr<std::vector<SHiP::SimParticle>> const& ip)
             -> std::vector<SHiP::RecParticle> {
             std::vector<SHiP::RecParticle> op;
             if (!ip)
                 return op;
             op.reserve(ip->size());
             for (auto const& p : *ip) {
                 op.emplace_back(SHiP::fromSimParticle(p));
                 op.back().ipPV = calculateIP(p.vertex, p.momentum);
             }
             return op;
         },
         phlex::concurrency::unlimited)
        .input_family(phlex::product_selector{
            .creator = "rntuple_source", .layer = layer, .suffix = "sim_particles"})
        .output_product_suffixes("rec_particles");
}
