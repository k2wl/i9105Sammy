/*****************************************************************************
*  Copyright 2012 Broadcom Corporation.  All rights reserved.
*
*  Unless you and Broadcom execute a separate written software license
*  agreement governing use of this software, this software is licensed to you
*  under the terms of the GNU General Public License version 2, available at
*  http://www.gnu.org/licenses/old-license/gpl-2.0.html (the "GPL").
*
*  Notwithstanding the above, under no circumstances may you combine this
*  software in any way with any other Broadcom software provided under a
*  license other than the GPL, without Broadcom's express prior written
*  consent.
*
*****************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/bug.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/power_supply.h>

#include <linux/mfd/bcmpmu.h>
#include <linux/spa_power.h>

#define BCMPMU_PRINT_ERROR (1U << 0)
#define BCMPMU_PRINT_INIT (1U << 1)
#define BCMPMU_PRINT_FLOW (1U << 2)
#define BCMPMU_PRINT_DATA (1U << 3)

#ifdef CONFIG_CHARGER_BCMPMU_SPA
#define BCMPMU_CHRGR_FIFO_LENGTH	8 //need to set 2^n
#define CHRGR_FIFO_HEAD(fifo)	(fifo.head = ((fifo.head+1) & (fifo.length-1)))
#define CHRGR_FIFO_TAIL(fifo)	(fifo.tail = ((fifo.tail+1) & (fifo.length-1)))
#define CHRGR_FIFO_EMPTY(fifo)	((fifo.head == fifo.tail) && !fifo.fifo_full)

struct bcmpmu_chrgr_fifo {
	unsigned char		head;
	unsigned char		tail;
	unsigned char		length;
	bool			fifo_full;
	int 			data[BCMPMU_CHRGR_FIFO_LENGTH];
};
#endif

#ifdef CONFIG_BQ24272_CHARGER
extern bq24272_start_chg();
extern bq24272_stop_chg();
#endif

static int debug_mask = BCMPMU_PRINT_ERROR | BCMPMU_PRINT_INIT;
/* static int debug_mask = 0xFF; */
#define pr_chrgr(debug_level, args...) \
	do { \
		if (debug_mask & BCMPMU_PRINT_##debug_level) { \
			pr_info(args); \
		} \
	} while (0)

static bool chrgr_usb_en = false;

struct bcmpmu_chrgr {
	struct bcmpmu *bcmpmu;
	struct power_supply chrgr;
	struct notifier_block nb;
	struct delayed_work work;
	struct delayed_work chrgr_enable;
	struct bcmpmu_spa_event_fifo spafifo;
	struct bcmpmu_chrgr_fifo chrgrfifo;	
	struct mutex lock;
	struct mutex chrgrlock;
	int eoc;
	int icc_fc;
	int status;
	enum bcmpmu_chrgr_type_t chrgrtype;
	int usb_ov;
	int mb_ov;
	int temp_out;
	int chrgrcurr;
	int chrgrcurr_max;
	int chrgcurrsetting;
};

static enum power_supply_property bcmpmu_chrgr_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_BATT_TEMP_ADC,
};

static int bcmpmu_chrgr_usb_en(struct bcmpmu *bcmpmu, int en);

static int set_icc_fc(struct bcmpmu *bcmpmu, int curr)
{
	int ret = 0;
	int retry_cnt;
	enum bcmpmu_chrgr_fc_curr_t setting;
	struct bcmpmu_chrgr *pchrgr = bcmpmu->chrgrinfo;

	if (curr < 50)
		setting = PMU_CHRGR_CURR_50;
	else if (curr > 1500)
		setting = PMU_CHRGR_CURR_1500;
	else if (curr > 1000)
		setting = (curr - 1000)/100 + PMU_CHRGR_CURR_1000;
	else
		setting = (curr - 50)/50;

	if (setting == pchrgr->chrgcurrsetting)
		return ret;

	ret = -1;
	retry_cnt = 0;
	while ((ret != 0) && (retry_cnt < 20)) {
		ret = bcmpmu->write_dev(bcmpmu,
			PMU_REG_CHRGR_ICC_FC,
			bcmpmu_chrgr_icc_fc_settings[setting],
			PMU_BITMASK_ALL);
		if (ret == 0) {
			pchrgr->chrgcurrsetting	= setting;
			pr_chrgr(FLOW, "%s, setting=%d, ret=%d\n",
				__func__, setting, ret);
		} else {
			pr_chrgr(ERROR, "%s, failed, retry=%d\n",
					__func__, retry_cnt);
				retry_cnt++;
		}
	}

	return ret;
}

static int bcmpmu_set_icc_fc(struct bcmpmu *bcmpmu, int curr)
{
	int ret;
	struct bcmpmu_chrgr *pchrgr = bcmpmu->chrgrinfo;
	pchrgr->chrgrcurr = curr;
	ret = set_icc_fc(bcmpmu, curr);
	return ret;
}

static int bcmpmu_set_icc_fc_lmt(struct bcmpmu *bcmpmu, int curr)
{
	int ret = 0;
	struct bcmpmu_chrgr *pchrgr = bcmpmu->chrgrinfo;
	pchrgr->chrgrcurr_max = curr;
	if (curr < 0)
		ret = set_icc_fc(bcmpmu, pchrgr->chrgrcurr);
	else if (curr < pchrgr->chrgrcurr)
		ret = set_icc_fc(bcmpmu, curr);
	pr_chrgr(FLOW, "%s, curr=%d, ret=%d\n", __func__, curr, ret);
	return ret;
}

