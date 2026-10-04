/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include "cgw_arbiter/cgw_arbiter.h"

#include <string.h>

#include "cgw_ports/cgw_rng_port.h"
#include "ls_dtc_gen.h"
#include "ls_enums_gen.h"
#include "ls_params_gen.h"

/* Result used internally for "check passed". */
#define ARB_PASS LS_COMMAND_RESULT_OK

static void arb_clear_out(cgw_arb_out_t *out)
{
    (void)memset(out, 0, sizeof(*out));
}

static void arb_set_win_ack(cgw_arb_out_t *out, uint32_t ref_id, uint8_t result)
{
    out->win_ack.valid = true;
    out->win_ack.ref_id = ref_id;
    out->win_ack.result = result;
}

static void arb_set_door_ack(cgw_arb_out_t *out, uint32_t ref_id, uint8_t result)
{
    out->door_ack.valid = true;
    out->door_ack.ref_id = ref_id;
    out->door_ack.result = result;
}

/* Next 8-bit identifier: 1..255, wrapping, skipping 0 and @p avoid (0 = nothing to avoid). */
static uint8_t arb_next_id(uint8_t last, uint8_t avoid)
{
    uint8_t next = (uint8_t)(last + 1u);

    for (uint8_t i = 0u; (i < 2u) && ((next == 0u) || ((avoid != 0u) && (next == avoid))); i++)
    {
        next = (uint8_t)(next + 1u);
        if (next == 0u)
        {
            next = 1u;
        }
    }
    return next;
}

static uint8_t arb_random_seed(void)
{
    uint8_t seed = 0u;

    if (cgw_rng_fill(&seed, sizeof(seed)) != CGW_OK)
    {
        seed = 0u;
    }
    return seed;
}

/* @satisfies SWR-CGW-025 */
static uint8_t arb_alloc_press_id(cgw_arbiter_t *a)
{
    if (!a->pid.seeded)
    {
        a->pid.last = arb_random_seed();
        a->pid.seeded = true;
    }
    a->pid.last = arb_next_id(a->pid.last, a->pid.echo);
    return a->pid.last;
}

static uint8_t arb_alloc_req_id(cgw_arbiter_t *a)
{
    if (!a->door.seeded)
    {
        a->door.last_req_id = arb_random_seed();
        a->door.seeded = true;
    }
    a->door.last_req_id = arb_next_id(a->door.last_req_id, 0u);
    return a->door.last_req_id;
}

/*
 * Mode, communication and version checks shared by window and door commands (LS-SAIC-001
 * section 8.7). INIT and SAFE give REJECTED_MODE; DEGRADED is reported as FAILED_COMM after the
 * more specific checks (LS-SAIC-001 section 6.2).
 */
static uint8_t arb_check_mode_comm(const cgw_arb_env_t *env)
{
    uint8_t rc = ARB_PASS;

    if (((env->cgw_mode != LS_NODE_MODE_NORMAL) && (env->cgw_mode != LS_NODE_MODE_DEGRADED)) ||
        env->pairing_busy)
    {
        rc = LS_COMMAND_RESULT_REJECTED_MODE;
    }
    else if (!env->dcu_alive)
    {
        rc = LS_COMMAND_RESULT_FAILED_COMM;
    }
    else if ((env->dcu_mode != LS_NODE_MODE_NORMAL) && (env->dcu_mode != LS_NODE_MODE_DEGRADED))
    {
        rc = LS_COMMAND_RESULT_REJECTED_MODE;
    }
    else
    {
        /* Passed. */
    }
    return rc;
}

static uint8_t arb_check_version_bus(const cgw_arb_env_t *env)
{
    uint8_t rc = ARB_PASS;

    if (!env->version_ok)
    {
        rc = LS_COMMAND_RESULT_REJECTED_VERSION;
    }
    else if (env->bus_off)
    {
        rc = LS_COMMAND_RESULT_FAILED_COMM;
    }
    else
    {
        /* Passed. */
    }
    return rc;
}

