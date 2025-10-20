// Unified hash container aliases with pluggable backends.
// Only two supported backends are kept:
//   -DEXACT_USE_BOOST
//   -DEXACT_USE_ANKERL
// Default: ankerl::unordered_dense if none is defined.

#pragma once

#include <type_traits>
#include <functional>

#if defined(__has_include)
#  define EXACT__HAS_INCLUDE(x) __has_include(x)
#else
#  define EXACT__HAS_INCLUDE(x) 0
#endif

// Enforce single-backend selection
#if defined(EXACT_USE_BOOST)
#  define EXACT__BACKEND_SELECTED 1
#endif
#if defined(EXACT_USE_ANKERL)
#  ifdef EXACT__BACKEND_SELECTED
#    error "Multiple EXACT_USE_* backends defined"
#  else
#    define EXACT__BACKEND_SELECTED 1
#  endif
#endif

// Default to ankerl
#ifndef EXACT__BACKEND_SELECTED
#  define EXACT_USE_ANKERL 1
#  define EXACT__BACKEND_SELECTED 1
#endif

// Backend: Boost.Unordered
#if defined(EXACT_USE_BOOST)
#  include <boost/unordered_set.hpp>
#  include <boost/unordered_map.hpp>
namespace exact { namespace hash {
template <typename Key, typename Hash, typename Eq>
using unordered_set = boost::unordered_set<Key, Hash, Eq>;
template <typename Key, typename T, typename Hash, typename Eq>
using unordered_map = boost::unordered_map<Key, T, Hash, Eq>;
}} // namespace exact::hash
#endif

// Backend: ankerl::unordered_dense
#if defined(EXACT_USE_ANKERL)
#  if EXACT__HAS_INCLUDE(<ankerl/unordered_dense.h>)
#    include <ankerl/unordered_dense.h>
namespace exact { namespace hash {
template <typename Key, typename Hash, typename Eq>
using unordered_set = ankerl::unordered_dense::set<Key, Hash, Eq>;
template <typename Key, typename T, typename Hash, typename Eq>
using unordered_map = ankerl::unordered_dense::map<Key, T, Hash, Eq>;
}} // namespace exact::hash
#  else
#    include <unordered_set>
#    include <unordered_map>
namespace exact { namespace hash {
template <typename Key, typename Hash, typename Eq>
using unordered_set = std::unordered_set<Key, Hash, Eq>;
template <typename Key, typename T, typename Hash, typename Eq>
using unordered_map = std::unordered_map<Key, T, Hash, Eq>;
}} // namespace exact::hash
#  endif
#endif
