#ifndef RTSP_PLAYBACK_H
#define RTSP_PLAYBACK_H

#include "video_player.h"
#include <SDL2/SDL.h>

int display_videos(Video_Player pPlayer[], int players_length,
                   SDL_Renderer *renderer);

#endif // RSTP_PLAYBACK_H