static bool arb_move_is_valid(const cgw_arbiter_t *a, const cgw_arb_move_t *mv)
{
    return (mv->press_id > a->win.last_press_id) &&
           ((mv->dir == LS_WINDOW_DIRECTION_UP) || (mv->dir == LS_WINDOW_DIRECTION_DOWN)) &&
           (mv->hold_ms <= LS_T_NEW_PRESS_MAX_MS);
}

/* @satisfies SWR-CGW-020 */
static uint8_t arb_admit_window(const cgw_arbiter_t *a, const cgw_arb_move_t *mv,
                                const cgw_arb_env_t *env)
{
    uint8_t rc = arb_check_mode_comm(env);

    if (rc != ARB_PASS)
    {
        /* Mode, DCU alive or DCU mode failed. */
    }
    else if (env->win_inhibit)
    {
        rc = LS_COMMAND_RESULT_REJECTED_INTERLOCK;
    }
    else if ((rc = arb_check_version_bus(env)) != ARB_PASS)
    {
        /* Version or bus-off failed. */
    }
    else if (!env->win_sts_valid)
    {
        rc = LS_COMMAND_RESULT_FAILED_COMM;
    }
    else if (!env->link_ok)
    {
        rc = LS_COMMAND_RESULT_REJECTED_LINK_QUALITY;
    }
    else if (!arb_move_is_valid(a, mv))
    {
        rc = LS_COMMAND_RESULT_REJECTED_INVALID;
    }
    else if (env->cgw_mode == LS_NODE_MODE_DEGRADED)
    {
        rc = LS_COMMAND_RESULT_FAILED_COMM;
    }
    else
    {
        rc = LS_COMMAND_RESULT_ACCEPTED;
    }
    return rc;
}

static void arb_start_press(cgw_arbiter_t *a, const cgw_arb_move_t *mv)
{
    a->win.state = CGW_ARB_WIN_MOVING;
    a->win.press_id = mv->press_id;
    a->win.dir = mv->dir;
    a->win.t_ka_ms = mv->t_rx_ms;
    a->win.t_start_ms = mv->t_rx_ms;
    a->win.can_press_id = arb_alloc_press_id(a);
}

void cgw_arbiter_init(cgw_arbiter_t *a)
{
    if (a != NULL)
    {
        (void)memset(a, 0, sizeof(*a));
    }
}

/* @satisfies SWR-CGW-015 */
void cgw_arbiter_session_start(cgw_arbiter_t *a)
{
    if (a != NULL)
    {
        a->session++;
        a->win.last_press_id = 0u;
        a->cache.valid = false;
        a->last_door_request = 0u;
        a->last_door_result = LS_COMMAND_RESULT_UNSPECIFIED;
    }
}

/* @satisfies SWR-CGW-021 */
/* @satisfies SWR-CGW-026 */
void cgw_arbiter_on_move(cgw_arbiter_t *a, const cgw_arb_move_t *mv, const cgw_arb_env_t *env,
                         cgw_arb_out_t *out)
{
    arb_clear_out(out);
    if ((a->win.state != CGW_ARB_WIN_IDLE) && (mv->press_id == a->win.press_id))
    {
        if (a->win.state == CGW_ARB_WIN_LATCHED)
        {
            /* WindowMove frames of a latched press are ignored. */
        }
        else if (mv->dir != a->win.dir)
        {
            cgw_arbiter_latch(a, CGW_LATCH_DIR_CHANGE, out);
        }
        else
        {
            a->win.t_ka_ms = mv->t_rx_ms;
            out->intent_changed = true;
        }
    }
    else if (mv->press_id > a->win.last_press_id)
    {
        uint8_t rc = arb_admit_window(a, mv, env);

        if (a->win.state != CGW_ARB_WIN_IDLE)
        {
            /* A newer press releases the old one. */
            a->win.state = CGW_ARB_WIN_IDLE;
            out->stop_decided = true;
        }
        a->win.last_press_id = mv->press_id;
        if (rc == LS_COMMAND_RESULT_ACCEPTED)
        {
            arb_start_press(a, mv);
        }
        arb_set_win_ack(out, mv->press_id, rc);
        out->intent_changed = true;
    }
    else
    {
        /* Older or already handled press_id: ignored. */
    }
}

