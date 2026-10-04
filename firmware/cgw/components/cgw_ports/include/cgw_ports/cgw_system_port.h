/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_system_port.h
 * @brief System port: controlled restart.
 *
 * Implemented by cgw_platform_esp. Used by the core after a factory reset.
 */

#ifndef CGW_SYSTEM_PORT_H
#define CGW_SYSTEM_PORT_H

#ifdef __cplusplus
extern "C"
{
#endif

    /**
 * @brief Request a software restart once the frames already queued have been handed over.
 *
 * @note The restart happens asynchronously; the caller keeps CGW_WinCmd at STOP meanwhile.
 */
    void cgw_system_restart(void);

#ifdef __cplusplus
}
#endif

#endif /* CGW_SYSTEM_PORT_H */
