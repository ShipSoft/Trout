// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// read_digitised_hits.cpp — Phlex source and driver plugin
//
// Reads shannon's digitised-hit output (one RNTuple per detector type, each
// a flat record-per-hit stream — SHiP has no discrete "event", just a
// continuous hit stream, so there's no on-disk event boundary) and cuts it
// into time windows (see io/TimeWindow.hpp), one per "spill"-layer
// data cell. Each detector's hit collection for a window is its own typed
// product, plus a pass-through copy of the raw SimHits (and, with
// sim_particles: true, SimParticles).
//
// The plugin registers two things:
//   - a phlex source (DigitisedHitsSource), which scans the input once and
//     owns the resulting TimeWindowing, and provides the products;
//   - a driver, which is handed that source (the workflow's driver block
//     names it in uses_sources) and emits one data cell per window it owns.
// So the windows are worked out in one place and the driver can't disagree
// with the reader about how many there are.
//
// Hits are never loaded whole. A first pass over just the time columns finds
// the time range — and so the number of windows — and how far out of time
// order each RNTuple is (io/DigitisedTimeScan.hpp).
// Each RNTuple is then read in one of two ways:
//
//   - streamed, if no entry is more than max_lag_ns earlier than an entry
//     before it: read forward once into a buffer holding only the hits of
//     windows not yet served. Memory is about one owned slice plus that
//     detector's padding. This suits large runs, whose hits shannon writes
//     roughly in time order.
//   - indexed, otherwise: keep a time-sorted (time, entry) index, about 16
//     bytes per hit, and read each window's entries directly. This suits
//     small runs, where every event starts at nearly the same time and file
//     order says little about hit time.
//
// Each collection is given for the window's owned slice widened by that
// detector's time padding (DigitisedTimeScan.hpp). Only the straw-tube hits
// (and their ids) are padded today, since tracking needs the hits around what
// it owns as context. Every other collection gets exactly the owned slice, so
// each of those hits is read and written out exactly once.

#include "io/DigitisedTimeScan.hpp"
#include "io/TimeWindow.hpp"
#include "phlex/concurrency.hpp"
#include "phlex/configuration.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/core/provider_node.hpp"
#include "phlex/driver.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/fixed_hierarchy.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/products.hpp"
#include "phlex/model/type_id.hpp"
#include "phlex/source.hpp"

#include <ROOT/RNTupleReader.hxx>
#include <ROOT/RNTupleView.hxx>
#include <TROOT.h>
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
using SHiP::trout::TimePadding;
using SHiP::trout::TimeWindow;
using SHiP::trout::TimeWindowing;

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
                   TimeWindowing const& windowing, double max_lag, TimePadding padding,
                   bool indexed)
        : name_{ntuple_name},
          reader_{ROOT::RNTupleReader::Open(ntuple_name, input_file)},
          time_view_{reader_->GetView<double>(time)},
          n_entries_{reader_->GetNEntries()},
          windowing_{windowing},
          max_lag_{max_lag},
          padding_{padding},
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
                "the driver must be read_digitised_hits with this source in uses_sources",
                i, windowing_.size()));
        auto const range = windowing_.at(i).padded(padding_);
        return indexed_ ? from_index(range.lo, range.hi) : from_stream(i, range.lo, range.hi);
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
            // Nothing earlier than the padded range of the first window still
            // to be served is needed again. The buffer is in file order,
            // which is only roughly time order, so this can't just pop from
            // the front.
            double const cutoff = windowing_.at(first_unserved_).padded(padding_).lo;
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
    TimePadding padding_;
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

// The creator every consumer selects these products by (creator =
// "rntuple_source"). Each product is made by its own algorithm under it,
// rntuple_source:read_<suffix>, because phlex names an implicit provider
// after its product's creator and allows only one product per creator. A
// selector's creator matches either half of the name, so consumers needn't
// change.
constexpr char const* kCreator = "rntuple_source";

struct SourceSettings {
    std::string input_file;
    std::string layer;
    std::string stage;
    bool simulation{false};
    bool sim_particles{false};
    double step_ns{2000.0};
    double max_lag_ns{1000.0};
};

