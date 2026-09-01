#include "ge_visual_probe_tour.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check_generated_tour(const char *path)
{
    GeVisualProbeView views[384];
    GeVisualProbeTour tour;
    FILE *stream = fopen(path, "rb");
    char *data;
    long length;

    assert(stream != NULL);
    assert(fseek(stream, 0L, SEEK_END) == 0);
    length = ftell(stream);
    assert(length > 0L && fseek(stream, 0L, SEEK_SET) == 0);
    data = malloc((size_t)length);
    assert(data != NULL);
    assert(fread(data, 1U, (size_t)length, stream) == (size_t)length);
    fclose(stream);
    assert(ge_visual_probe_tour_parse(data, (size_t)length, views, 384U,
                                     &tour) == GE_VISUAL_PROBE_TOUR_OK);
    assert(tour.count > 1U && tour.count <= 384U);
    free(data);
}

int main(int argc, char **argv)
{
    static const char valid[] =
        "GEVIEW1\n"
        "# frames room x y z lookx looky lookz upx upy upz label\n"
        "3 135 1 2 3 -1 0 0 0 1 0 spawn-pad-033\n"
        "2 132 4 5 6 0 0 1 0 1 0 tunnel-window\n";
    GeVisualProbeView views[2];
    GeVisualProbeTour tour;
    size_t index = 99U;

    assert(ge_visual_probe_tour_parse(valid, strlen(valid), views, 2U,
               &tour) == GE_VISUAL_PROBE_TOUR_OK);
    assert(tour.count == 2U && tour.total_frames == 5U);
    assert(ge_visual_probe_tour_view_at(&tour, 0U, &index) == &views[0]);
    assert(index == 0U && strcmp(views[0].label, "spawn-pad-033") == 0);
    assert(ge_visual_probe_tour_view_at(&tour, 2U, &index) == &views[0]);
    assert(ge_visual_probe_tour_view_at(&tour, 3U, &index) == &views[1]);
    assert(index == 1U && views[1].room == 132U);
    assert(ge_visual_probe_tour_view_at(&tour, 4U, NULL) == &views[1]);
    assert(ge_visual_probe_tour_view_at(&tour, 5U, NULL) == NULL);
    assert(ge_visual_probe_tour_parse(valid, strlen(valid), views, 1U,
               &tour) == GE_VISUAL_PROBE_TOUR_CAPACITY_EXCEEDED);
    assert(ge_visual_probe_tour_parse(
               "GEVIEW1\n1 1 0 0 0 1 0 0 2 0 0 bad\n", 41U,
               views, 2U, &tour) == GE_VISUAL_PROBE_TOUR_INVALID_FORMAT);
    while (--argc > 0) check_generated_tour(*++argv);
    puts("visual probe tour parser/finite camera selection: ok");
    return 0;
}
