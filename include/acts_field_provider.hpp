// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "FieldService/FieldMap.h"
#include "FieldService/IFieldSource.h"

#include <Acts/Definitions/Algebra.hpp>
#include <Acts/Definitions/Units.hpp>
#include <Acts/MagneticField/MagneticFieldError.hpp>
#include <Acts/MagneticField/MagneticFieldProvider.hpp>
#include <Acts/Utilities/Result.hpp>

#include <SHiP/Units.hpp>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <memory>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <utility>

// Global extent of a placed field map, in ACTS native units.
struct FieldBounds {
    double xmin, xmax, ymin, ymax, zmin, zmax;
};

// Extent of a map whose local origin sits at `translation` (mm) in the
// global frame. A mirrored axis stores only coordinates >= 0 and covers
// [-max, max].
[[nodiscard]] inline FieldBounds fieldBounds(ship::FieldMapGrid const& grid,
                                             ship::FieldMapSymmetry const& symmetry,
                                             std::array<double, 3> const& translation) {
    std::array<double, 6> b{};
    for (std::size_t a = 0; a < 3; ++a) {
        double const lo = symmetry.mirror[a] ? -grid.max[a] : grid.min[a];
        b[2 * a] = (lo + translation[a]) * Acts::UnitConstants::mm;
        b[2 * a + 1] = (grid.max[a] + translation[a]) * Acts::UnitConstants::mm;
    }
    return {b[0], b[1], b[2], b[3], b[4], b[5]};
}

class ShipActsFieldProvider final : public Acts::MagneticFieldProvider {
   public:
    // `eval` answers in the map's local frame; `translation` (mm) is the
    // global position of the map's origin, and `bounds` its global extent.
    ShipActsFieldProvider(std::shared_ptr<ship::IFieldEvaluator> eval, FieldBounds bounds,
                          std::array<double, 3> translation)
        : m_bounds(bounds), m_translation(translation), m_eval(std::move(eval)) {
        if (!m_eval)
            throw std::invalid_argument("ShipActsFieldProvider: null field evaluator");
    }

    struct Cache {};

    [[nodiscard]] Acts::MagneticFieldProvider::Cache makeCache(
        const Acts::MagneticFieldContext& /*mctx*/) const override {
        return Acts::MagneticFieldProvider::Cache{Cache{}};
    }

    [[nodiscard]] Acts::Result<Acts::Vector3> getField(
        const Acts::Vector3& posActs,
        Acts::MagneticFieldProvider::Cache& /*cache*/) const override {
        return doEvaluate(posActs);
    }

   private:
    using Result = Acts::Result<Acts::Vector3>;

    // Outside the mapped volume the field is genuinely negligible (the map
    // covers the full magnetic region by construction), so out-of-bounds
    // lookups return B = 0 as a *deliberate* physics statement — the
    // stepper's trial steps routinely poke past the map edges. The evaluator
    // itself clamps to the boundary value outside its grid, so without this
    // check the edge field would extend over the whole tracker. Anomalies
    // (non-finite values, evaluator exceptions), by contrast, fail the
    // lookup so the propagator aborts instead of silently continuing on a
    // straight line; they are also logged once per provider.
    [[nodiscard]] Result doEvaluate(const Acts::Vector3& posActs) const {
        if (!std::isfinite(posActs[0]) || !std::isfinite(posActs[1]) ||
            !std::isfinite(posActs[2])) {
            warnOnce("non-finite position queried");
            return Result::failure(Acts::MagneticFieldError::OutOfBounds);
        }

        if (posActs[0] < m_bounds.xmin || posActs[0] > m_bounds.xmax ||
            posActs[1] < m_bounds.ymin || posActs[1] > m_bounds.ymax ||
            posActs[2] < m_bounds.zmin || posActs[2] > m_bounds.zmax) {
            return Result::success(Acts::Vector3::Zero());
        }

        // Global → map-local frame.
        const ship::Length x =
            (posActs[0] / Acts::UnitConstants::mm - m_translation[0]) * ship::units::mm;
        const ship::Length y =
            (posActs[1] / Acts::UnitConstants::mm - m_translation[1]) * ship::units::mm;
        const ship::Length z =
            (posActs[2] / Acts::UnitConstants::mm - m_translation[2]) * ship::units::mm;

        std::array<ship::IFieldEvaluator::field_q, 3> b{};
        try {
            b = m_eval->at(x, y, z);
        } catch (std::exception const& e) {
            warnOnce(e.what());
            return Result::failure(Acts::MagneticFieldError::OutOfBounds);
        } catch (...) {
            warnOnce("unknown exception from evaluator");
            return Result::failure(Acts::MagneticFieldError::OutOfBounds);
        }

        const double bx_T = b[0].numerical_value_in(ship::units::tesla);
        const double by_T = b[1].numerical_value_in(ship::units::tesla);
        const double bz_T = b[2].numerical_value_in(ship::units::tesla);

        if (!std::isfinite(bx_T) || !std::isfinite(by_T) || !std::isfinite(bz_T)) {
            warnOnce("non-finite field value from evaluator");
            return Result::failure(Acts::MagneticFieldError::OutOfBounds);
        }

        return Result::success(Acts::Vector3{bx_T * Acts::UnitConstants::T,
                                             by_T * Acts::UnitConstants::T,
                                             bz_T * Acts::UnitConstants::T});
    }

    void warnOnce(const char* what) const {
        if (!m_warned.exchange(true))
            spdlog::warn("ShipActsFieldProvider: {} — failing field lookups", what);
    }

    FieldBounds m_bounds;
    std::array<double, 3> m_translation;
    std::shared_ptr<ship::IFieldEvaluator> m_eval;
    mutable std::atomic<bool> m_warned{false};
};

// Output of the acts_field_provider phlex module, mirroring DetectorGeometry
// (acts_geometry_provider.hpp): the field is loaded once and shared as a
// single product, rather than every consumer loading its own copy.
struct DetectorField {
    std::shared_ptr<Acts::MagneticFieldProvider> field;
};
