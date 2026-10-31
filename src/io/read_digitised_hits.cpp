// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// read_digitised_hits.cpp — Phlex provider plugin
//
// Reads shannon's digitised-hit output (one RNTuple per detector type, each
// a flat record-per-hit stream — SHiP has no discrete "event", just a
// continuous hit stream, so there's no on-disk event boundary) and cuts it
// into sliding time windows (see io/TimeWindow.hpp), one per "spill"-layer
// data cell. Each detector's hit collection for a window is its own typed
// product, plus a pass-through copy of the raw SimHits (and, with
// sim_particles: true, SimParticles).
//
// Hits are never loaded whole. A first pass over just the time columns finds
// the time range — and so the number of windows, which time_window_driver
// derives the same way (io/DigitisedTimeScan.hpp) to emit one "spill" data
// cell per window — and how far out of time order each RNTuple is.
// Each RNTuple is then read in one of two ways:
//
//   - streamed, if no entry is more than max_lag_ns earlier than an entry
//     before it: read forward once into a buffer holding only the hits of
//     windows not yet served. Memory is about one window. This suits large
//     runs, whose hits shannon writes roughly in time order.
//   - indexed, otherwise: keep a time-sorted (time, entry) index, about 16
//     bytes per hit, and read each window's entries directly. This suits
//     small runs, where every event starts at nearly the same time and file
//     order says little about hit time.
//
// Only the straw-tube hits (and their ids) are given for the whole window,
// since tracking needs the hits around what it owns as context. Every other
// collection is cut to the window's owned slice, so each hit is read and
// written out exactly once even though the windows overlap.

#include "io/DigitisedTimeScan.hpp"
#include "io/TimeWindow.hpp"
#include "phlex/configuration.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/module.hpp"
#include "phlex/source.hpp"

#include <ROOT/RNTupleReader.hxx>
#include <ROOT/RNTupleView.hxx>
#include <tuple>
#include <type_traits>

#include <SHiP/SimHit.hpp>
#include <SHiP/SimParticle.hpp>
#include <SHiP/detectors/CaloHit.hpp>
#include <SHiP/detectors/SBTHit.hpp>
#include <SHiP/detectors/StrawTubesHit.hpp>
#include <SHiP/detectors/TimeDetHit.hpp>
#include <SHiP/detectors/UBTHit.hpp>
#include <algorithm>
#include <cstdint>
#include <deque>
#include <format>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using SHiP::trout::time_field;
using SHiP::trout::TimeWindow;
using SHiP::trout::TimeWindowing;

// Which part of a window a stream hands out.
enum class Slice { window, owned };

// Hands out the entries of one RNTuple a window at a time, streamed or
// indexed (see the top of this file). T is either the hit type itself or
// std::uint64_t, in which case the values are the entries' indices in the
// RNTuple — stable ids for the hits across overlapping windows. Either way,
// a window's values come in file order. Not thread-safe: register it serial.
template <typename T>
class WindowedStream {
    static constexpr bool kEntryIds = std::is_same_v<T, std::uint64_t>;

   public:
    WindowedStream(std::string const& input_file, std::string const& ntuple_name, char const* time,
                   TimeWindowing const& windowing, double max_lag, Slice slice, bool indexed)
        : name_{ntuple_name},
          reader_{ROOT::RNTupleReader::Open(ntuple_name, input_file)},
          time_view_{reader_->GetView<double>(time)},
          n_entries_{reader_->GetNEntries()},
          windowing_{windowing},
          max_lag_{max_lag},
          slice_{slice},
          indexed_{indexed},
          served_(windowing.size(), false) {
        if constexpr (!kEntryIds)
            value_view_.emplace(reader_->GetView<T>("hit"));
        if (indexed_) {
            index_.reserve(n_entries_);
            for (ROOT::NTupleSize_t i = 0; i < n_entries_; ++i)
                index_.push_back(IndexEntry{time_view_(i), i});
            std::ranges::sort(index_, [](IndexEntry const& a, IndexEntry const& b) {
                return std::tie(a.time, a.entry) < std::tie(b.time, b.entry);
            });
        }
    }

