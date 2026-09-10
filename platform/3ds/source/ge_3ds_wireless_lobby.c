#include "ge_3ds_wireless.h"

#include <stdio.h>

void ge_3ds_wireless_lobby_run(uint32_t build_id, uint32_t asset_id)
{
    Ge3dsWireless wireless;
    Result result = ge_3ds_wireless_init(&wireless, build_id, asset_id);
    size_t selected = 0;
    unsigned frame = 0;
    int redraw = 1;
    while (aptMainLoop()) {
        u32 down;
        hidScanInput();
        down = hidKeysDown();
        if (down & KEY_START) break;
        if (down & KEY_B) {
            if (!wireless.connected) break;
            ge_3ds_wireless_leave(&wireless);
        }
        if (wireless.initialized) {
            if (!wireless.connected) {
                if (down & KEY_X) {
                    uint32_t session = (uint32_t)svcGetSystemTick();
                    result = ge_3ds_wireless_host(&wireless, session ? session : 1);
                } else if (down & KEY_Y) {
                    printf("\x1b[20;1HScanning nearby systems...       ");
                    gfxFlushBuffers();
                    result = ge_3ds_wireless_scan(&wireless);
                    selected = 0;
                } else if ((down & KEY_A) && wireless.network_count > 0)
                    result = ge_3ds_wireless_join(&wireless, selected);
                if ((down & KEY_DDOWN) && selected + 1 < wireless.network_count)
                    ++selected;
                if ((down & KEY_DUP) && selected > 0) --selected;
            } else if (down & KEY_A)
                ge_3ds_wireless_ready(&wireless, !wireless.lobby.local.ready);
            if (wireless.connected) {
                Result polled = ge_3ds_wireless_poll(&wireless, osGetTime());
                if (R_FAILED(polled)) result = polled;
            }
        }
        if (redraw || down || frame++ % 15 == 0) {
            unsigned i;
            printf("\x1b[2J\x1b[1;1HGoldenEye local wireless\n\n");
            printf("Connection test / lobby only\n");
            printf("Network matches are not available yet.\n\n");
            if (!wireless.initialized) {
                printf("Could not initialize local wireless.\n");
                printf("Check wireless is enabled.\n");
            } else if (wireless.connected) {
                printf("%s  Node %u\n", wireless.host ? "Hosting" : "Joined",
                    (unsigned)wireless.lobby.local_node);
                printf("Compatible peers: %u / 3\n",
                    ge_wireless_lobby_peer_count(&wireless.lobby));
                printf("You: %s\n", wireless.lobby.local.ready ? "Ready" : "Not ready");
                for (i = 0; i < GE_WIRELESS_MAX_NODES; ++i)
                    if (wireless.lobby.peers[i].active)
                        printf("Node %u: %s\n", i + 1,
                            wireless.lobby.peers[i].presence.ready ? "Ready" : "Not ready");
                printf("\n%s\n", ge_wireless_lobby_all_ready(&wireless.lobby)
                    ? "Lobby ready (no match runtime)" : "Waiting for peer readiness");
                printf("\nA: Toggle ready   B: Leave lobby\n");
            } else {
                printf("X: Host   Y: Find nearby lobbies\n");
                printf("Up/Down: Select   A: Join\n\n");
                for (i = 0; i < wireless.network_count; ++i)
                    printf("%c Lobby %08lx\n", i == selected ? '>' : ' ',
                        (unsigned long)wireless.advertisements[i].session);
                if (wireless.incompatible_networks)
                    printf("%u incompatible lobby(s) hidden\n", wireless.incompatible_networks);
                printf("\nB: Return to game\n");
            }
            if (R_FAILED(result)) printf("\nWireless error: %08lx\n", (unsigned long)result);
            printf("\nSTART: Return to game\n");
            redraw = 0;
        }
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }
    ge_3ds_wireless_exit(&wireless);
    consoleClear();
}
