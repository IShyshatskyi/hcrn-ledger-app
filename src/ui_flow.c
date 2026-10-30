// UX flows for the Shisha Ledger app
//
// Supports Nano S / Nano X / Nano S+ (bagl-based screens).
// Stax / Flex (NBGL touchscreen) would require a separate NBGL implementation.
//
// Three flows:
//   ux_idle_flow       — main menu: "Shisha Network" + "Quit"
//   ux_sign_flow       — transaction confirmation: amount, fee, shard, recipient
//   ux_get_pk_flow     — address display and confirmation

#include "globals.h"

extern void user_action_sign_flow_approved(void);
extern void user_action_sign_flow_rejected(void);
extern void user_action_get_pk_approved(void);
extern void user_action_get_pk_rejected(void);

// ── Idle / main menu ──────────────────────────────────────────────────────────

UX_STEP_NOCB(
    ux_idle_step_ready,
    nn,
    { "Shisha Network", "ready" });

UX_STEP_NOCB(
    ux_idle_step_version,
    bn,
    { "Version", APPVERSION });

UX_STEP_VALID(
    ux_idle_step_quit,
    pb,
    os_sched_exit(-1),
    { &C_icon_dashboard_x, "Quit" });

UX_FLOW(ux_idle_flow,
    &ux_idle_step_ready,
    &ux_idle_step_version,
    &ux_idle_step_quit);

void ui_idle(void) {
    UX_FLOW_INIT(0, ux_idle_flow);
}

// ── Sign transaction flow ─────────────────────────────────────────────────────
//
// Screens:
//   1. "Review Transaction" header
//   2. Total output amount ("Send X.XX SHISHA")
//   3. Fee ("0.000100 SHISHA")
//   4. Shard ("shard N")
//   5. Signing round ("input K of N") — shown only for multi-input transactions
//   6. Recipient / output summary (scrollable)
//   7. Approve ✓
//   8. Reject ✗

UX_STEP_NOCB(
    ux_sign_flow_1_review,
    pnn,
    { &C_icon_eye, "Review", "Transaction" });

UX_STEP_NOCB(
    ux_sign_flow_2_amount,
    bnnn_paging,
    { .title = "Send", .text = G_ctx.total_out_str });

UX_STEP_NOCB(
    ux_sign_flow_3_fee,
    bnnn_paging,
    { .title = "Fee", .text = G_ctx.fee_str });

UX_STEP_NOCB(
    ux_sign_flow_4_shard,
    bn,
    { "Chain", G_ctx.shard_str });

// "input K of N" — visible on multi-input transactions so the user knows
// how many signing rounds remain.
UX_STEP_NOCB(
    ux_sign_flow_5_round,
    bn,
    { "Signing", G_ctx.outputs_str });

UX_STEP_NOCB(
    ux_sign_flow_6_recipient,
    bnnn_paging,
    { .title = "Recipient", .text = G_ctx.recipient_str });

UX_STEP_VALID(
    ux_sign_flow_7_approve,
    pb,
    user_action_sign_flow_approved(),
    { &C_icon_validate_14, "Approve" });

UX_STEP_VALID(
    ux_sign_flow_8_reject,
    pb,
    user_action_sign_flow_rejected(),
    { &C_icon_crossmark, "Reject" });

UX_FLOW(ux_sign_flow,
    &ux_sign_flow_1_review,
    &ux_sign_flow_2_amount,
    &ux_sign_flow_3_fee,
    &ux_sign_flow_4_shard,
    &ux_sign_flow_5_round,
    &ux_sign_flow_6_recipient,
    &ux_sign_flow_7_approve,
    &ux_sign_flow_8_reject);

void ui_display_sign_tx(void) {
    ux_flow_init(0, ux_sign_flow, NULL);
}

// ── Get public key / address display flow ─────────────────────────────────────

UX_STEP_NOCB(
    ux_get_pk_flow_1_header,
    pnn,
    { &C_icon_eye, "Verify", "Address" });

UX_STEP_NOCB(
    ux_get_pk_flow_2_address,
    bnnn_paging,
    { .title = "Address", .text = G_ctx.recipient_str });

UX_STEP_VALID(
    ux_get_pk_flow_3_approve,
    pb,
    user_action_get_pk_approved(),
    { &C_icon_validate_14, "Confirm" });

UX_STEP_VALID(
    ux_get_pk_flow_4_reject,
    pb,
    user_action_get_pk_rejected(),
    { &C_icon_crossmark, "Reject" });

UX_FLOW(ux_get_pk_flow,
    &ux_get_pk_flow_1_header,
    &ux_get_pk_flow_2_address,
    &ux_get_pk_flow_3_approve,
    &ux_get_pk_flow_4_reject);

void ui_display_public_key(void) {
    ux_flow_init(0, ux_get_pk_flow, NULL);
}