void cgw_arbiter_on_stop(cgw_arbiter_t *a, uint32_t press_id, cgw_arb_out_t *out)
{
    arb_clear_out(out);
    if ((a->win.state != CGW_ARB_WIN_IDLE) && (press_id == a->win.press_id))
    {
        a->win.state = CGW_ARB_WIN_IDLE;
        out->intent_changed = true;
        out->stop_decided = true;
    }
}

static uint8_t arb_admit_door(const cgw_arbiter_t *a, const cgw_arb_door_cmd_t *cmd,
                              const cgw_arb_env_t *env)
{
    uint8_t rc = ARB_PASS;

    if ((cmd->action != LS_DOOR_ACTION_LOCK) && (cmd->action != LS_DOOR_ACTION_UNLOCK))
    {
        rc = LS_COMMAND_RESULT_REJECTED_INVALID;
    }
    else if (!env->door_sts_valid)
    {
        rc = LS_COMMAND_RESULT_FAILED_COMM;
    }
    else if (env->door_rate_limited)
    {
        rc = LS_COMMAND_RESULT_REJECTED_RATE_LIMIT;
    }
    else if (env->lock_inhibit)
    {
        rc = LS_COMMAND_RESULT_REJECTED_INTERLOCK;
    }
    else if (((rc = arb_check_mode_comm(env)) != ARB_PASS) ||
             ((rc = arb_check_version_bus(env)) != ARB_PASS))
    {
        /* Mode, communication or version failed. */
    }
    else if (env->cgw_mode == LS_NODE_MODE_DEGRADED)
    {
        rc = LS_COMMAND_RESULT_FAILED_COMM;
    }
    else if (a->door.state == CGW_ARB_DOOR_PENDING)
    {
        rc = LS_COMMAND_RESULT_REJECTED_BUSY;
    }
    else
    {
        rc = LS_COMMAND_RESULT_ACCEPTED;
    }
    return rc;
}

/* Repeated request_id of the current session (LS-SAIC-001 section 8.7). */
static bool arb_door_repeat(const cgw_arbiter_t *a, const cgw_arb_door_cmd_t *cmd,
                            cgw_arb_out_t *out)
{
    bool handled = false;

    if ((a->door.state == CGW_ARB_DOOR_PENDING) && (a->door.session == a->session) &&
        (a->door.request_id == cmd->request_id))
    {
        arb_set_door_ack(out, cmd->request_id, LS_COMMAND_RESULT_ACCEPTED);
        handled = true;
    }
    else if (a->cache.valid && (a->cache.request_id == cmd->request_id) &&
             ((uint32_t)(cmd->t_rx_ms - a->cache.t_done_ms) < LS_T_CGW_DOOR_CACHE_MS))
    {
        out->door_result_valid = true;
        out->door_request_id = cmd->request_id;
        out->door_result = a->cache.result;
        out->door_lock_state = a->cache.lock_state;
        handled = true;
    }
    else
    {
        /* New request. */
    }
    return handled;
}

/* @satisfies SWR-CGW-030 */
/* @satisfies SWR-CGW-031 */
/* @satisfies SWR-CGW-033 */
void cgw_arbiter_on_door(cgw_arbiter_t *a, const cgw_arb_door_cmd_t *cmd, const cgw_arb_env_t *env,
                         cgw_arb_out_t *out)
{
    arb_clear_out(out);
    if (!arb_door_repeat(a, cmd, out))
    {
        uint8_t rc = arb_admit_door(a, cmd, env);

        arb_set_door_ack(out, cmd->request_id, rc);
        a->last_door_request = cmd->request_id;
        a->last_door_result = rc;
        if (rc == LS_COMMAND_RESULT_ACCEPTED)
        {
            a->door.state = CGW_ARB_DOOR_PENDING;
            a->door.request_id = cmd->request_id;
            a->door.session = a->session;
            a->door.t_sent_ms = cmd->t_rx_ms;
            a->door.can_req_id = arb_alloc_req_id(a);
            a->cache.valid = false;
            out->door_can_valid = true;
            out->door_can_req_id = a->door.can_req_id;
            out->door_can_request = cmd->action;
        }
    }
}

