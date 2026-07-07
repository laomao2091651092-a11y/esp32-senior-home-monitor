#ifndef __S3_DISPLAY_H__
#define __S3_DISPLAY_H__

#ifdef __cplusplus
extern "C" {
#endif

// LCD显示初始化（只打印日志）
void s3_display_init(void);

// 启动LCD显示任务
void s3_display_start(void);

#ifdef __cplusplus
}
#endif

#endif