static int bcmpmu_set_eoc(struct bcmpmu *bcmpmu, int curr)
{
	int ret;
	struct bcmpmu_chrgr *pchrgr = bcmpmu->chrgrinfo;
	pchrgr->eoc = curr;

	if(curr > 250)
		curr = 250;
	ret = bcmpmu->write_dev(bcmpmu,
				PMU_REG_CHRGR_EOC,
				curr,
				PMU_BITMASK_ALL);
	return ret;
}

static int bcmpmu_chrgr_usb_en(struct bcmpmu *bcmpmu, int en)
{
	struct bcmpmu_chrgr *pchrgr = bcmpmu->chrgrinfo;

	int ret,val;
	int timeout = 50;

	printk(KERN_INFO "%s: EN :%d!!!!\n", __func__, en);

	if (en == 0) {
		ret = bcmpmu->write_dev(bcmpmu,
			PMU_REG_CHRGR_USB_EN,
			0,
			bcmpmu->regmap[PMU_REG_CHRGR_USB_EN].mask);
		chrgr_usb_en = false;
	}
	else{
		while (timeout) {
			if(!pchrgr->spafifo.chrg_on)
				break;
			mdelay(100);
			// P_UBPD_INT
			bcmpmu->read_dev(bcmpmu, PMU_REG_ENV2,
					&val, PMU_BITMASK_ALL);

			if(val&0x04)
			{
				ret = bcmpmu->write_dev(bcmpmu,
					PMU_REG_CHRGR_USB_EN,
			bcmpmu->regmap[PMU_REG_CHRGR_USB_EN].mask,
			bcmpmu->regmap[PMU_REG_CHRGR_USB_EN].mask);
				break;
			}
			timeout--;
		}

		if(timeout) {
		timeout = 10;
		while (timeout) {
				if(!pchrgr->spafifo.chrg_on)
				break;
			mdelay(100);
			// USB_VALID
			bcmpmu->read_dev(bcmpmu, PMU_REG_ENV2,
					&val, PMU_BITMASK_ALL);

			if(val&0x80){
				break;
			}
			timeout--;
		}
		}

		bcmpmu->read_dev(bcmpmu, PMU_REG_CHRGR_USB_EN,
				&val, PMU_BITMASK_ALL);
		if(val == 0x01) {
		chrgr_usb_en = true;
			printk(KERN_INFO "%s: chrgr_usb_en success\n", __func__);
		}
		else
			printk(KERN_INFO "%s: chrgr_usb_en failed\n", __func__);


	}
	return ret;
}

static int bcmpmu_ntcht_rise_set(struct bcmpmu *bcmpmu, int val)
{
	unsigned int hi_val, lo_val;
	int nbytes = 0;
	hi_val = (((val >> 8) & 0xFF)) << 6;
	lo_val = (val & 0xFF);

	if (bcmpmu->write_dev) {
		nbytes = bcmpmu->write_dev(bcmpmu,
					   PMU_REG_NTCHT_RISE_LO,
					   lo_val,
		bcmpmu->regmap[PMU_REG_NTCHT_RISE_LO].mask);

		nbytes += bcmpmu->write_dev(bcmpmu,
					    PMU_REG_NTCHT_RISE_HI,
					    hi_val,
		bcmpmu->regmap[PMU_REG_NTCHT_RISE_HI].mask);
	}
	return nbytes;
}

static int bcmpmu_ntcht_fall_set(struct bcmpmu *bcmpmu, int val)
{
	unsigned int hi_val, lo_val;
	int nbytes = 0;
	hi_val = (((val >> 8) & 0xFF)) << 4;
	lo_val = (val & 0xFF);

	if (bcmpmu->write_dev) {
		nbytes = bcmpmu->write_dev(bcmpmu,
					   PMU_REG_NTCHT_FALL_LO,
					   lo_val,
		bcmpmu->regmap[PMU_REG_NTCHT_FALL_LO].mask);

		nbytes += bcmpmu->write_dev(bcmpmu,
					    PMU_REG_NTCHT_FALL_HI,
					    hi_val,
		bcmpmu->regmap[PMU_REG_NTCHT_FALL_HI].mask);
	}
	return nbytes;
}


static int bcmpmu_ntcct_rise_set(struct bcmpmu *bcmpmu, int val)
{
	unsigned int hi_val, lo_val;
	int nbytes = 0;
	hi_val = (((val >> 8) & 0xFF)) << 2;
	lo_val = (val & 0xFF);

	if (bcmpmu->write_dev) {
		nbytes = bcmpmu->write_dev(bcmpmu,
					   PMU_REG_NTCCT_RISE_LO,
					   lo_val,
		bcmpmu->regmap[PMU_REG_NTCCT_RISE_LO].mask);

		nbytes += bcmpmu->write_dev(bcmpmu,
					    PMU_REG_NTCCT_RISE_HI,
					    hi_val,
		bcmpmu->regmap[PMU_REG_NTCCT_RISE_HI].mask);
	}
	return nbytes;
}