/* @satisfies SWR-CGW-025 */
void cgw_arbiter_on_win_sts(cgw_arbiter_t *a, uint8_t echo, uint8_t win_result, cgw_arb_out_t *out)
{
    arb_clear_out(out);
    if (!a->pid.seeded)
    {
        a->pid.last = echo;
        a->pid.seeded = true;
    }
    a->pid.echo = echo;
    if ((a->win.state == CGW_ARB_WIN_MOVING) && (echo == a->win.can_press_id) &&
        (win_result != LS_COMMAND_RESULT_UNSPECIFIED) &&
        (win_result != LS_COMMAND_RESULT_ACCEPTED) && (win_result != LS_COMMAND_RESULT_OK))
    {
        a->win.state = CGW_ARB_WIN_LATCHED;
        arb_set_win_ack(out, a->win.press_id, win_result);
        out->notice = LS_NOTICE_WINDOW_STOP_DCU;
        out->intent_changed = true;
        out->stop_decided = true;
    }
}

static void arb_door_complete(cgw_arbiter_t *a, uint8_t result, uint8_t lock_state, uint32_t now_ms,
                              cgw_arb_out_t *out)
{
    a->door.state = CGW_ARB_DOOR_IDLE;
    if (a->door.session == a->session)
    {
        out->door_result_valid = true;
        out->door_request_id = a->door.request_id;
        out->door_result = result;
        out->door_lock_state = lock_state;
        a->last_door_result = result;
        a->cache.valid = true;
        a->cache.request_id = a->door.request_id;
        a->cache.result = result;
        a->cache.lock_state = lock_state;
        a->cache.t_done_ms = now_ms;
    }
}

/* @satisfies SWR-CGW-032 */
void cgw_arbiter_on_door_sts(cgw_arbiter_t *a, const cgw_arb_door_sts_t *sts, cgw_arb_out_t *out)
{
    arb_clear_out(out);
    if (!a->door.seeded)
    {
        a->door.last_req_id = sts->last_req_id;
        a->door.seeded = true;
    }
    if ((a->door.state == CGW_ARB_DOOR_PENDING) && (sts->last_req_id == a->door.can_req_id) &&
        (sts->last_result != LS_COMMAND_RESULT_UNSPECIFIED) &&
        (sts->last_result != LS_COMMAND_RESULT_ACCEPTED))
    {
        arb_door_complete(a, sts->last_result, sts->lock_state, sts->t_rx_ms, out);
    }
}

/* @satisfies SWR-CGW-022 */
void cgw_arbiter_latch(cgw_arbiter_t *a, cgw_latch_reason_t reason, cgw_arb_out_t *out)
{
    static const uint8_t k_result[] = {
        LS_COMMAND_RESULT_FAILED_TIMEOUT,        /* KA_TIMEOUT */
        LS_COMMAND_RESULT_REJECTED_LINK_QUALITY, /* LINK_QUALITY */
        LS_COMMAND_RESULT_REJECTED_INVALID,      /* DIR_CHANGE */
        LS_COMMAND_RESULT_FAILED_COMM,           /* COMM */
        LS_COMMAND_RESULT_REJECTED_MODE,         /* MODE */
        LS_COMMAND_RESULT_FAILED_TIMEOUT,        /* BACKSTOP */
        LS_COMMAND_RESULT_UNSPECIFIED,           /* SESSION: no acknowledgement */
    };
    static const uint16_t k_notice[] = {
        LS_NOTICE_WINDOW_STOP_KA_TIMEOUT, LS_NOTICE_WINDOW_STOP_LINK_QUALITY,
        LS_NOTICE_WINDOW_STOP_DIR_CHANGE, LS_NOTICE_WINDOW_STOP_COMM,
        LS_NOTICE_WINDOW_STOP_MODE,       LS_NOTICE_WINDOW_STOP_BACKSTOP,
        LS_NOTICE_WINDOW_STOP_SESSION,
    };

    if ((a->win.state == CGW_ARB_WIN_MOVING) && ((size_t)reason < sizeof(k_result)))
    {
        a->win.state = CGW_ARB_WIN_LATCHED;
        if (k_result[reason] != LS_COMMAND_RESULT_UNSPECIFIED)
        {
            arb_set_win_ack(out, a->win.press_id, k_result[reason]);
        }
        out->notice = k_notice[reason];
        out->intent_changed = true;
        out->stop_decided = true;
    }
}

