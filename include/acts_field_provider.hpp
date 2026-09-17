// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

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
#include <memory>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <utility>

// TODO: read the bounds from the field map instead of hardcoding them.
// Values are in ACTS native units.
struct FieldBounds {
    double xmin = -4000 * Acts::UnitConstants::mm, xmax = 4000 * Acts::UnitConstants::mm,
           ymin = -4000 * Acts::UnitConstants::mm, ymax = 4000 * Acts::UnitConstants::mm,
           zmin = 0 * Acts::UnitConstants::mm, zmax = 100000 * Acts::UnitConstants::mm;
};

class ShipActsFieldProvider final : public Acts::MagneticFieldProvider {
   public:
    explicit ShipActsFieldProvider(std::shared_ptr<ship::IFieldEvaluator> eval)
        : m_eval(std::move(eval)) {
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
    // stepper's trial steps routinely poke past the map edges. Anomalies
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

        const ship::Length x = posActs[0] / Acts::UnitConstants::mm * ship::units::mm;
        const ship::Length y = posActs[1] / Acts::UnitConstants::mm * ship::units::mm;
        const ship::Length z = posActs[2] / Acts::UnitConstants::mm * ship::units::mm;

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
    std::shared_ptr<ship::IFieldEvaluator> m_eval;
    mutable std::atomic<bool> m_warned{false};
};

// Output of the acts_field_provider phlex module, mirroring DetectorGeometry
// (acts_geometry_provider.hpp): the field is loaded once and shared as a
// single product, rather than every consumer loading its own copy.
struct DetectorField {
    std::shared_ptr<Acts::MagneticFieldProvider> field;
};