static int bcmpmu_ntcct_fall_set(struct bcmpmu *bcmpmu, int val)
{
	unsigned int hi_val, lo_val;
	int nbytes = 0;
	hi_val = ((val >> 8) & 0xFF);
	lo_val = (val & 0xFF);

	if (bcmpmu->write_dev) {
		nbytes = bcmpmu->write_dev(bcmpmu,
					   PMU_REG_NTCCT_FALL_LO,
					   lo_val,
		bcmpmu->regmap[PMU_REG_NTCCT_FALL_LO].mask);

		nbytes += bcmpmu->write_dev(bcmpmu,
					    PMU_REG_NTCCT_FALL_HI,
					    hi_val,
		bcmpmu->regmap[PMU_REG_NTCCT_FALL_HI].mask);
	}
	return nbytes;
}

#ifdef CONFIG_MFD_BCMPMU_DBG
static ssize_t
dbgmsk_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	return sprintf(buf, "debug_mask is %x\n", debug_mask);
}

static ssize_t
dbgmsk_set(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t count)
{
	int ret;
	int val;
	ret = kstrtoint(buf, 0, &val);
	if (ret)
		return ret;
	if (val > 0xFF || val == 0)
		return -EINVAL;
	debug_mask = val;
	return count;
}

static ssize_t
bcmpmu_dbg_show_icc_fc(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int val;
	struct bcmpmu *bcmpmu = dev->platform_data;
	bcmpmu->read_dev(bcmpmu, PMU_REG_CHRGR_ICC_FC,
			&val, PMU_BITMASK_ALL);
	return sprintf(buf, "0x%X\n", val);
}

static ssize_t
bcmpmu_dbg_set_icc_fc(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t n)
{
	struct bcmpmu *bcmpmu = dev->platform_data;
	int ret;
	int val;
	ret = kstrtoint(buf, 0, &val);
	if (ret)
		return ret;
	bcmpmu_set_icc_fc(bcmpmu, (int)val);
	return n;
}

static ssize_t
bcmpmu_dbg_show_eoc(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int val;
	struct bcmpmu *bcmpmu = dev->platform_data;
	bcmpmu->read_dev(bcmpmu, PMU_REG_CHRGR_EOC,
			&val, PMU_BITMASK_ALL);
	return sprintf(buf, "0x%X\n", val);
}

static ssize_t
bcmpmu_dbg_set_eoc(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t n)
{
	struct bcmpmu *bcmpmu = dev->platform_data;
	int ret;
	int val;
	ret = kstrtoint(buf, 0, &val);
	if (ret)
		return ret;
	bcmpmu_set_eoc(bcmpmu, (int)val);
	return n;
}

static ssize_t
bcmpmu_dbg_usb_en(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t n)
{
	struct bcmpmu *bcmpmu = dev->platform_data;
	int ret;
	int val;
	ret = kstrtoint(buf, 0, &val);
	if (ret)
		return ret;
	bcmpmu_chrgr_usb_en(bcmpmu, (int)val);
	return n;
}

static ssize_t
bcmpmu_dbg_show_driver_data(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct bcmpmu *bcmpmu = dev->platform_data;
	struct bcmpmu_chrgr *pchrgr = bcmpmu->chrgrinfo;
	ssize_t count = 0;

	count += sprintf(buf + count, "eoc = %d\n", pchrgr->eoc);
	count += sprintf(buf + count, "icc_fc = %d\n", pchrgr->icc_fc);
	count += sprintf(buf + count, "status = %d\n", pchrgr->status);
	count += sprintf(buf + count, "chrgr type = %d\n", pchrgr->chrgrtype);
	count += sprintf(buf + count, "usb_ov = %d\n", pchrgr->usb_ov);
	count += sprintf(buf + count, "mb_ov = %d\n", pchrgr->mb_ov);
	count += sprintf(buf + count, "temp_out = %d\n", pchrgr->temp_out);

	return count;
}

static ssize_t
bcmpmu_dbg_set_prop(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t n)
{
	struct power_supply *ps;
	union power_supply_propval propval;
	enum power_supply_property prop;
	struct bcmpmu *bcmpmu = dev->platform_data;
	struct bcmpmu_chrgr *pchrgr = bcmpmu->chrgrinfo;
	sscanf(buf, "%d %d", (int *)&prop, (int *)&propval.intval);

	ps = power_supply_get_by_name((char *)pchrgr->chrgr.name);
	if (ps == 0) {
		pr_chrgr(ERROR, "%s, ps not found\n", __func__);
		return n;
	}
	ps->set_property(ps, prop, &propval);
	pr_chrgr(INIT, "%s, setting prop, prop=%d, val=%d\n",
		__func__, (int)prop, (int)propval.intval);
	return n;
}

static ssize_t
bcmpmu_dbg_get_prop(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t n)
{
	struct power_supply *ps;
	union power_supply_propval propval;
	enum power_supply_property prop;
	struct bcmpmu *bcmpmu = dev->platform_data;
	struct bcmpmu_chrgr *pchrgr = bcmpmu->chrgrinfo;
	sscanf(buf, "%d", (int *)&prop);

	ps = power_supply_get_by_name((char *)pchrgr->chrgr.name);
	if (ps == 0) {
		pr_chrgr(ERROR, "%s, ps not found\n", __func__);
		return n;
	}
	ps->get_property(ps, prop, &propval);
	pr_chrgr(INIT, "%s, getting prop, prop=%d, val=%d\n",
		__func__, (int)prop, (int)propval.intval);
	return n;
}

