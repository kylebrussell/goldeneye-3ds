#include "ge_original_model_callbacks.h"
#include "game/model.h"
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static unsigned calls[2];
static Model *expected_model;
static s32 accept_root(Model *model, coord3d *source, coord3d *destination,
                       f32 *ground)
{
    assert(model == expected_model);
    assert(source->x == 1.0f && destination->x == 5.0f);
    ++calls[0];
    destination->x += 2.0f;
    destination->y += 3.0f;
    *ground = 10.0f;
    return 1;
}

static s32 reject_root(Model *model, coord3d *source, coord3d *destination,
                       f32 *ground)
{
    assert(model == expected_model);
    assert(source->x == 1.0f && destination->x == 5.0f);
    (void)ground;
    ++calls[1];
    return 0;
}

int main(void)
{
    Model models[4] = {0};
    union ModelRwData rw[4] = {0};
    union ModelRoData ro = {0};
    ModelNode node = {0};
    ModelFileHeader header = {0};
    node.Opcode = MODELNODE_OPCODE_HEADER;
    node.Data = &ro;
    header.RootNode = &node;
    assert(sizeof(models[0].unka0) == 4);
    for (unsigned i = 0; i < 4; ++i) {
        animInit(&models[i], &header, (u32 *)&rw[i]);
        assert(models[i].unka0 == 0);
        rw[i].Header.pos.x = 1.0f;
        rw[i].Header.unk34.x = 5.0f;
        rw[i].Header.unk34.y = 2.0f;
        ge_original_model_root_callback_set(&models[i],
                                             i & 1 ? reject_root : accept_root);
    }
    assert(models[0].unka0 == models[2].unka0);
    assert(models[1].unka0 == models[3].unka0);
    assert(models[0].unka0 != models[1].unka0);
    for (unsigned i = 0; i < 4; ++i) {
        expected_model = &models[i];
        sub_GAME_7F06D490(&models[i], &node);
        assert(rw[i].Header.pos.x == (i & 1 ? 1.0f : 7.0f));
        assert(rw[i].Header.pos.y == (i & 1 ? 0.0f : 15.0f));
        assert(rw[i].Header.unk34.x == (i & 1 ? 5.0f : 7.0f));
    }
    assert(calls[0] == 2 && calls[1] == 2);
    /* A copied live model keeps its function token; no address-keyed owner. */
    Model copy = models[1];
    expected_model = &copy;
    sub_GAME_7F06D490(&copy, &node);
    assert(calls[1] == 3);
    ge_original_model_root_callback_set(&copy, NULL);
    sub_GAME_7F06D490(&copy, &node);
    assert(calls[1] == 3 && rw[1].Header.pos.x == 5.0f);
    animInit(&models[0], &header, (u32 *)&rw[0]);
    assert(models[0].unka0 == 0);
    assert(models[2].unka0 != 0);
    /* Unknown/truncated addresses must fail explicitly, never be executed. */
    pid_t child = fork();
    assert(child >= 0);
    if (!child) {
        models[0].unka0 = 0x12345678;
        sub_GAME_7F06D490(&models[0], &node);
        _exit(1);
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
    puts("host native model callbacks: four models, typed dispatch, reject, copy, reset and invalid handle passed");
    return 0;
}