// Scans the input once at construction and owns the resulting windowing, so
// the driver below takes the number of windows from here rather than working
// it out again. Each product's provider (and its WindowedStream, i.e. its
// RNTupleReader and, in indexed mode, its index) is only made when some
// consumer asks phlex for that product.
class DigitisedHitsSource final : public phlex::source {
   public:
    explicit DigitisedHitsSource(SourceSettings settings)
        : settings_{std::move(settings)},
          ntuples_{SHiP::trout::digitised_ntuples(settings_.simulation, settings_.sim_particles)},
          scan_{SHiP::trout::scan_input(settings_.input_file, ntuples_, settings_.step_ns)} {
        auto const& windowing = scan_.windowing;
        spdlog::info(
            "read_digitised_hits: detector hits span [{}, {}] ns, giving {} windows owning "
            "{} ns each",
            scan_.all.min, scan_.all.max, windowing.size(), settings_.step_ns);

        for (auto const& ntuple : ntuples_) {
            padding_[ntuple.name] = ntuple.padding;
            spdlog::info(
                "read_digitised_hits: {} is padded by {} ns before and {} ns after the "
                "owned slice",
                ntuple.name, ntuple.padding.before_ns, ntuple.padding.after_ns);
        }

        for (auto const& [name, scan] : scan_.per_ntuple) {
            indexed_[name] = scan.max_lag > settings_.max_lag_ns;
            spdlog::info("read_digitised_hits: {} is up to {} ns out of time order, so it is {}",
                         name, scan.max_lag,
                         indexed_[name] ? "indexed (more than max_lag_ns)" : "streamed");
        }
    }

    TimeWindowing const& windowing() const { return scan_.windowing; }
    std::string const& layer() const { return settings_.layer; }

    phlex::provider_bundles create_providers(phlex::product_selector const& selector) override {
        phlex::provider_bundles bundles;

        offer<TimeWindow>(bundles, selector, "window", phlex::concurrency::unlimited,
                          [windowing = scan_.windowing] {
                              return [windowing](phlex::data_cell_index const& id) {
                                  if (id.number() >= windowing.size())
                                      throw std::out_of_range(std::format(
                                          "read_digitised_hits: window {} requested but the "
                                          "input only spans {}",
                                          id.number(), windowing.size()));
                                  return windowing.at(id.number());
                              };
                          });

        offer_hits<SHiP::UBTHit>(bundles, selector, "ubt_hits");
        offer_hits<SHiP::SBTHit>(bundles, selector, "sbt_hits");
        offer_hits<SHiP::StrawTubesHit>(bundles, selector, "straw_tubes_hits");
        offer_stream<std::uint64_t>(bundles, selector, "straw_tubes_hit_ids", "straw_tubes_hits",
                                    time_field<SHiP::StrawTubesHit>);
        offer_hits<SHiP::CaloHit>(bundles, selector, "calorimeter_hits");
        offer_hits<SHiP::TimeDetHit>(bundles, selector, "timing_detector_hits");
        if (settings_.simulation)
            offer_hits<SHiP::SimHit>(bundles, selector, "sim_hits");
        if (settings_.simulation && settings_.sim_particles)
            offer_hits<SHiP::SimParticle>(bundles, selector, "sim_particles");

        return bundles;
    }

   private:
    // Adds a provider of `suffix` to `bundles` if `selector` asks for it.
    // make_function is only called then, so nothing is opened for a product
    // no consumer reads.
    template <typename Product, typename MakeFunction>
    void offer(phlex::provider_bundles& bundles, phlex::product_selector const& selector,
               std::string const& suffix, phlex::concurrency max_concurrency,
               MakeFunction make_function) const {
        using namespace phlex::experimental;
        algorithm_name creator{identifier{kCreator}, identifier{"read_" + suffix}};
        product_specification spec{std::move(creator), identifier{suffix},
                                   make_type_id<Product>()};
        if (!selector.match(spec, identifier{settings_.layer}, identifier{settings_.stage}))
            return;
        bundles.push_back(phlex::provider_bundle{
            .provider_function = [f = make_function()](phlex::data_cell_index const& id)
                -> product_ptr { return product_for(f(id)); },
            .max_concurrency = max_concurrency,
            .spec = std::move(spec),
            .layer = settings_.layer,
            .stage = settings_.stage});
    }

    // A WindowedStream of `ntuple_name`, provided as `suffix`. The provider
    // returns the shared_ptr itself, so consumers share one copy of the
    // window's hits rather than each getting their own. Streams aren't
    // thread-safe, so the provider is serial.
    template <typename T>
    void offer_stream(phlex::provider_bundles& bundles, phlex::product_selector const& selector,
                      std::string const& suffix, std::string const& ntuple_name,
                      char const* field) const {
        offer<std::shared_ptr<std::vector<T>>>(
            bundles, selector, suffix, phlex::concurrency::serial, [&, this] {
                auto stream = std::make_shared<WindowedStream<T>>(
                    settings_.input_file, ntuple_name, field, scan_.windowing,
                    settings_.max_lag_ns, padding_.at(ntuple_name), indexed_.at(ntuple_name));
                return [stream](phlex::data_cell_index const& id) {
                    return stream->window(id.number());
                };
            });
    }

