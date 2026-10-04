/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file main.c
 * @brief DCU entry point: start-up sequence and scheduler.
 *
 * EcuM_EarlyInit() (safe outputs) runs from the reset handler before the C runtime
 * initialisation; main() continues with clock, interrupt priorities, SysTick, watchdog, the
 * single AFIO remap write and the initialisation of every layer, then starts the scheduler.
 */

#include "services/ecum/ecum.h"
#include "services/sched/sched.h"

int main(void)
{
    EcuM_Init();
    Sched_Start();
    return 0;
}
