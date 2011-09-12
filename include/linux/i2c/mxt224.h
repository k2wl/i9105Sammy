/*
 * AT42QT602240/ATMXT224 Touchscreen driver
 *
 * Copyright (C) 2012 Samsung Electronics Co.Ltd
 * This touch driver is based on mxt224_u1.h
 * Author: Taeheon Kim <th908.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __MXT224_H__
#define __MXT224_H__

#define MXT224_DEV_NAME "Atmel MXT224"

#define MXT224_MAX_MT_FINGERS		10

#define MAX_USING_FINGER_NUM	10

#define MXT224_AUTOCAL_WAIT_TIME		2000

#define SEC_TSP_FACTORY_TEST

#define DRIVER_FILTER
enum {
	RESERVED_T0 = 0,
	RESERVED_T1,
	DEBUG_DELTAS_T2,
	DEBUG_REFERENCES_T3,
	DEBUG_SIGNALS_T4,
	GEN_MESSAGEPROCESSOR_T5,
	GEN_COMMANDPROCESSOR_T6,
	GEN_POWERCONFIG_T7,
	GEN_ACQUISITIONCONFIG_T8,
	TOUCH_MULTITOUCHSCREEN_T9,
	TOUCH_SINGLETOUCHSCREEN_T10,
	TOUCH_XSLIDER_T11,
	TOUCH_YSLIDER_T12,
	TOUCH_XWHEEL_T13,
	TOUCH_YWHEEL_T14,
	TOUCH_KEYARRAY_T15,
	PROCG_SIGNALFILTER_T16,
	PROCI_LINEARIZATIONTABLE_T17,
	SPT_COMCONFIG_T18,
	SPT_GPIOPWM_T19,
	PROCI_GRIPFACESUPPRESSION_T20,
	RESERVED_T21,
	PROCG_NOISESUPPRESSION_T22,
	TOUCH_PROXIMITY_T23,
	PROCI_ONETOUCHGESTUREPROCESSOR_T24,
	SPT_SELFTEST_T25,
	DEBUG_CTERANGE_T26,
	PROCI_TWOTOUCHGESTUREPROCESSOR_T27,
	SPT_CTECONFIG_T28,
	SPT_GPI_T29,
	SPT_GATE_T30,
	TOUCH_KEYSET_T31,
	TOUCH_XSLIDERSET_T32,
	RESERVED_T33,
	GEN_MESSAGEBLOCK_T34,
	SPT_GENERICDATA_T35,
	RESERVED_T36,
	DEBUG_DIAGNOSTIC_T37,
	SPT_USERDATA_T38,
	SPARE_T39,
	PROCI_GRIPSUPPRESSION_T40,
	SPARE_T41,
	PROCI_TOUCHSUPPRESSION_T42,
	SPARE_T43,
	SPARE_T44,
	SPARE_T45,
	SPT_CTECONFIG_T46,
	PROCI_STYLUS_T47,
	PROCG_NOISESUPPRESSION_T48,
	SPARE_T49,
	SPARE_T50,
	RESERVED_T255 = 255,
};
struct t22_freq_table_config_t {
	u8 fherr_setting;
	u8 fherr_cnt;
	u8 fherr_num;
	u8 t9_blen_for_fherr;
	u8 t9_blen_for_fherr_cnt;
	u8 t9_thr_for_fherr;
	u8 t9_movfilter_for_fherr;
	u8 t22_noisethr_for_fherr;
	u8 t22_freqscale_for_fherr;
	u8 freq_for_fherr1[5];
	u8 freq_for_fherr2[5];
	u8 freq_for_fherr3[5];
	u8 freq_for_fherr4[5];
};

struct t48_median_config_t {
	bool median_on_flag;
	bool mferr_setting;
	u8 mferr_count;
	u8 t46_actvsyncsperx_for_mferr;
	u8 t48_mfinvlddiffthr_for_mferr;
	u8 t48_mferrorthr_for_mferr;
	u8 t48_thr_for_mferr;
	u8 t48_movfilter_for_mferr;
};
struct object_t {
	u8 object_type;
	u16 i2c_address;
	u8 size;
	u8 instances;
	u8 num_report_ids;
} __packed;

struct finger_info {
	s16 x;
	s16 y;
	s16 z;
	u16 w;
	int16_t component;
};
struct mxt224_callbacks {
	void (*inform_charger)(struct mxt224_callbacks *, int mode);
};

struct mxt224_platform_data {
	int max_finger_touches;
	const u8 **config;
	const u8 **config_e;
	const u8 *t48_config_batt_e;
	const u8 *t48_config_chrg_e;
	int gpio_read_done;
	int min_x;
	int max_x;
	int min_y;
	int max_y;
	int min_z;
	int max_z;
	int min_w;
	int max_w;
	u8 atchcalst;
	u8 atchcalsthr;
	u8 tchthr_batt;
	u8 tchthr_charging;
	u8 tchthr_batt_init;
	u8 noisethr_batt;
	u8 noisethr_charging;
	u8 movfilter_batt;
	u8 movfilter_charging;
	u8 atchcalst_e;
	u8 atchcalsthr_e;
	u8 tchthr_batt_e;
	u8 tchthr_charging_e;
	u8 calcfg_batt_e;
	u8 calcfg_charging_e;
	u8 atchfrccalthr_e;
	u8 atchfrccalratio_e;
	u8 chrgtime_batt_e;
	u8 chrgtime_charging_e;
	u8 blen_batt_e;
	u8 blen_charging_e;
	u8 movfilter_batt_e;
	u8 movfilter_charging_e;
	u8 actvsyncsperx_e;
	u8 nexttchdi_e;
	void (*power_on) (void);
	void (*power_off) (void);
	void (*register_cb)(struct mxt224_callbacks *);
	void (*read_ta_status) (void *);
	void (*idle_on) (void);
	void (*idle_off) (void);
};

#ifdef SEC_TSP_FACTORY_TEST
#define TSP_BUF_SIZE 1024

#define TSP_CMD_STR_LEN 32
#define TSP_CMD_RESULT_STR_LEN 512
#define TSP_CMD_PARAM_NUM 8

#define TSP_CMD(name, func) .cmd_name = name, .cmd_func = func

struct tsp_cmd {
	struct list_head	list;
	const char	*cmd_name;
	void	(*cmd_func)(void *device_data);
};

static void get_config_ver(void *device_data);
static void get_fw_ver_bin(void *device_data);
static void get_fw_ver_ic(void *device_data);
static void get_threshold(void *device_data);
static void get_chip_vendor(void *device_data);
static void get_chip_name(void *device_data);
static void get_reference(void *device_data);
static void get_delta(void *device_data);
static void get_x_num(void *device_data);
static void get_y_num(void *device_data);
static void run_reference_read(void *device_data);
static void run_delta_read(void *device_data);
static void not_support_cmd(void *device_data);

#endif

struct mxt224_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct mxt224_platform_data *pdata;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	u8 family_id;
	u32 finger_mask;
	struct object_t *objects;
	u8 objects_len;
	u8 tsp_version;
	const u8 *power_cfg;
	const u8 *noise_suppression_cfg_ta;
	const u8 *noise_suppression_cfg;
	u8 finger_type;
	u16 msg_proc;
	u16 cmd_proc;
	u16 msg_object_size;
	u32 x_dropbits:2;
	u32 y_dropbits:2;
	int num_fingers;
	struct finger_info fingers[MXT224_MAX_MT_FINGERS];
	struct t22_freq_table_config_t freq_table;
	struct t48_median_config_t noise_median;

	int mxt224_enabled;
	bool g_debug_switch;
	u8 mxt_version_disp;
	u8 tsp_version_disp;
	int optiacl_gain;
	int firm_status_data;
	bool lock_status;
	int touch_state;		/* 1:release, 2:press, 3:others */
	int palm_chk_flag;
	bool ta_status_pre;
	int errcondition;
	int threshold;
	int threshold_e;
	bool boot_or_resume;	/*1: boot_or_resume,0: others */
	bool sleep_mode_flag;
	char *tsp_config_version;
	bool median_err_flag;
	int touch_is_pressed_arr[MAX_USING_FINGER_NUM];
	struct completion	init_done;
	struct mutex	lock;
	bool enabled;
