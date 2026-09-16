// This file is part of the ACTS project.
//
// Copyright (C) 2016 CERN for the benefit of the ACTS project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

// Trimmed copy from the ACTS Examples framework: only GeometryIdMultiset,
// its accessor, and selectModule are used here; the volume/layer selectors,
// groupByModule, and the Athena id bimap were dropped.

#pragma once

#include "Acts/Geometry/GeometryIdentifier.hpp"
#include "Acts/Utilities/PointerTraits.hpp"
#include "Range.hpp"

#include <boost/container/flat_set.hpp>
#include <utility>

namespace ActsExamples {
namespace detail {
/// @brief Concept to define objects that have a geometryId getter method
template <typename ObjType>
concept GeometryIdObj = requires(const ObjType& obj) {
    { obj.geometryId() } -> std::same_as<Acts::GeometryIdentifier>;
};
/// @brief Concept to define objects to pointer that have a geometryId getter method
template <typename ObjType>
concept GeometryIdPtrObj =
    Acts::PointerConcept<ObjType> && GeometryIdObj<typename Acts::RemovePointer<ObjType>::type>;

/// @brief Concept to define an object which carries time
template <typename ObjType>
concept TimedObj = requires(const ObjType& obj) {
    { obj.time() };
};
/// @brief Concept to define a pointer to an object which carries time
template <typename ObjType>
concept TimedObjPtr =
    Acts::PointerConcept<ObjType> && TimedObj<typename Acts::RemovePointer<ObjType>::type>;

// extract the geometry identifier from a variety of types
struct GeometryIdGetter {
    // explicit geometry identifier are just forwarded
    constexpr Acts::GeometryIdentifier operator()(Acts::GeometryIdentifier geometryId) const {
        return geometryId;
    }
    // encoded geometry ids are converted back to geometry identifiers.
    constexpr Acts::GeometryIdentifier operator()(Acts::GeometryIdentifier::Value encoded) const {
        return Acts::GeometryIdentifier(encoded);
    }
    // support elements in map-like structures.
    template <typename T>
    constexpr Acts::GeometryIdentifier operator()(
        const std::pair<Acts::GeometryIdentifier, T>& mapItem) const {
        return mapItem.first;
    }
    // Support pointer to object that implement `.geometryId()`.
    template <GeometryIdPtrObj T>
    constexpr Acts::GeometryIdentifier operator()(const T& thing) const {
        return thing->geometryId();
    }
    // support elements that implement `.geometryId()`.
    template <GeometryIdObj T>
    inline auto operator()(const T& thing) const {
        return thing.geometryId();
    }
    // support reference_wrappers around such types as well
    template <GeometryIdObj T>
    inline auto operator()(std::reference_wrapper<T> thing) const
        -> decltype(thing.get().geometryId(), Acts::GeometryIdentifier()) {
        return thing.get().geometryId();
    }
};

struct CompareGeometryId {
    // indicate that comparisons between keys and full objects are allowed.
    using is_transparent = void;

    /// @brief Comparator that sorts objects with the same geometry id by time
    template <TimedObj Left, TimedObj Right>
    constexpr bool operator()(const Left& lhs, const Right& rhs) const {
        const auto lId = GeometryIdGetter()(lhs);
        const auto rId = GeometryIdGetter()(rhs);
        if (lId == rId) {
            return lhs.time() < rhs.time();
        }
        return lId < rId;
    }
    template <TimedObjPtr Left, TimedObjPtr Right>
    constexpr bool operator()(const Left& lhs, const Right& rhs) const {
        return (*this)(*lhs, *rhs);
    }
    // compare two elements using the automatic key extraction.
    template <typename Left, typename Right>
    constexpr bool operator()(Left&& lhs, Right&& rhs) const {
        return GeometryIdGetter()(lhs) < GeometryIdGetter()(rhs);
    }
};

}  // namespace detail

/// Store elements that know their detector geometry id, e.g. simulation hits.
///
/// @tparam T type to be stored, must be compatible with `CompareGeometryId`
///
/// The container stores an arbitrary number of elements for any geometry
/// id. Elements can be retrieved via the geometry id; elements can be selected
/// for a specific geometry id or for a larger range, e.g. a volume or a layer
/// within the geometry hierarchy using the helper functions below. Elements can
/// also be accessed by index that uniquely identifies each element regardless
/// of geometry id.
template <typename T>
using GeometryIdMultiset = boost::container::flat_multiset<T, detail::CompareGeometryId>;

/// Select all elements for the given module / sensitive surface.
template <typename T>
inline Range<typename GeometryIdMultiset<T>::const_iterator> selectModule(
    const GeometryIdMultiset<T>& container, Acts::GeometryIdentifier geoId) {
    // module is the lowest level and defines a single geometry id value
    return makeRange(container.equal_range(geoId));
}

/// Select all elements for the given module / sensitive surface.
template <typename T>
inline auto selectModule(const GeometryIdMultiset<T>& container,
                         Acts::GeometryIdentifier::Value volume,
                         Acts::GeometryIdentifier::Value layer,
                         Acts::GeometryIdentifier::Value sensitive) {
    return selectModule(
        container,
        Acts::GeometryIdentifier().withVolume(volume).withLayer(layer).withSensitive(sensitive));
}

/// The accessor for the GeometryIdMultiset container
///
/// It wraps up a few lookup methods to be used in the Combinatorial Kalman
/// Filter
template <typename T>
struct GeometryIdMultisetAccessor {
    using Container = GeometryIdMultiset<T>;
    using Key = Acts::GeometryIdentifier;
    using Value = typename GeometryIdMultiset<T>::value_type;
    using Iterator = typename GeometryIdMultiset<T>::const_iterator;

    // pointer to the container
    const Container* container = nullptr;
};

}  // namespace ActsExamples