    std::shared_ptr<std::vector<T>> window(std::size_t i) {
        if (i >= windowing_.size())
            throw std::out_of_range(std::format(
                "read_digitised_hits: window {} requested but the input only spans {} windows; "
                "the driver must be time_window_driver with the same input_file, window_ns, "
                "step_ns, simulation and sim_particles",
                i, windowing_.size()));
        auto const w = windowing_.at(i);
        double const lo = slice_ == Slice::window ? w.lo : w.own_lo;
        double const hi = slice_ == Slice::window ? w.hi : w.own_hi;
        return indexed_ ? from_index(lo, hi) : from_stream(i, lo, hi);
    }

   private:
    struct Entry {
        double time;
        T value;
    };
    struct IndexEntry {
        double time;
        ROOT::NTupleSize_t entry;
    };

    T load(ROOT::NTupleSize_t entry) {
        if constexpr (kEntryIds)
            return entry;
        else
            return (*value_view_)(entry);
    }

    std::shared_ptr<std::vector<T>> from_index(double lo, double hi) {
        auto const first = std::ranges::lower_bound(index_, lo, {}, &IndexEntry::time);
        auto const last = std::ranges::lower_bound(first, index_.end(), hi, {}, &IndexEntry::time);
        // Read in file order: friendlier to ROOT's cluster cache than time
        // order, and the same order a streamed window would have.
        std::vector<ROOT::NTupleSize_t> entries;
        entries.reserve(static_cast<std::size_t>(last - first));
        for (auto it = first; it != last; ++it)
            entries.push_back(it->entry);
        std::ranges::sort(entries);

        auto out = std::make_shared<std::vector<T>>();
        out->reserve(entries.size());
        for (auto const e : entries)
            out->push_back(load(e));
        return out;
    }

    std::shared_ptr<std::vector<T>> from_stream(std::size_t i, double lo, double hi) {
        read_until(hi + max_lag_);

        auto out = std::make_shared<std::vector<T>>();
        for (auto const& e : buffer_) {
            if (e.time >= lo && e.time < hi)
                out->push_back(e.value);
        }

        served_[i] = true;
        while (first_unserved_ < served_.size() && served_[first_unserved_])
            ++first_unserved_;
        if (first_unserved_ == served_.size()) {
            buffer_.clear();
        } else {
            // Nothing earlier than the first window still to be served is
            // needed again. The buffer is in file order, which is only
            // roughly time order, so this can't just pop from the front.
            double const cutoff = windowing_.at(first_unserved_).lo;
            std::erase_if(buffer_, [cutoff](Entry const& e) { return e.time < cutoff; });
        }
        return out;
    }

    // Reads forward until an entry at or after t_stop has been read. A window
    // ending at hi is complete once that holds for t_stop = hi + max_lag.
    void read_until(double t_stop) {
        while (next_ < n_entries_ && max_time_read_ < t_stop) {
            double const t = time_view_(next_);
            // Can't happen when the first pass chose streaming, but this is
            // what guarantees no hit is silently dropped.
            if (t < max_time_read_ - max_lag_)
                throw std::runtime_error(std::format(
                    "read_digitised_hits: {} entry {} at {} ns is {} ns earlier than one already "
                    "read, more than max_lag_ns = {}",
                    name_, next_, t, max_time_read_ - t, max_lag_));
            max_time_read_ = std::max(max_time_read_, t);
            buffer_.push_back(Entry{t, load(next_)});
            ++next_;
        }
    }

    std::string name_;
    std::unique_ptr<ROOT::RNTupleReader> reader_;
    ROOT::RNTupleView<double> time_view_;
    std::optional<ROOT::RNTupleView<T>> value_view_;
    ROOT::NTupleSize_t n_entries_;

    TimeWindowing windowing_;
    double max_lag_;
    Slice slice_;
    bool indexed_;

    // Indexed mode.
    std::vector<IndexEntry> index_;

    // Streamed mode.
    ROOT::NTupleSize_t next_{0};
    double max_time_read_{-std::numeric_limits<double>::infinity()};
    std::deque<Entry> buffer_;
    std::vector<bool> served_;
    std::size_t first_unserved_{0};
};

template <typename T, typename Registrar>
void provide_stream(Registrar& m, std::shared_ptr<WindowedStream<T>> stream,
                    std::string const& suffix, phlex::experimental::identifier const& layer) {
    // Provide the shared_ptr itself, so consumers share one copy of the
    // window's hits rather than each getting their own.
    m.provide(
         "read_" + suffix,
         [stream = std::move(stream)](phlex::data_cell_index const& id)
             -> std::shared_ptr<std::vector<T>> { return stream->window(id.number()); },
         phlex::concurrency::serial)
        .output_product("rntuple_source", phlex::experimental::identifier{suffix}, layer);
}