static DEVICE_ATTR(dbgmsk, 0644, dbgmsk_show, dbgmsk_set);
static DEVICE_ATTR(icc_fc, 0644, bcmpmu_dbg_show_icc_fc, bcmpmu_dbg_set_icc_fc);
static DEVICE_ATTR(eoc, 0644, bcmpmu_dbg_show_eoc, bcmpmu_dbg_set_eoc);
static DEVICE_ATTR(usb_en, 0644, NULL, bcmpmu_dbg_usb_en);
static DEVICE_ATTR(drvdata, 0644, bcmpmu_dbg_show_driver_data, NULL);
static DEVICE_ATTR(getprop, 0644, NULL, bcmpmu_dbg_get_prop);
static DEVICE_ATTR(setprop, 0644, NULL, bcmpmu_dbg_set_prop);

static struct attribute *bcmpmu_chrgr_attrs[] = {
	&dev_attr_dbgmsk.attr,
	&dev_attr_icc_fc.attr,
	&dev_attr_eoc.attr,
	&dev_attr_usb_en.attr,
	&dev_attr_drvdata.attr,
	&dev_attr_setprop.attr,
	&dev_attr_getprop.attr,
	NULL
};

static const struct attribute_group bcmpmu_chrgr_attr_group = {
	.attrs = bcmpmu_chrgr_attrs,
};
#endif

static int bcmpmu_chrgr_set_property(struct power_supply *ps,
		enum power_supply_property prop,
		const union power_supply_propval *propval)
{
	int ret = 0;
	struct bcmpmu_chrgr *pchrgr = container_of(ps,
		struct bcmpmu_chrgr, chrgr);
	struct bcmpmu *bcmpmu = pchrgr->bcmpmu;
	struct bcmpmu_platform_data *pdata = pchrgr->bcmpmu->pdata;

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		pchrgr->status = propval->intval;
#if defined(CONFIG_BQ24272_CHARGER)

	if(propval->intval == POWER_SUPPLY_STATUS_CHARGING)
		bq24272_start_chg();
	else
		bq24272_stop_chg();

#else
		if (!pchrgr->chrgrfifo.fifo_full) {
				mutex_lock(&pchrgr->chrgrlock);
				pchrgr->chrgrfifo.data[pchrgr->chrgrfifo.head] = (propval->intval == POWER_SUPPLY_STATUS_CHARGING)?1:0;
				//CHRGR_FIFO_HEAD[pchrgr->chrgrfifo];
				pchrgr->chrgrfifo.head = ((pchrgr->chrgrfifo.head+1)
								 & (pchrgr->chrgrfifo.length-1));

				if(pchrgr->chrgrfifo.head == pchrgr->chrgrfifo.tail)
					pchrgr->chrgrfifo.fifo_full = true;
	    		mutex_unlock(&pchrgr->chrgrlock);

				schedule_delayed_work(&pchrgr->chrgr_enable,msecs_to_jiffies((propval->intval == POWER_SUPPLY_STATUS_CHARGING)?1200:0));
		} 
		else {
			cancel_delayed_work_sync(&pchrgr->chrgr_enable);
			mutex_lock(&pchrgr->chrgrlock);			
			pchrgr->chrgrfifo.head = 0;
			pchrgr->chrgrfifo.tail = 0;
			pchrgr->chrgrfifo.fifo_full = false;

			pchrgr->chrgrfifo.data[pchrgr->chrgrfifo.head] = (propval->intval == POWER_SUPPLY_STATUS_CHARGING)?1:0;
			//CHRGR_FIFO_HEAD[pchrgr->chrgrfifo];
			pchrgr->chrgrfifo.head = ((pchrgr->chrgrfifo.head+1)
							 & (pchrgr->chrgrfifo.length-1));
			mutex_unlock(&pchrgr->chrgrlock);
			schedule_delayed_work(&pchrgr->chrgr_enable,msecs_to_jiffies((propval->intval == POWER_SUPPLY_STATUS_CHARGING)?1200:0));
		}
#endif		
		blocking_notifier_call_chain(
			&pchrgr->bcmpmu->event[BCMPMU_CHRGR_EVENT_CHRG_STATUS].
			notifiers, BCMPMU_CHRGR_EVENT_CHRG_STATUS,
			(void *)&pchrgr->status);
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		pchrgr->icc_fc = propval->intval;
		bcmpmu_set_icc_fc(bcmpmu, (int)propval->intval);
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		pchrgr->eoc = propval->intval;
		if (pdata->support_hw_eoc)
			bcmpmu_set_eoc(bcmpmu, (int)propval->intval);
		else if (bcmpmu->fg_set_eoc)
			bcmpmu->fg_set_eoc(bcmpmu, pchrgr->eoc);
		else
			ret = -ENODATA;
		break;

	case POWER_SUPPLY_PROP_CAPACITY:
		if ((propval->intval == 1) &&
			(bcmpmu->em_reset))
			bcmpmu->em_reset(bcmpmu);
		else
			ret = -EINVAL;

		break;

	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int bcmpmu_chrgr_get_property(struct power_supply *ps,
	enum power_supply_property prop,
	union power_supply_propval *propval)
{
	int ret = 0;
	struct bcmpmu_chrgr *pchrgr = container_of(ps,
		struct bcmpmu_chrgr, chrgr);
	struct bcmpmu *bcmpmu = pchrgr->bcmpmu;
	struct bcmpmu_adc_req req;

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		propval->intval = pchrgr->status;
		break;

	case POWER_SUPPLY_PROP_TYPE:
		if ((pchrgr->chrgrtype == PMU_CHRGR_TYPE_NONE) ||
			(pchrgr->chrgrtype >= PMU_CHRGR_TYPE_MAX))
			propval->intval = POWER_SUPPLY_TYPE_BATTERY;
		else if (pchrgr->chrgrtype == PMU_CHRGR_TYPE_SDP)
			propval->intval = POWER_SUPPLY_TYPE_USB;
		else if (pchrgr->chrgrtype == PMU_CHRGR_TYPE_CDP)
			propval->intval = POWER_SUPPLY_TYPE_USB_CDP;
		else if (pchrgr->chrgrtype == PMU_CHRGR_TYPE_DCP)
			propval->intval = POWER_SUPPLY_TYPE_USB_DCP;
		else
			propval->intval = POWER_SUPPLY_TYPE_BATTERY;

		break;
	case POWER_SUPPLY_PROP_TEMP:
		req.sig = PMU_ADC_NTC;
		req.tm = PMU_ADC_TM_HK;
		req.flags = PMU_ADC_RAW_AND_UNIT;
		if (bcmpmu->adc_req) {
			bcmpmu->adc_req(bcmpmu, &req);
			propval->intval = req.cnv;
		} else
			ret = -ENODATA;
		break;

	case POWER_SUPPLY_PROP_BATT_TEMP_ADC:
		req.sig = PMU_ADC_NTC;
		req.tm = PMU_ADC_TM_HK;
		req.flags = PMU_ADC_RAW_AND_UNIT;
		if (bcmpmu->adc_req) {
			bcmpmu->adc_req(bcmpmu, &req);
			propval->intval = req.raw;
		} else
			ret = -ENODATA;
		break;

	case POWER_SUPPLY_PROP_CAPACITY:
		if (bcmpmu->fg_get_capacity)
			propval->intval =
				bcmpmu->fg_get_capacity(bcmpmu);
		else
			ret = -ENODATA;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		req.sig = PMU_ADC_VMBATT;
		req.tm = PMU_ADC_TM_HK;
		req.flags = PMU_ADC_RAW_AND_UNIT;
		if (bcmpmu->adc_req) {
			bcmpmu->adc_req(bcmpmu, &req);
			propval->intval = req.cnv;
		} else
			ret = -ENODATA;
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		req.sig = PMU_ADC_BSI;
		req.tm = PMU_ADC_TM_HK;
		req.flags = PMU_ADC_RAW_AND_UNIT;
		if (bcmpmu->adc_req) {
			bcmpmu->adc_req(bcmpmu, &req);
			propval->intval = (req.cnv == 0x3FF) ? 0 : 1;
		} else
			ret = -ENODATA;
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:

	default:
		ret = -ENODATA;
		break;
	}
	return ret;
}