void cgw_arbiter_tick(cgw_arbiter_t *a, uint32_t now_ms, const cgw_arb_env_t *env,
                      cgw_arb_out_t *out)
{
    arb_clear_out(out);
    if (a->win.state == CGW_ARB_WIN_MOVING)
    {
        if ((uint32_t)(now_ms - a->win.t_ka_ms) > LS_T_CGW_KA_TO_MS)
        {
            cgw_arbiter_latch(a, CGW_LATCH_KA_TIMEOUT, out);
        }
        else if ((uint32_t)(now_ms - a->win.t_start_ms) >= LS_T_CGW_MAX_RUN_BACKSTOP_MS)
        {
            cgw_arbiter_latch(a, CGW_LATCH_BACKSTOP, out);
        }
        else if (!env->link_ok)
        {
            cgw_arbiter_latch(a, CGW_LATCH_LINK_QUALITY, out);
        }
        else
        {
            /* Press healthy. */
        }
    }
    if ((a->door.state == CGW_ARB_DOOR_PENDING) &&
        ((uint32_t)(now_ms - a->door.t_sent_ms) >= LS_T_CGW_DOOR_RESULT_TO_MS))
    {
        arb_door_complete(a, LS_COMMAND_RESULT_FAILED_TIMEOUT, env->lock_state, now_ms, out);
    }
}

void cgw_arbiter_intent(const cgw_arbiter_t *a, cgw_win_intent_t *intent)
{
    intent->active = (a->win.state != CGW_ARB_WIN_IDLE);
    intent->latched = (a->win.state == CGW_ARB_WIN_LATCHED);
    intent->dir = intent->active ? a->win.dir : LS_WINDOW_REQUEST_STOP;
    intent->can_press_id = a->win.can_press_id;
    intent->t_ka_ms = a->win.t_ka_ms;
}

bool cgw_arbiter_press_active(const cgw_arbiter_t *a)
{
    return a->win.state != CGW_ARB_WIN_IDLE;
}

uint8_t cgw_arbiter_stop_reason_result(uint8_t stop_reason)
{
    static const uint8_t k_map[LS_WINDOW_STOP_REASON_COUNT] = {
        LS_COMMAND_RESULT_UNSPECIFIED,        /* NONE */
        LS_COMMAND_RESULT_OK,                 /* RELEASED */
        LS_COMMAND_RESULT_REJECTED_INTERLOCK, /* UPPER_LIMIT */
        LS_COMMAND_RESULT_REJECTED_INTERLOCK, /* LOWER_LIMIT */
        LS_COMMAND_RESULT_FAILED_TIMEOUT,     /* HOLD_TIMEOUT */
        LS_COMMAND_RESULT_FAILED_COMM,        /* CAN_TIMEOUT */
        LS_COMMAND_RESULT_FAILED_COMM,        /* E2E_ERROR */
        LS_COMMAND_RESULT_FAILED_ACTUATOR,    /* STALL */
        LS_COMMAND_RESULT_FAILED_ACTUATOR,    /* OVERCURRENT */
        LS_COMMAND_RESULT_FAILED_TIMEOUT,     /* MAX_RUNTIME */
        LS_COMMAND_RESULT_REJECTED_INTERLOCK, /* OBSTACLE */
        LS_COMMAND_RESULT_REJECTED_INTERLOCK, /* UNDERVOLTAGE */
        LS_COMMAND_RESULT_REJECTED_INTERLOCK, /* OVERVOLTAGE */
        LS_COMMAND_RESULT_REJECTED_INTERLOCK, /* OVERTEMP */
        LS_COMMAND_RESULT_FAILED_ACTUATOR,    /* DRIVER_FAULT */
        LS_COMMAND_RESULT_REJECTED_MODE,      /* MODE_INHIBIT */
        LS_COMMAND_RESULT_FAILED_ACTUATOR,    /* DIR_MISMATCH */
    };

    return (stop_reason < LS_WINDOW_STOP_REASON_COUNT) ? k_map[stop_reason]
                                                       : LS_COMMAND_RESULT_UNSPECIFIED;
}
