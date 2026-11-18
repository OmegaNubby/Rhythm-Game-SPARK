#include "chart.h"
#include "ff.h"
#include "ff_stdio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CHARTS 3
#define MAX_CHART_NOTES 128

typedef struct {
    char osu_path[96];
    char wav_path[96];
    chart_info_t info;
} chart_entry_t;

static FATFS fs;
static chart_entry_t charts[MAX_CHARTS];
static int chart_count_value;
static chart_note_t chart_notes[MAX_CHART_NOTES];
static int chart_note_count;
static int chart_note_index;

static bool has_extension(const char *name, const char *extension) {
    size_t name_len = strlen(name);
    size_t extension_len = strlen(extension);
    if (name_len < extension_len) return false;
    const char *suffix = name + name_len - extension_len;
    for (size_t i = 0; i < extension_len; i++) {
        char a = suffix[i];
        char b = extension[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = (char)(b + ('a' - 'A'));
        if (a != b) return false;
    }
    return true;
}

static void strip_line(char *line) {
    size_t len = strlen(line);
    while (len > 0 &&
           (line[len - 1] == '\r' || line[len - 1] == '\n' ||
            line[len - 1] == ' ')) {
        line[--len] = '\0';
    }
}

static void read_chart_header(chart_entry_t *chart) {
    FF_FILE *osu = ff_fopen(chart->osu_path, "r");
    char line[192];

    strncpy(chart->wav_path, "", sizeof(chart->wav_path));
    chart->info.difficulty = 1;
    chart->info.bpm = 0;
    chart->info.length_ms = 0;
    strncpy(chart->info.title, "Untitled", sizeof(chart->info.title));

    if (osu == NULL) return;

    while (ff_fgets(line, sizeof(line), osu) != NULL) {
        strip_line(line);

        if (strncmp(line, "Title:", 6) == 0) {
            strncpy(chart->info.title, line + 6, CHART_TITLE_MAX);
            chart->info.title[CHART_TITLE_MAX] = '\0';
        } else if (strncmp(line, "Version:", 8) == 0) {
            char *version = line + 8;
            if (strstr(version, "Hard") != NULL)
                chart->info.difficulty = 3;
            else if (strstr(version, "Medium") != NULL ||
                     strstr(version, "Normal") != NULL)
                chart->info.difficulty = 2;
            else
                chart->info.difficulty = 1;
        } else if (strncmp(line, "AudioFilename:", 14) == 0) {
            const char *audio = line + 14;
            while (*audio == ' ') audio++;
            strncpy(chart->wav_path, audio, sizeof(chart->wav_path) - 1);
            chart->wav_path[sizeof(chart->wav_path) - 1] = '\0';

            char *dot = strrchr(chart->wav_path, '.');
            if (dot != NULL) strcpy(dot, ".wav");
        }

        if (strncmp(line, "[TimingPoints]", 14) == 0) {
            if (ff_fgets(line, sizeof(line), osu) != NULL) {
                strip_line(line);
                if (line[0] != '\0') {
                    char *comma = strchr(line, ',');
                    if (comma != NULL) {
                        double beat_ms = strtod(comma + 1, NULL);
                        if (beat_ms > 0.0)
                            chart->info.bpm =
                                (uint16_t)(60000.0 / beat_ms + 0.5);
                    }
                }
            }
        }
    }

    ff_fclose(osu);

    if (chart->wav_path[0] == '\0') {
        strncpy(chart->wav_path, chart->osu_path,
                sizeof(chart->wav_path) - 1);
        char *dot = strrchr(chart->wav_path, '.');
        if (dot != NULL) strcpy(dot, ".wav");
    }
}

static bool add_chart(const char *osu_path) {
    if (chart_count_value >= MAX_CHARTS) return false;

    chart_entry_t *chart = &charts[chart_count_value];
    memset(chart, 0, sizeof(*chart));
    strncpy(chart->osu_path, osu_path, sizeof(chart->osu_path) - 1);
    read_chart_header(chart);

    chart_count_value++;
    return true;
}

static bool scan_directory(const char *directory) {
    FF_FindData_t found;
    if (ff_findfirst(directory, &found) != 0) return false;

    do {
        if (found.pcFileName &&
            found.pcFileName[0] != '.' &&
            has_extension(found.pcFileName, ".osu")) {
            char path[96];
            if (directory[0] == '\0' || strcmp(directory, "/") == 0)
                snprintf(path, sizeof(path), "%s", found.pcFileName);
            else
                snprintf(path, sizeof(path), "%s/%s", directory, found.pcFileName);
            add_chart(path);
        }
    } while (ff_findnext(&found) == 0 && chart_count_value < MAX_CHARTS);

    return chart_count_value > 0;
}

bool chart_init(void) {
    chart_count_value = 0;

    if (f_mount(&fs, "0:", 1) == FR_OK) {
        scan_directory("");
        if (chart_count_value == 0) scan_directory("/charting");
    }

    return chart_count_value > 0;
}

int chart_count(void) {
    return chart_count_value;
}

bool chart_get_info(int index, chart_info_t *out) {
    if (index < 0 || index >= chart_count_value || out == NULL) return false;
    *out = charts[index].info;
    return true;
}

const char *chart_audio_path(int index) {
    if (index < 0 || index >= chart_count_value) return NULL;
    return charts[index].wav_path;
}

static uint8_t osu_x_to_lane(int x) {
    if (x < 128) return 0;
    if (x < 256) return 1;
    if (x < 384) return 2;
    return 3;
}

bool chart_open(int index) {
    chart_note_count = 0;
    chart_note_index = 0;

    if (index < 0 || index >= chart_count_value) return false;

    FF_FILE *osu = ff_fopen(charts[index].osu_path, "r");
    if (osu != NULL) {
        char line[192];
        bool hit_objects = false;

        while (ff_fgets(line, sizeof(line), osu) != NULL &&
               chart_note_count < MAX_CHART_NOTES) {
            char *trimmed = line;
            while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

            if (strncmp(trimmed, "[HitObjects]", 13) == 0) {
                hit_objects = true;
                continue;
            }
            if (!hit_objects || trimmed[0] == '\0' || trimmed[0] == '/' ||
                trimmed[0] == '[') {
                continue;
            }

            char *end;
            long x = strtol(trimmed, &end, 10);
            if (end == trimmed || *end != ',') continue;

            char *cursor = end + 1;
            strtol(cursor, &end, 10);
            if (*end != ',') continue;

            cursor = end + 1;
            long time_ms = strtol(cursor, &end, 10);
            if (*end != ',') continue;

            cursor = end + 1;
            long object_type = strtol(cursor, &end, 10);
            if (*end != ',') continue;

            uint16_t duration = 0;
            if ((object_type & 128) != 0) {
                cursor = end + 1;
                strtol(cursor, &end, 10);
                if (*end != ',') continue;

                cursor = end + 1;
                long end_time = strtol(cursor, &end, 10);
                if (end_time > time_ms)
                    duration = (uint16_t)(end_time - time_ms);
            }

            chart_notes[chart_note_count].lane = osu_x_to_lane((int)x);
            chart_notes[chart_note_count].time_ms = (uint32_t)time_ms;
            chart_notes[chart_note_count].duration_ms = duration;
            uint32_t note_end = (uint32_t)time_ms + duration;
            if (note_end > charts[index].info.length_ms)
                charts[index].info.length_ms = note_end;
            chart_note_count++;
        }

        ff_fclose(osu);
    }

    if (chart_note_count == 0) {
        return false;
    }

    return chart_note_count > 0;
}

bool chart_next_note(chart_note_t *out) {
    if (out == NULL || chart_note_index >= chart_note_count) return false;
    *out = chart_notes[chart_note_index++];
    return true;
}