static int bcmpmu_chrgr_event_handler(struct notifier_block *nb,
		unsigned long event, void *para)
{
	struct bcmpmu_chrgr *pchrgr = container_of(nb, struct bcmpmu_chrgr, nb);
	struct bcmpmu_platform_data *pdata = pchrgr->bcmpmu->pdata;
	struct bcmpmu *bcmpmu = pchrgr->bcmpmu;

	int data;

	switch (event) {
	case BCMPMU_CHRGR_EVENT_CHGR_DETECTION:
		pchrgr->chrgrtype = *(enum bcmpmu_chrgr_type_t *)para;
		pr_chrgr(FLOW, "%s, chrgr type=%d\n",
			__func__, pchrgr->chrgrtype);

		if ((pchrgr->chrgrtype == PMU_CHRGR_TYPE_NONE) ||
			(pchrgr->chrgrtype >= PMU_CHRGR_TYPE_MAX))
			data = POWER_SUPPLY_TYPE_BATTERY;
		else if (pchrgr->chrgrtype == PMU_CHRGR_TYPE_SDP)
			data = POWER_SUPPLY_TYPE_USB;
		else if (pchrgr->chrgrtype == PMU_CHRGR_TYPE_CDP)
			data = POWER_SUPPLY_TYPE_USB_CDP;
		else if (pchrgr->chrgrtype == PMU_CHRGR_TYPE_DCP)
			data = POWER_SUPPLY_TYPE_USB_DCP;
		else
			data = POWER_SUPPLY_TYPE_BATTERY;



		if (pdata->piggyback_notify)
			pdata->piggyback_notify(
				BCMPMU_CHRGR_EVENT_CHGR_DETECTION, data);
		break;

	case BCMPMU_USB_EVENT_IN:
		printk(KERN_INFO "%s: PMU_IRQ_USBINS called.\n", __func__);
		break;

	case BCMPMU_CHRGR_UBPD_CHG_F:
		printk(KERN_INFO "%s: BCMPMU_CHRGR_UBPD_CHG_F called.\n", __func__);
		if(chrgr_usb_en){
			mdelay(50);
			bcmpmu_set_icc_fc(bcmpmu, pchrgr->chrgrcurr);
		}
		break;

	default:
		break;
	}
	return 0;
}

