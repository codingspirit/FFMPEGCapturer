/**
 * @file FFMPEGCapturer.h
 * @author Alex Li (lizhiqin46783937@live.com)
 * @brief
 * @version 0.1
 * @date 2022-05-15
 *
 * @copyright Copyright (c) 2022
 *
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef void *FFMPEGCapturerHandle;

/**
 * @brief Open selected v4l2 device.
 *
 * @param[in] pDeviceName v4l2 device to open. i.e. /dev/video0.
 * @return FFMPEGCapturerHandle Capturer Handle for further operations. It can
 * be NULL if error occurs.
 */
FFMPEGCapturerHandle ffmpegCapturerOpen(const char *pDeviceName);

/**
 * @brief Close capturer and release resources.
 *
 * @param[in] ppHandle Capturer Handle.
 */
void ffmpegGCapturerClose(FFMPEGCapturerHandle *ppHandle);

/**
 * @brief Get frame in sync mode(blocking).
 *
 * @param[in] pHandle Capturer Handle.
 * @param[in,out] pFrameDataBuffer Target frame data buffer.
 * @param[in] frameDataBufferSize Target frame data buffer size in bytes.
 * @param[out] pFrameSize Frame data size in bytes
 * @return int 0 or error code.
 */
int ffmpegCapturerSyncGetFrame(FFMPEGCapturerHandle pHandle,
                               void *pFrameDataBuffer,
                               const size_t frameDataBufferSize,
                               size_t *pFrameSize);

#ifdef __cplusplus
}
#endif
