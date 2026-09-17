// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// surround_tagger_histogrammer.hpp — Surround Background Tagger validation
// histograms. A placeholder for the SBT experts to fill in: so far only the
// per-event hit multiplicity, which comes from the shared
// MultiplicityHistogrammer. Book SBT-specific histograms in the constructor and
// fill them in observe(); once there are enough of them, drop the base class
// and keep only this file (see SpectrometerHistogrammer for how that ends up
// looking).

#pragma once

#include "HistoFileService.hpp"
#include "detectors/multiplicity_histogrammer.hpp"

#include <SHiP/detectors/SBTHit.hpp>
#include <memory>
#include <utility>
#include <vector>

class SurroundTaggerHistogrammer : public MultiplicityHistogrammer<SHiP::SBTHit> {
   public:
    explicit SurroundTaggerHistogrammer(std::shared_ptr<HistoFileService> file_service)
        : MultiplicityHistogrammer{std::move(file_service), "h_surround_tagger_multiplicity",
                                   "Surround tagger hits per event;N;Events"} {}

    // Deliberately not inherited: phlex deduces the bound object type and the
    // member pointer's class in one go (see delegate() in
    // phlex/metaprogramming/delegate.hpp), so &SurroundTaggerHistogrammer::observe
    // has to name a member of this class. Convenient, since this is also where
    // the SBT-specific fills belong.
    void observe(std::vector<SHiP::SBTHit> const& hits) { MultiplicityHistogrammer::observe(hits); }
};
