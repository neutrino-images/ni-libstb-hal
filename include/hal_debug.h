#ifndef __HAL_DEBUG_H__
#define __HAL_DEBUG_H__

#define HAL_DEBUG_AUDIO     0
#define HAL_DEBUG_VIDEO     1
#define HAL_DEBUG_DEMUX     2
#define HAL_DEBUG_PLAYBACK  3
#define HAL_DEBUG_PWRMNGR   4
#define HAL_DEBUG_INIT      5
#define HAL_DEBUG_CA        6
#define HAL_DEBUG_RECORD    7
#define HAL_DEBUG_PLAYER    8
#define HAL_DEBUG_ALL       ((1<<9)-1)

extern int debuglevel;

#ifdef __cplusplus
extern "C" {
#endif
void _hal_debug(int facility, const void *, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
void _hal_info(int facility, const void *, const char *fmt, ...)  __attribute__((format(printf, 3, 4)));
#ifdef __cplusplus
}
#endif

void hal_debug_init(void);
void hal_set_threadname(const char *name);

#endif // __HAL_DEBUG_H__