static void bcmpmu_chrgr_isr(enum bcmpmu_irq irq, void *data)
{
	struct bcmpmu_chrgr *pchrgr = (struct bcmpmu_chrgr *)data;
	struct bcmpmu_platform_data *pdata = pchrgr->bcmpmu->pdata;
	pr_chrgr(FLOW, "%s, interrupt %d\n", __func__, irq);

	switch (irq) {
	case PMU_IRQ_MBTEMPHIGH:
		pchrgr->temp_out = 1;
		if (pdata->piggyback_notify)
			pdata->piggyback_notify(
				BCMPMU_CHRGR_EVENT_MBTEMP, 1);
		break;
	case PMU_IRQ_MBTEMPLOW:
		pchrgr->temp_out = 1;
		if (pdata->piggyback_notify)
			pdata->piggyback_notify(
				BCMPMU_CHRGR_EVENT_MBTEMP, 1);
		break;
	case PMU_IRQ_CHGERRDIS:
		if (pdata->piggyback_notify) {
			if (pchrgr->temp_out == 1)
				pdata->piggyback_notify(
					BCMPMU_CHRGR_EVENT_MBTEMP, 0);
			if (pchrgr->mb_ov == 1)
				pdata->piggyback_notify(
					BCMPMU_CHRGR_EVENT_MBOV, 0);
			if (pchrgr->usb_ov == 1)
				pdata->piggyback_notify(
					BCMPMU_CHRGR_EVENT_USBOV, 0);
		}
		pchrgr->temp_out = 0;
		pchrgr->mb_ov = 0;
		pchrgr->usb_ov = 0;
		break;
	case PMU_IRQ_MBOV:
		pchrgr->mb_ov = 1;
		if (pdata->piggyback_notify)
			pdata->piggyback_notify(
				BCMPMU_CHRGR_EVENT_MBOV, 1);
		break;
	case PMU_IRQ_MBOV_DIS:
		if ((pchrgr->mb_ov == 1) && (pdata->piggyback_notify))
			pdata->piggyback_notify(
				BCMPMU_CHRGR_EVENT_MBOV, 0);
		pchrgr->mb_ov = 0;
		break;
	case PMU_IRQ_USBOV:
		pchrgr->usb_ov = 1;
		if (pdata->piggyback_notify)
			pdata->piggyback_notify(
				BCMPMU_CHRGR_EVENT_USBOV, 1);
		break;
	case PMU_IRQ_USBOV_DIS:
		if ((pchrgr->usb_ov == 1) && (pdata->piggyback_notify))
			pdata->piggyback_notify(
				BCMPMU_CHRGR_EVENT_USBOV, 0);
		pchrgr->usb_ov = 0;
		break;
	case PMU_IRQ_EOC:
		if (pdata->support_hw_eoc) {
			if (pdata->piggyback_notify)
				pdata->piggyback_notify(
					BCMPMU_CHRGR_EVENT_EOC, 0);
			blocking_notifier_call_chain(
				&pchrgr->bcmpmu->event[BCMPMU_CHRGR_EVENT_EOC].
				notifiers, BCMPMU_CHRGR_EVENT_EOC,
				NULL);
		}
		break;
	default:
		break;
	}
}

static void bcmpmu_chrgr_enable(struct work_struct *work)
{
	struct bcmpmu_chrgr *pchrgr = container_of(work, struct bcmpmu_chrgr, chrgr_enable.work);
	struct bcmpmu *bcmpmu = pchrgr->bcmpmu;

	int data = 0;
	bool event_exist = false;

	printk(KERN_INFO "%s bcmpmu_chrgr_enable\n",__func__);
	
	mutex_lock(&pchrgr->chrgrlock);
	if(!CHRGR_FIFO_EMPTY(pchrgr->chrgrfifo)){
		data = pchrgr->chrgrfifo.data[pchrgr->chrgrfifo.tail];
		CHRGR_FIFO_TAIL(pchrgr->chrgrfifo);
		if(pchrgr->chrgrfifo.fifo_full)			
			pchrgr->chrgrfifo.fifo_full = false;
		if(!CHRGR_FIFO_EMPTY(pchrgr->chrgrfifo))
			event_exist = true;
		mutex_unlock(&pchrgr->chrgrlock);

		printk(KERN_INFO "%s event_exist=%d, data=%d\n",
			__func__, event_exist, data);
		printk(KERN_INFO "%s: head : %d, tail : %d\n", __func__,pchrgr->chrgrfifo.head,pchrgr->chrgrfifo.tail);

		bcmpmu_chrgr_usb_en(bcmpmu, (data == 1)?1:0);			
	}
	else {
		printk(KERN_INFO "%s fifo empty\n",__func__);
		mutex_unlock(&pchrgr->chrgrlock);
	}
	
	if(event_exist)
		schedule_delayed_work(&pchrgr->chrgr_enable, msecs_to_jiffies(1000));	
}	

static void bcmpmu_chrgr_spa_evt(struct work_struct *work)
{
	struct bcmpmu_chrgr *pchrgr = container_of(work, struct bcmpmu_chrgr, work.work);
	enum bcmpmu_event_t event = 0;
	int data = 0;
	bool event_exist = false;

	mutex_lock(&pchrgr->lock);
	if (!SPA_FIFO_EMPTY(pchrgr->spafifo)) {
		event = pchrgr->spafifo.event[pchrgr->spafifo.tail];
		data = pchrgr->spafifo.data[pchrgr->spafifo.tail];
		SPA_FIFO_TAIL(pchrgr->spafifo);
		if (pchrgr->spafifo.fifo_full)
			pchrgr->spafifo.fifo_full = false;
		if (!SPA_FIFO_EMPTY(pchrgr->spafifo))
			event_exist = true;
		mutex_unlock(&pchrgr->lock);

		printk(KERN_INFO "%s event=%d, data=%d\n",
			__func__, event, data);


		switch (event) {
		case BCMPMU_CHRGR_EVENT_CHGR_DETECTION:
			//spa_event_handler(SPA_EVT_CHARGER, data);
			break;
		case BCMPMU_CHRGR_EVENT_MBTEMP:
			spa_event_handler(SPA_EVT_TEMP, data);
			break;
		case BCMPMU_CHRGR_EVENT_MBOV:
			spa_event_handler(SPA_EVT_OVP, data);
			break;
		case BCMPMU_CHRGR_EVENT_USBOV:
			spa_event_handler(SPA_EVT_OVP, data);
			break;
		case BCMPMU_CHRGR_EVENT_EOC:
			spa_event_handler(SPA_EVT_EOC, 0);
			break;
		case BCMPMU_CHRGR_EVENT_CAPACITY:
			spa_event_handler(SPA_EVT_CAPACITY, data);
			break;

		default:
			break;
		}
	}
	else {
		printk(KERN_INFO "%s fifo empty\n",__func__);
		mutex_unlock(&pchrgr->lock);
	}

	if (event_exist)
		schedule_delayed_work(&pchrgr->work, 0);

}