    template <typename Hit>
    void offer_hits(phlex::provider_bundles& bundles, phlex::product_selector const& selector,
                    std::string const& ntuple_name) const {
        offer_stream<Hit>(bundles, selector, ntuple_name, ntuple_name, time_field<Hit>);
    }

    SourceSettings settings_;
    std::vector<SHiP::trout::DigitisedNtuple> ntuples_;
    SHiP::trout::InputTimeScan scan_;
    std::map<std::string, TimePadding> padding_;
    std::map<std::string, bool> indexed_;
};

}  // namespace

PHLEX_REGISTER_SOURCE(s, config) {
    // Each collection's provider is serial, but phlex runs different
    // providers at the same time, so several RNTuple readers deserialise
    // dictionary-backed SHiP types on different threads at once. That needs
    // ROOT's global state (TClass, streamer info) locked. Idempotent, and must
    // happen before any reader is opened.
    ROOT::EnableThreadSafety();

    SourceSettings settings;
    settings.input_file = config.get<std::string>("input_file");
    settings.layer = config.get<std::string>("layer");
    // Implicit providers must name a real stage ("CURRENT" is rejected), and
    // a source can't see the job's, so the workflow passes it.
    settings.stage = config.get<std::string>("stage");
    if (settings.stage.empty() || settings.stage == "CURRENT")
        throw std::invalid_argument(
            "read_digitised_hits: 'stage' must name the job's stage (e.g. 'reconstruction'); "
            "'CURRENT' is reserved");
    settings.simulation = config.get<bool>("simulation", false);
    // Off by default: aegir keeps every Geant4 particle, which can be orders
    // of magnitude more entries than all the hits together, and nothing in
    // reconstruction uses them.
    settings.sim_particles = settings.simulation && config.get<bool>("sim_particles", false);
    settings.step_ns = config.get<double>("step_ns", 2000.0);
    settings.max_lag_ns = config.get<double>("max_lag_ns", 1000.0);
    if (!(settings.max_lag_ns >= 0.0))
        throw std::invalid_argument("read_digitised_hits: max_lag_ns must be >= 0");

    // Registered under the workflow's label for this source, which is what
    // the driver block's uses_sources names.
    auto const name = config.get<std::string>("module_label");
    s.add_source<DigitisedHitsSource>(name, std::move(settings));
}

// Emits one data cell per time window of the source named in the driver
// block's uses_sources, so the count comes straight from the source's scan.
PHLEX_REGISTER_DRIVER(d, config) {
    auto const layer = config.get<std::string>("layer", std::string{"spill"});

    // Optional cap, to process just the first part of the input.
    std::optional<std::size_t> max_spills;
    if (auto const cap = config.get_if_present<std::int64_t>("max_spills")) {
        if (*cap < 1)
            throw std::invalid_argument("read_digitised_hits: driver max_spills must be >= 1");
        max_spills = static_cast<std::size_t>(*cap);
    }

    // phlex matches the first parameter's type exactly, so the cursor (a cheap,
    // copyable view) is taken by value rather than const&. The source comes
    // second, from uses_sources.
    return d.driver(
        phlex::fixed_hierarchy{std::vector<std::vector<std::string>>{{"job", layer}}},
        [layer, max_spills](phlex::data_cell_cursor job, DigitisedHitsSource const& input) {
            if (input.layer() != layer)
                throw std::invalid_argument(std::format(
                    "read_digitised_hits: the driver emits '{}' data cells but its source "
                    "provides '{}'",
                    layer, input.layer()));

            std::size_t n_windows = input.windowing().size();
            spdlog::info("read_digitised_hits: driver emits {} '{}' data cells", n_windows,
                         layer);
            if (max_spills && *max_spills < n_windows) {
                spdlog::info(
                    "read_digitised_hits: max_spills limits this to the first {} of {} windows",
                    *max_spills, n_windows);
                n_windows = *max_spills;
            }
            for (std::size_t i = 0; i < n_windows; ++i)
                job.yield_child(layer, i);
        });
}
