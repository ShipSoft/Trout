// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// multiplicity_histogrammer.hpp — per-event hit-multiplicity validation
// histogram, shared by the subdetectors whose monitoring is (so far) just a
// multiplicity count. A detector that grows richer monitoring gets its own
// histogrammer class instead (see SpectrometerHistogrammer).

#pragma once

#include "HistoFileService.hpp"

#include <ROOT/RHist.hxx>
#include <ROOT/RHistConcurrentFiller.hxx>
#include <ROOT/RHistFillContext.hxx>

#include <memory>
#include <oneapi/tbb/enumerable_thread_specific.h>
#include <string>
#include <utility>
#include <vector>

template <typename Hit>
class MultiplicityHistogrammer {
   public:
    MultiplicityHistogrammer(std::shared_ptr<HistoFileService> file_service, std::string hist_name,
                             std::string hist_title)
        : file_service_{std::move(file_service)},
          hist_name_{std::move(hist_name)},
          hist_title_{std::move(hist_title)},
          h_multiplicity_{make_hist(1000, -0.5, 999.5)},
          f_multiplicity_{h_multiplicity_} {}

    void observe(std::vector<Hit> const& hits) {
        ensure_context().Fill(static_cast<double>(hits.size()));
    }

    ~MultiplicityHistogrammer() {
        fill_contexts_.clear();
        file_service_->put(hist_name_.c_str(), hist_title_.c_str(), *h_multiplicity_);
    }

   private:
    using HistD = ROOT::Experimental::RHist<double>;
    using FillerD = ROOT::Experimental::RHistConcurrentFiller<double>;
    using ContextD = ROOT::Experimental::RHistFillContext<double>;

    ContextD& ensure_context() {
        auto& ctx = fill_contexts_.local();
        if (!ctx) {
            ctx = f_multiplicity_.CreateFillContext();
        }
        return *ctx;
    }

    std::shared_ptr<HistoFileService> file_service_;
    std::string hist_name_;
    std::string hist_title_;
    std::shared_ptr<HistD> h_multiplicity_;
    FillerD f_multiplicity_;
    tbb::enumerable_thread_specific<std::shared_ptr<ContextD>> fill_contexts_;
};