static int __devinit bcmpmu_chrgr_probe(struct platform_device *pdev)
{
	int ret = 0;

	struct bcmpmu *bcmpmu = pdev->dev.platform_data;
	struct bcmpmu_chrgr *pchrgr;
	struct bcmpmu_platform_data *pdata = bcmpmu->pdata;

	printk(KERN_INFO "%s: called.\n", __func__);
	if (pdata->piggyback_chrg == 0) {
		printk(KERN_INFO "%s: flag piggyback_chrg is not set.\n",
			__func__);
		return 0;
	}

	pchrgr = kzalloc(sizeof(struct bcmpmu_chrgr), GFP_KERNEL);
	if (pchrgr == NULL) {
		dev_err(&pdev->dev, "failed to alloc mem: %d\n", ret);
		return -ENOMEM;
	}
	pchrgr->bcmpmu = bcmpmu;
	bcmpmu->chrgrinfo = (void *)pchrgr;

	pchrgr->chrgrtype = bcmpmu->usb_accy_data.chrgr_type;
	pchrgr->chrgrcurr_max = PMU_CHRGING_CURR_MAX;

	bcmpmu->set_icc_fc_lmt = bcmpmu_set_icc_fc_lmt;
	bcmpmu->ntcht_rise_set = bcmpmu_ntcht_rise_set;
	bcmpmu->ntcht_fall_set = bcmpmu_ntcht_fall_set;
	bcmpmu->ntcct_rise_set = bcmpmu_ntcct_rise_set;
	bcmpmu->ntcct_fall_set = bcmpmu_ntcct_fall_set;

	pchrgr->chrgr.properties = bcmpmu_chrgr_props;
	pchrgr->chrgr.num_properties = ARRAY_SIZE(bcmpmu_chrgr_props);
	pchrgr->chrgr.get_property = bcmpmu_chrgr_get_property;
	pchrgr->chrgr.set_property = bcmpmu_chrgr_set_property;
	if (pdata->piggyback_chrg_name)
		pchrgr->chrgr.name = pdata->piggyback_chrg_name;
	else
		pchrgr->chrgr.name = "bcmpmu_charger";

	ret = power_supply_register(&pdev->dev, &pchrgr->chrgr);
	if (ret)
		goto err;

	pdata->piggyback_work = &pchrgr->work;
	pchrgr->spafifo.head = 0;
	pchrgr->spafifo.tail = 0;
	pchrgr->spafifo.length = BCMPMU_SPA_EVENT_FIFO_LENGTH;
	pchrgr->spafifo.fifo_full = false;
	pchrgr->spafifo.chrg_on = false;
	pdata->spafifo = &pchrgr->spafifo;
	mutex_init(&pchrgr->lock);
	pdata->spalock = &pchrgr->lock;

	INIT_DELAYED_WORK(&pchrgr->work, bcmpmu_chrgr_spa_evt);

	pchrgr->chrgrfifo.head = 0;
	pchrgr->chrgrfifo.tail = 0;
	pchrgr->chrgrfifo.length = BCMPMU_CHRGR_FIFO_LENGTH;
	pchrgr->chrgrfifo.fifo_full = false;
	mutex_init(&pchrgr->chrgrlock);

	INIT_DELAYED_WORK(&pchrgr->chrgr_enable, bcmpmu_chrgr_enable);
	

	pchrgr->chrgrtype = bcmpmu->usb_accy_data.chrgr_type;

	bcmpmu->register_irq(bcmpmu, PMU_IRQ_MBTEMPLOW,
		bcmpmu_chrgr_isr, pchrgr);
	bcmpmu->register_irq(bcmpmu, PMU_IRQ_MBTEMPHIGH,
		bcmpmu_chrgr_isr, pchrgr);
	bcmpmu->register_irq(bcmpmu, PMU_IRQ_CHGERRDIS,
		bcmpmu_chrgr_isr, pchrgr);
	bcmpmu->register_irq(bcmpmu, PMU_IRQ_MBOV,
		bcmpmu_chrgr_isr, pchrgr);
	bcmpmu->register_irq(bcmpmu, PMU_IRQ_MBOV_DIS,
		bcmpmu_chrgr_isr, pchrgr);
	if (pdata->support_hw_eoc)
		bcmpmu->register_irq(bcmpmu, PMU_IRQ_EOC,
		bcmpmu_chrgr_isr, pchrgr);
		bcmpmu->register_irq(bcmpmu, PMU_IRQ_USBOV,
		bcmpmu_chrgr_isr, pchrgr);
		bcmpmu->register_irq(bcmpmu, PMU_IRQ_USBOV_DIS,
		bcmpmu_chrgr_isr, pchrgr);

	bcmpmu->unmask_irq(bcmpmu, PMU_IRQ_MBTEMPLOW);
	bcmpmu->unmask_irq(bcmpmu, PMU_IRQ_MBTEMPHIGH);
	bcmpmu->unmask_irq(bcmpmu, PMU_IRQ_CHGERRDIS);
	bcmpmu->unmask_irq(bcmpmu, PMU_IRQ_MBOV);
	bcmpmu->unmask_irq(bcmpmu, PMU_IRQ_MBOV_DIS);
	if (pdata->support_hw_eoc)
		bcmpmu->unmask_irq(bcmpmu, PMU_IRQ_EOC);
	bcmpmu->unmask_irq(bcmpmu, PMU_IRQ_USBOV);
	bcmpmu->unmask_irq(bcmpmu, PMU_IRQ_USBOV_DIS);

	pchrgr->nb.notifier_call = bcmpmu_chrgr_event_handler;
	ret = bcmpmu_add_notifier(BCMPMU_CHRGR_EVENT_CHGR_DETECTION,
		&pchrgr->nb);
	if (ret) {
		pr_chrgr(ERROR, "%s, failed to register chrgr det notifier\n",
			__func__);
		goto err1;
	}

	ret = bcmpmu_add_notifier(BCMPMU_USB_EVENT_IN,
		&pchrgr->nb);
	if (ret) {
		pr_chrgr(ERROR, "%s, failed to register usbins notifier\n",
			__func__);
		goto err2;
	}

	ret = bcmpmu_add_notifier(BCMPMU_CHRGR_UBPD_CHG_F,
		&pchrgr->nb);
	if (ret) {
		pr_chrgr(ERROR, "%s, failed to register UBPD_CHG notifier\n",
			__func__);
		goto err2;
	}

#ifdef CONFIG_MFD_BCMPMU_DBG
	ret = sysfs_create_group(&pdev->dev.kobj, &bcmpmu_chrgr_attr_group);
	if (ret)
		goto err2;
#endif
	return 0;
err2:
	bcmpmu_remove_notifier(BCMPMU_USB_EVENT_IN, &pchrgr->nb);
	bcmpmu_remove_notifier(BCMPMU_CHRGR_EVENT_CHGR_DETECTION, &pchrgr->nb);
	bcmpmu_remove_notifier(BCMPMU_CHRGR_UBPD_CHG_F, &pchrgr->nb);
err1:
	power_supply_unregister(&pchrgr->chrgr);
err:
	kfree(pchrgr);
	return ret;
}

