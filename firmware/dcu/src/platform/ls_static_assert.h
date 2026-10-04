/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file ls_static_assert.h
 * @brief Compile-time assertion for table sizes and enumeration equalities.
 */

#ifndef LS_STATIC_ASSERT_H
#define LS_STATIC_ASSERT_H

/**
 * @brief Fails the compilation when @p cond is false.
 *
 * @param cond Integer constant expression.
 * @param msg  String literal reported by the compiler.
 */
#define LS_STATIC_ASSERT(cond, msg) _Static_assert((cond), msg)

#endif /* LS_STATIC_ASSERT_H */
