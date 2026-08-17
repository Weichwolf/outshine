/* THE ONE ENTRY POINT EVERY RENDER CORPUS'S HARNESS CALLS (board:1196). The scoring instrument is
 * shared because a case is decided the same way whoever authored the asset; the harness is not,
 * because a corpus is a suite and a suite is a folder. */
#ifndef SHARED_RENDER_RENDERCASE_H
#define SHARED_RENDER_RENDERCASE_H

[[nodiscard]] int ScoreRenderCase(int argc, char **argv);

#endif
