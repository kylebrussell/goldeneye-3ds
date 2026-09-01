#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <libaudio.h>

static uint8_t *read_file(const char *path, size_t *size_out)
{
    FILE *file = fopen(path, "rb");
    long length;
    uint8_t *bytes;

    assert(file != NULL);
    assert(fseek(file, 0, SEEK_END) == 0);
    length = ftell(file);
    assert(length >= 0);
    assert(fseek(file, 0, SEEK_SET) == 0);
    bytes = malloc((size_t)length);
    assert(bytes != NULL);
    assert(fread(bytes, 1, (size_t)length, file) == (size_t)length);
    assert(fclose(file) == 0);
    *size_out = (size_t)length;
    return bytes;
}

int main(int argc, char **argv)
{
    ALCSeq sequence;
    ALEvent event;
    size_t size;
    uint8_t *bytes;
    const uint32_t tempo = UINT32_C(0x068a1a);
    const float one_quarter_seconds = (float)tempo / 1000000.0f;

    assert(argc == 2);
    bytes = read_file(argv[1], &size);
    assert(size > 0x1527U);

    alCSeqNew(&sequence, bytes);
    assert(sequence.validTracks == UINT32_C(0x7fff));
    assert(fabsf(sequence.qnpt - (1.0f / 384.0f)) < 0.000001f);
    assert(sequence.curLoc[0] == bytes + 0x45U);
    assert(sequence.curLoc[14] == bytes + 0x1528U);
    assert(sequence.curLoc[15] == NULL);

    alCSeqNextEvent(&sequence, &event);
    assert(event.type == AL_TEMPO_EVT);
    assert(event.msg.tempo.status == AL_MIDI_Meta);
    assert(event.msg.tempo.type == AL_MIDI_META_TEMPO);
    assert(event.msg.tempo.byte1 == 0x06U);
    assert(event.msg.tempo.byte2 == 0x8aU);
    assert(event.msg.tempo.byte3 == 0x1aU);
    assert(fabsf(alCSeqTicksToSec(&sequence, 384, tempo)
            - one_quarter_seconds) < 0.00001f);
    assert(alCSeqSecToTicks(&sequence, one_quarter_seconds, tempo) == 384U);

    free(bytes);
    puts("original big-endian Dam CSeq header and first event pass");
    return 0;
}