#ifdef DRIVER_FILTER
	bool gbfilter;
#endif
	unsigned int qt_time_point_freq;
	unsigned int qt_time_diff_freq;
	unsigned int qt_time_point;
	unsigned int qt_time_diff;
	unsigned int qt_timer_state;
	unsigned int good_check_flag;
	unsigned int not_yet_count;
	u8 cal_check_flag;
	u8 doing_calibration_flag;
	unsigned char test_node[5];
	uint16_t qt_refrence_node[209];
	uint16_t qt_delta_node[209];
	int index_delta;
	int index_reference;
	struct mxt224_callbacks callbacks;
	
#if defined(SEC_TSP_FACTORY_TEST)
	struct list_head			cmd_list_head;
	unsigned char cmd_state;
	char			cmd[TSP_CMD_STR_LEN];
	int			cmd_param[TSP_CMD_PARAM_NUM];
	char			cmd_result[TSP_CMD_RESULT_STR_LEN];
	struct mutex			cmd_lock;
	bool			cmd_is_running;

	bool ft_flag;
#endif				/* SEC_TSP_FACTORY_TEST */

};

enum {
	QT_PAGE_UP = 0x01,
	QT_PAGE_DOWN = 0x02,
	QT_DELTA_MODE = 0x10,
	QT_REFERENCE_MODE = 0x11,
	QT_CTE_MODE = 0x31
};

enum {
	ERR_RTN_CONDITION_T9,
	ERR_RTN_CONDITION_T48,
	ERR_RTN_CONDITION_IDLE
};

#define ERR_RTN_CONDITION_MAX	(ERR_RTN_CONDITION_IDLE + 1)


extern struct class *sec_class;
#endif /* __MXT224_H__ */