static int __devexit bcmpmu_chrgr_remove(struct platform_device *pdev)
{
	struct bcmpmu *bcmpmu = pdev->dev.platform_data;
	struct bcmpmu_chrgr *pchrgr = bcmpmu->chrgrinfo;
	struct bcmpmu_platform_data *pdata = pchrgr->bcmpmu->pdata;

	bcmpmu_remove_notifier(BCMPMU_USB_EVENT_IN, &pchrgr->nb);
	bcmpmu_remove_notifier(BCMPMU_CHRGR_EVENT_CHGR_DETECTION, &pchrgr->nb);
	bcmpmu_remove_notifier(BCMPMU_CHRGR_UBPD_CHG_F, &pchrgr->nb);
	power_supply_unregister(&pchrgr->chrgr);

	bcmpmu->unregister_irq(bcmpmu, PMU_IRQ_MBTEMPLOW);
	bcmpmu->unregister_irq(bcmpmu, PMU_IRQ_MBTEMPHIGH);
	bcmpmu->unregister_irq(bcmpmu, PMU_IRQ_CHGERRDIS);
	bcmpmu->unregister_irq(bcmpmu, PMU_IRQ_MBOV);
	bcmpmu->unregister_irq(bcmpmu, PMU_IRQ_MBOV_DIS);
	if (pdata->support_hw_eoc)
		bcmpmu->unregister_irq(bcmpmu, PMU_IRQ_EOC);
	bcmpmu->unregister_irq(bcmpmu, PMU_IRQ_USBOV);
	bcmpmu->unregister_irq(bcmpmu, PMU_IRQ_USBOV_DIS);

#ifdef CONFIG_MFD_BCMPMU_DBG
	sysfs_remove_group(&pdev->dev.kobj, &bcmpmu_chrgr_attr_group);
#endif
	kfree(pchrgr);
	return 0;
}

static struct platform_driver bcmpmu_chrgr_driver = {
	.driver = {
		.name = "bcmpmu_chrgr_pb",
	},
	.probe = bcmpmu_chrgr_probe,
	.remove = __devexit_p(bcmpmu_chrgr_remove),
};

static int __init bcmpmu_chrgr_init(void)
{
	return platform_driver_register(&bcmpmu_chrgr_driver);
}
subsys_initcall_sync(bcmpmu_chrgr_init);

static void __exit bcmpmu_chrgr_exit(void)
{
	platform_driver_unregister(&bcmpmu_chrgr_driver);
}
module_exit(bcmpmu_chrgr_exit);

MODULE_DESCRIPTION("BCM PMIC piggyback charger driver");
MODULE_LICENSE("GPL");