// What every WindowedStream of one input shares, so each collection's stream
// can be made with just its name, time field and slice.
struct WindowedReading {
    std::string input_file;
    TimeWindowing windowing;
    double max_lag;
    std::map<std::string, bool> indexed;

    template <typename T>
    std::shared_ptr<WindowedStream<T>> stream(std::string const& ntuple_name, char const* field,
                                              Slice slice) const {
        return std::make_shared<WindowedStream<T>>(input_file, ntuple_name, field, windowing,
                                                   max_lag, slice, indexed.at(ntuple_name));
    }
};

template <typename Hit, typename Registrar>
void provide_hit_type(Registrar& m, WindowedReading const& reading, std::string const& ntuple_name,
                      Slice slice, phlex::experimental::identifier const& layer) {
    provide_stream(m, reading.stream<Hit>(ntuple_name, time_field<Hit>, slice), ntuple_name, layer);
}

}  // namespace

PHLEX_REGISTER_PROVIDERS(m, config) {
    auto const input_file = config.get<std::string>("input_file");
    auto const layer = phlex::experimental::identifier{config.get<std::string>("layer")};
    auto const isSim = config.get<bool>("simulation", false);
    // Off by default: aegir keeps every Geant4 particle, which can be orders
    // of magnitude more entries than all the hits together, and nothing in
    // reconstruction uses them.
    auto const simParticles = isSim && config.get<bool>("sim_particles", false);
    auto const window_ns = config.get<double>("window_ns", 6500.0);
    auto const step_ns = config.get<double>("step_ns", 2000.0);
    auto const max_lag_ns = config.get<double>("max_lag_ns", 1000.0);
    if (!(max_lag_ns >= 0.0))
        throw std::invalid_argument("read_digitised_hits: max_lag_ns must be >= 0");

    // time_window_driver runs the same scan with the same settings to decide
    // how many "spill" data cells to emit, so the two agree on the windows.
    auto const [scans, all, windowing] = SHiP::trout::scan_input(
        input_file, SHiP::trout::digitised_ntuples(isSim, simParticles), window_ns, step_ns);
    spdlog::info(
        "read_digitised_hits: hits span [{}, {}] ns, giving {} windows of {} ns every {} ns",
        all.min, all.max, windowing.size(), window_ns, step_ns);

    std::map<std::string, bool> indexed;
    for (auto const& [name, scan] : scans) {
        indexed[name] = scan.max_lag > max_lag_ns;
        spdlog::info("read_digitised_hits: {} is up to {} ns out of time order, so it is {}", name,
                     scan.max_lag, indexed[name] ? "indexed (more than max_lag_ns)" : "streamed");
    }

    m.provide(
         "provide_window",
         [windowing](phlex::data_cell_index const& id) -> TimeWindow {
             if (id.number() >= windowing.size())
                 throw std::out_of_range(
                     std::format("read_digitised_hits: window {} requested but the input only "
                                 "spans {}; check time_window_driver's settings match",
                                 id.number(), windowing.size()));
             return windowing.at(id.number());
         },
         phlex::concurrency::unlimited)
        .output_product("rntuple_source", "window", layer);

    using enum Slice;
    WindowedReading const reading{input_file, windowing, max_lag_ns, indexed};
    provide_hit_type<SHiP::UBTHit>(m, reading, "ubt_hits", owned, layer);
    provide_hit_type<SHiP::SBTHit>(m, reading, "sbt_hits", owned, layer);
    provide_hit_type<SHiP::StrawTubesHit>(m, reading, "straw_tubes_hits", window, layer);
    provide_stream(
        m,
        reading.stream<std::uint64_t>("straw_tubes_hits", time_field<SHiP::StrawTubesHit>, window),
        "straw_tubes_hit_ids", layer);
    provide_hit_type<SHiP::CaloHit>(m, reading, "calorimeter_hits", owned, layer);
    provide_hit_type<SHiP::TimeDetHit>(m, reading, "timing_detector_hits", owned, layer);
    if (isSim)
        provide_hit_type<SHiP::SimHit>(m, reading, "sim_hits", owned, layer);
    if (simParticles)
        provide_hit_type<SHiP::SimParticle>(m, reading, "sim_particles", owned, layer);
}
