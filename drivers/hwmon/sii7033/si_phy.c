/*
*Copyright (C) 2013-2014 Silicon Image, Inc.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation version 2.
* This program is distributed AS-IS WITHOUT ANY WARRANTY of any
* kind, whether express or implied; INCLUDING without the implied warranty
* of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
* See the GNU General Public License for more details at
* http://www.gnu.org/licenses/gpl-2.0.html.
*/
#include "Wrap.h"
#include "si_usbpd_main.h"
#include "si_time.h"
static void sii70xx_phy_sm_reset(struct sii_typec *phy);

extern int drp_mode;
void recovery_state_on_disconnect(struct sii_typec *ptypec_dev,
		bool is_dfp)
{
	if (is_dfp) {
		if (ptypec_dev->state != ATTACHED_DFP)
			ptypec_dev->state = DFP_UNATTACHED;
	} else {
		if (ptypec_dev->state != ATTACHED_UFP)
			ptypec_dev->state = UFP_UNATTACHED;
	}
}

void update_typec_status(struct sii_typec *ptypec_dev, bool is_dfp,
		bool status)
{
	if (status == DETTACH) {
		if (is_dfp) {

			ptypec_dev->ufp_attached = false;
			set_bit(UFP_DETACHED, &ptypec_dev->
					inputs);
			/*recovery_state_on_disconnect(ptypec_dev, is_dfp);*/
			queue_work(ptypec_dev->typec_work_queue, &ptypec_dev->
					sm_work);
#if defined(I2C_DBG_SYSFS)
			usbpd_event_notify(ptypec_dev->drv_context,
				PD_DFP_DETACHED,
				0x00, NULL);
#endif
		} else {
			set_bit(DFP_DETACHED, &ptypec_dev->inputs);
			ptypec_dev->dfp_attached = false;
			/*recovery_state_on_disconnect(ptypec_dev, is_dfp);*/
			queue_work(ptypec_dev->typec_work_queue, &ptypec_dev->
					sm_work);
#if defined(I2C_DBG_SYSFS)
			usbpd_event_notify(ptypec_dev->drv_context,
				PD_UFP_DETACHED,
				0x00, NULL);
#endif

		}
	} else if (status == ATTACH) {
		if (is_dfp) {
			/*sii_platform_clr_bit8(REG_ADDR__PDCC24INTM3,
			BIT_MSK__PDCC24INTM3__REG_PDCC24_INTRMASK24);*/
			ptypec_dev->dfp_attached = true;
			queue_work(ptypec_dev->typec_work_queue,
				&ptypec_dev->sm_work);
#if defined(I2C_DBG_SYSFS)
			usbpd_event_notify(ptypec_dev->drv_context,
				PD_DFP_ATTACHED,
				0x00, NULL);
#endif
		} else {
			ptypec_dev->ufp_attached = true;
#if defined(I2C_DBG_SYSFS)
			usbpd_event_notify(ptypec_dev->drv_context,
				PD_UFP_ATTACHED,
				0x00, NULL);
#endif
			queue_work(ptypec_dev->typec_work_queue,
				&ptypec_dev->sm_work);
		}
	}
}

void enable_power_mode(struct sii_typec *ptypec_dev,
	enum ufp_volsubstate cur_sel)
{
	if (!ptypec_dev)
		pr_warning("%s: Error\n", __func__);

	pr_info("CC Current Selection: ");

	switch (cur_sel) {
	case UFP_DEFAULT:
		ptypec_dev->pwr_ufp_sub_state =
			UFP_DEFAULT;
		pr_info("  Default\n");
		break;
	case UFP_1P5V:
		sii_platform_wr_reg8(REG_ADDR__ANA_CCPD_MODE_CTRL,
			0x1);
		ptypec_dev->pwr_ufp_sub_state = UFP_1P5V;
		pr_info("  1.5A\n");
		break;
	case UFP_3V:
		sii_platform_wr_reg8(REG_ADDR__ANA_CCPD_MODE_CTRL,
			0x2);
		ptypec_dev->pwr_ufp_sub_state = UFP_3V;
		pr_info("  3A\n");
		break;
	default:
		pr_info("Not a valid case\n");
		break;
	}
}

static void sii70xx_phy_sm_reset(struct sii_typec *ptypeC)
{
	ptypeC->ufp_attached = false;
	ptypeC->dfp_attached = false;
	ptypeC->dead_battery = false;
	ptypeC->is_flipped = TYPEC_UNDEFINED;
	ptypeC->pwr_ufp_sub_state = 0;
	ptypeC->pwr_dfp_sub_state = 0;
	ptypeC->inputs = 0;
}

static void typec_sm_state_init(struct sii_typec *ptypec_dev,
		enum phy_drp_config drp_role)
{
	if (drp_role == TYPEC_DRP_TOGGLE_RD) {
		pr_info("DRP ROLE: set to TOGGLE_RD\n");
		ptypec_dev->cc_mode = DRP;
	} else if (drp_role == TYPEC_DRP_TOGGLE_RP) {
		pr_info("DRP ROLE: set to TOGGLE_RP\n");
		ptypec_dev->cc_mode = DRP;
	} else if (drp_role == TYPEC_DRP_DFP) {
		pr_info(" DFP SET(F)\n");
		ptypec_dev->cc_mode = DFP;
	} else {
		pr_info(" UFP SET(F)\n");
		ptypec_dev->cc_mode = UFP;
	}
		ptypec_dev->state = UNATTACHED;
}
/*
static void enable_vconn(enum typec_orientation cbl_orientation)
{
	uint8_t rcd_data1,rcd_data2;

	pr_info(" VCONN FLIP/NON-FLIP : ");

	rcd_data1 = sii_platform_rd_reg8(REG_ADDR__CC1VOL) & 0x3F;
	rcd_data2 = sii_platform_rd_reg8(REG_ADDR__CC2VOL) & 0x3F;

	if (cbl_orientation == TYPEC_CABLE_NOT_FLIPPED)	{
		if (((rcd_data2) >= DEFAULT_VCONN_MIN) &&
		((rcd_data2) <= DEFAULT_VCONN_MAX)) {
			pr_info(" TYPEC_CABLE_NOT_FLIPPED\n");
			goto vconn_enable;
		} else {
			pr_info(" NOT IN RANGE - FLIPPED\n");
		}
	} else {
		if(((rcd_data1) >= DEFAULT_VCONN_MIN) &&
		((rcd_data1) <= DEFAULT_VCONN_MAX)) {
			pr_info(" TYPEC_CABLE_FLIPPED\n");
			goto vconn_enable;
		} else {
			pr_info(" NOT IN RANGE - NOT FLIPPED\n");
		}
	}

vconn_enable:
	sii_platform_set_bit8(REG_ADDR__ANA_CCPD_DRV_CTRL,
			BIT_MSK__ANA_CCPD_DRV_CTRL__RI_VCONN_EN);
}
*/
static bool check_substrate_req_dfp(struct sii_typec *ptypec_dev)
{
	uint8_t rcd_data1, rcd_data2;
	enum typec_orientation cb_orietn = TYPEC_UNDEFINED;
	uint8_t vol_thres = 0;
	/*struct sii70xx_drv_context *drv_context =
		(struct sii70xx_drv_context *)
		ptypec_dev->drv_context;*/

	if (!ptypec_dev) {
		pr_warning("%s: Error\n", __func__);
		return false;
	}

	rcd_data1 = sii_platform_rd_reg8(REG_ADDR__CC1VOL) &
		0x3F;
	rcd_data2 = sii_platform_rd_reg8(REG_ADDR__CC2VOL) &
		0x3F;
	vol_thres = sii_platform_rd_reg8(REG_ADDR__CCCTR8) & 0x3F;
	pr_info("CC1 voltage = %d and CC2 vol = %d %d\n",
			rcd_data1, rcd_data2,
			ptypec_dev->pwr_ufp_sub_state);
	if (((rcd_data1) > DEFAULT_MIN) &&
			((rcd_data1) <= vol_thres)) {
		cb_orietn = TYPEC_CABLE_NOT_FLIPPED;
	} else if (((rcd_data2) > DEFAULT_MIN) &&
			((rcd_data2) <= vol_thres)) {
		cb_orietn = TYPEC_CABLE_FLIPPED;
	} else {
		cb_orietn = TYPEC_UNDEFINED;
	}

	pr_info(" CC Connection FLIP/NON-FLIP : ");

	if (cb_orietn == TYPEC_CABLE_NOT_FLIPPED) {
		pr_info(" %s: NON-FLIP  :%s\n",
			ANSI_ESC_CYAN_TEXT, ANSI_ESC_RESET_TEXT);
		sii_platform_clr_bit8(REG_ADDR__PDCTR12,
			BIT_MSK__PDCTR12__RI_MODE_FLIP);
		ptypec_dev->is_flipped =
			TYPEC_CABLE_NOT_FLIPPED;
	} else if (cb_orietn == TYPEC_CABLE_FLIPPED) {
		pr_info("%s: FLIP :%s\n",
			ANSI_ESC_CYAN_TEXT, ANSI_ESC_RESET_TEXT);
		sii_platform_set_bit8(REG_ADDR__PDCTR12,
			BIT_MSK__PDCTR12__RI_MODE_FLIP);
		ptypec_dev->is_flipped =
			TYPEC_CABLE_FLIPPED;
	} else {
		ptypec_dev->is_flipped = TYPEC_UNDEFINED;
		pr_info(": In-Valid :\n");
	}
	/*substrate is not checked yet*/
	return true;
}


static void ufp_cc_adc_work(struct work_struct *w)
{
	struct sii_typec *ptypec_dev = container_of(w,
			struct sii_typec, adc_work);

	uint8_t rcd_data1, rcd_data2, vol_thres;
	rcd_data1 = 0;
	rcd_data2 = 0;
	vol_thres = 0;
	ptypec_dev->rcd_data1 = 0;
	ptypec_dev->rcd_data2 = 0;
	vol_thres = sii_platform_rd_reg8(REG_ADDR__CCCTR8) & 0x3F;
	pr_info("\nufp_cc_adc_work\n");
	do {
		rcd_data1 = sii_platform_rd_reg8(REG_ADDR__CC1VOL) & 0x3F;
		rcd_data2 = sii_platform_rd_reg8(REG_ADDR__CC2VOL) & 0x3F;

		if (((rcd_data1 > DEFAULT_MIN) &&
			(rcd_data1 <= vol_thres)) || ((rcd_data2 > DEFAULT_MIN)
			&& (rcd_data2 <= vol_thres)) ||
			(ptypec_dev->state != ATTACHED_UFP)) {
			pr_info("Not ATTACHED_UFP\n");
			goto exit;
		}
		msleep(20);/*min should me 20ms in linux is valid*/
	} while (1);

exit:
	ptypec_dev->rcd_data1 = rcd_data1;
	ptypec_dev->rcd_data2 = rcd_data2;
	complete(&ptypec_dev->adc_read_done_complete);
}

static bool check_substrate_req_ufp(struct sii_typec *ptypec_dev)
{
	uint8_t rcd_data1, rcd_data2;
	uint8_t vol_thres;
	enum typec_orientation cb_orietn = TYPEC_UNDEFINED;

	if (!ptypec_dev) {
		pr_warning("%s: Error\n", __func__);
		return false;
	}

	rcd_data1 = sii_platform_rd_reg8(REG_ADDR__CC1VOL) & 0x3F;
	rcd_data2 = sii_platform_rd_reg8(REG_ADDR__CC2VOL) & 0x3F;
	vol_thres = sii_platform_rd_reg8(REG_ADDR__CCCTR8) & 0x3F;
	pr_info("CC1 voltage = %X and CC2 vol = %X\n",
		rcd_data1, rcd_data2);

	if (((rcd_data1 > DEFAULT_MIN) && (rcd_data1 <= vol_thres)) ||
		((rcd_data2 > DEFAULT_MIN) && (rcd_data2 <= vol_thres))) {
		pr_info("ADC checking\n");
		goto ufp_adc_done;
	} else {
		init_completion(&ptypec_dev->adc_read_done_complete);
		schedule_work(&ptypec_dev->adc_work);
	}

	wait_for_completion_timeout(&ptypec_dev->
		adc_read_done_complete, msecs_to_jiffies(900));
	rcd_data1 = ptypec_dev->rcd_data1;
	rcd_data2 = ptypec_dev->rcd_data2;

ufp_adc_done:

	pr_info("\n **Fianal CC1 = %X and CC2 = %X **\n",
		rcd_data1, rcd_data2);
	if (((rcd_data1) > DEFAULT_MIN) &&
			((rcd_data1) <= vol_thres)) {
		cb_orietn = TYPEC_CABLE_NOT_FLIPPED;
	} else if (((rcd_data2) > DEFAULT_MIN) &&
			((rcd_data2) <= vol_thres)) {
		cb_orietn = TYPEC_CABLE_FLIPPED;
	} else {
		cb_orietn = TYPEC_UNDEFINED;
	}

	if (((rcd_data1) > DEFAULT_MIN) &&
			((rcd_data1) <= DEFAULT_MAX)) {
		ptypec_dev->pwr_dfp_sub_state = UFP_DEFAULT;
	} else if (((rcd_data2) > DEFAULT_MIN) &&
			((rcd_data2) <= DEFAULT_MAX)) {
		ptypec_dev->pwr_dfp_sub_state = UFP_DEFAULT;
	} else if (((rcd_data1) > MIN_1_5) &&
			((rcd_data1) <= MAX_1_5)) {
		ptypec_dev->pwr_dfp_sub_state = UFP_1P5V;
	} else if (((rcd_data2) > MIN_1_5) &&
			((rcd_data2) <= MAX_1_5)) {
		ptypec_dev->pwr_dfp_sub_state = UFP_1P5V;
	} else if (((rcd_data1) > MIN_3) &&
			((rcd_data1) <= MAX_3)) {
		ptypec_dev->pwr_dfp_sub_state = UFP_3V;
	} else if (((rcd_data2) > MIN_3) &&
			((rcd_data2) <= MAX_3)) {
		ptypec_dev->pwr_dfp_sub_state = UFP_3V;
	} else {
		ptypec_dev->pwr_dfp_sub_state = UFP_DEFAULT;
	}


	pr_info(" UFP CC Connection FLIP/NON-FLIP : ");

	if (cb_orietn == TYPEC_CABLE_NOT_FLIPPED) {
		pr_info("%s : NON-FLIP  :%s\n",
			ANSI_ESC_CYAN_TEXT, ANSI_ESC_RESET_TEXT);
		sii_platform_clr_bit8(REG_ADDR__PDCTR12,
			BIT_MSK__PDCTR12__RI_MODE_FLIP);
		ptypec_dev->is_flipped =
			TYPEC_CABLE_NOT_FLIPPED;
	} else if (cb_orietn == TYPEC_CABLE_FLIPPED) {
		pr_info("%s: FLIP :%s\n",
			ANSI_ESC_CYAN_TEXT, ANSI_ESC_RESET_TEXT);
		sii_platform_set_bit8(REG_ADDR__PDCTR12,
			BIT_MSK__PDCTR12__RI_MODE_FLIP);
		ptypec_dev->is_flipped =
			TYPEC_CABLE_FLIPPED;
	} else {
		ptypec_dev->is_flipped = TYPEC_UNDEFINED;
		pr_info(": In-Valid :\n");
	}

	return true;
}

void sii_typec_events(struct sii_typec *ptypec_dev,
	uint32_t event_flags)
{
	int work = 1;

	pr_info("\n Handle sii_typec_events :%x\n",
		event_flags);
	switch (ptypec_dev->state) {

	case ATTACHED_UFP:
		if (event_flags == FEAT_PR_SWP) {
			ptypec_dev->state = ATTACHED_DFP;
			set_bit(PR_SWAP_DONE, &ptypec_dev->inputs);
			queue_work(ptypec_dev->typec_work_queue,
				&ptypec_dev->sm_work);
			break;
		}

		if (event_flags == FEAT_DR_SWP) {
			set_bit(DR_SWAP_DONE, &ptypec_dev->inputs);
			work = 0;
			queue_work(ptypec_dev->typec_work_queue,
				&ptypec_dev->sm_work);
				break;
			/*change to DFP*/
		}
		break;
	case ATTACHED_DFP:
		if (event_flags == FEAT_PR_SWP) {
			ptypec_dev->is_vbus_detected = true;
			ptypec_dev->state = ATTACHED_UFP;
			set_bit(PR_SWAP_DONE, &ptypec_dev->inputs);
			queue_work(ptypec_dev->typec_work_queue, &ptypec_dev->
				sm_work);
			break;
		}

		if (event_flags == FEAT_DR_SWP) {
			clear_bit(FEAT_DR_SWP, &ptypec_dev->inputs);
			set_bit(DR_SWAP_DONE, &ptypec_dev->inputs);
			/*ptypec_dev->state = ATTACHED_UFP;*/
			queue_work(ptypec_dev->typec_work_queue,
				&ptypec_dev->sm_work);
		}
	default:
		work = 0;
		break;
	}
}

static void typec_sm0_work(struct work_struct *w)
{
	struct sii_typec *ptypec_dev = container_of(w,
			struct sii_typec, sm_work);
	struct sii70xx_drv_context *drv_context =
		(struct sii70xx_drv_context *)ptypec_dev->
		drv_context;
	/*struct sii_typec *phy = ptypec_dev->phy;*/
	int work = 0;
	bool result;

	if (!down_interruptible(&drv_context->isr_lock)) {
		pr_info("Type-c State: %x", ptypec_dev->state);
		switch (ptypec_dev->state) {
		case DISABLED:
		case ERROR_RECOVERY:
			pr_info("DISABLED\n");

			ptypec_dev->prev_state = ptypec_dev->state;
			ptypec_dev->is_flipped = TYPEC_UNDEFINED;
			ptypec_dev->state = UFP_UNATTACHED;
			msleep(25);
			break;

		case UNATTACHED:
			pr_info("%s:UNATTACHED:%s\n",
				ANSI_ESC_MAGENTA_TEXT, ANSI_ESC_RESET_TEXT);
			sii_platform_read_70xx_gpio(drv_context);

			/*if (ptypec_dev->prev_state == UNATTACHED) {
				pr_info("PREV_STATE:UNATTACHED\n");
				if (ptypec_dev->cc_mode ==
					DRP) {
					pr_info("**toggle RD");
					sii_update_70xx_mode(ptypec_dev->
						drv_context,
					TYPEC_DRP_TOGGLE_RD);
				}
			}*/
			if (ptypec_dev->prev_state == ATTACHED_UFP) {
				pr_info("PREV_STATE:ATTACHED_UFP\n");
				if (ptypec_dev->cc_mode ==
					DRP) {
					pr_info("**toggle RD");
					sii_update_70xx_mode(ptypec_dev->
						drv_context,
					TYPEC_DRP_TOGGLE_RD);
				} else if (ptypec_dev->cc_mode == DFP) {
					sii_update_70xx_mode(ptypec_dev->
						drv_context, TYPEC_DRP_DFP);
				} else if (ptypec_dev->cc_mode == UFP) {
					sii_update_70xx_mode(ptypec_dev->
						drv_context, TYPEC_DRP_UFP);
				}
				ptypec_dev->ufp_attached = false;
				/*to take new interrupt again if
				unplug/plug happens*/
			}
			if (ptypec_dev->prev_state == LOCK_UFP) {
				pr_info("PREV_STATE:LOCK_UFP\n");
				if (ptypec_dev->cc_mode ==
					DRP) {
						pr_info("**toggle RP");
					sii_update_70xx_mode(ptypec_dev->
						drv_context,
						TYPEC_DRP_TOGGLE_RP);
				} else if (ptypec_dev->cc_mode ==
					DFP) {
					sii_update_70xx_mode(ptypec_dev->
						drv_context,
						TYPEC_DRP_DFP);
				}
				/*src= 0, snk = 0*/
				sii70xx_vbus_enable(drv_context, VBUS_SNK);
				ptypec_dev->dfp_attached = false;
			}
			if (ptypec_dev->prev_state == ATTACHED_DFP) {
				pr_info("PREV_STATE:ATTACHED_DFP\n");
				/*after pr swap this will happen*/
				sii70xx_vbus_enable(drv_context, VBUS_SNK);
				if (ptypec_dev->cc_mode == DFP) {
					sii_update_70xx_mode(ptypec_dev->
						drv_context, TYPEC_DRP_DFP);
				} else if (ptypec_dev->cc_mode == UFP) {
					sii_update_70xx_mode(ptypec_dev->
						drv_context, TYPEC_DRP_UFP);
				}
				ptypec_dev->dfp_attached = false;
				/*to take new interrupt again if
				unplug/plug happens*/
			}

			if (test_bit(DFP_ATTACHED, &ptypec_dev->inputs)) {
				clear_bit(DFP_ATTACHED, &ptypec_dev->inputs);
				ptypec_dev->inputs = 0;
				ptypec_dev->is_vbus_detected = true;
				ptypec_dev->state = ATTACHED_UFP;
				work = 1;
				break;
			} else if (test_bit(UFP_ATTACHED, &ptypec_dev->
				inputs)) {
				clear_bit(UFP_ATTACHED, &ptypec_dev->
					inputs);
				ptypec_dev->state =	DFP_DRP_WAIT;
				work = 1;
				break;
			} else {
			}
			clear_bit(UFP_DETACHED, &ptypec_dev->inputs);
			clear_bit(DFP_DETACHED, &ptypec_dev->inputs);
			if (test_bit(PR_SWAP_DONE, &ptypec_dev->inputs))
				clear_bit(PR_SWAP_DONE, &ptypec_dev->inputs);

			ptypec_dev->prev_state = ptypec_dev->state;
			ptypec_dev->is_flipped = TYPEC_UNDEFINED;
			break;

		case LOCK_UFP:
			pr_info("%s:LOCK_UFP:%s\n",
				ANSI_ESC_MAGENTA_TEXT, ANSI_ESC_RESET_TEXT);
			sii70xx_vbus_enable(drv_context, VBUS_DEFAULT);
			/*enable_vconn(cb_orietn);*/
			set_70xx_mode(drv_context, TYPEC_DRP_UFP);
			/*as per mtk schematic and daniel suggestions*/
			/* do not make CC pins to gnd by rd*/
			ptypec_dev->prev_state = ptypec_dev->state;
			msleep(100);
			ptypec_dev->state = UNATTACHED;
			work = true;

			break;

		case ATTACHED_UFP:
			pr_info("%s:ATTACHED UFP:%s\n",
				ANSI_ESC_MAGENTA_TEXT, ANSI_ESC_RESET_TEXT);
			ptypec_dev->prev_state = ptypec_dev->state;

			if (test_bit(DFP_DETACHED, &ptypec_dev->inputs)) {
				pr_info("*** DFP Detached\n");
				ptypec_dev->dfp_attached = false;
				clear_bit(DFP_DETACHED, &ptypec_dev->
					inputs);
				ptypec_dev->state = UNATTACHED;
				ptypec_dev->inputs = 0;
				work = 1;
				break;
			}
			if (test_bit(PR_SWAP_DONE, &ptypec_dev->inputs)) {
				clear_bit(PR_SWAP_DONE, &ptypec_dev->inputs);
				pr_info("PR_SWAP\n");
				break;
			}
			if (test_bit(DR_SWAP_DONE, &ptypec_dev->inputs)) {
				clear_bit(DR_SWAP_DONE, &ptypec_dev->
					inputs);
				pr_info("DR_SWAP_DONE\n");
				break;
			}
			if (test_bit(DFP_ATTACHED, &ptypec_dev->inputs)) {
				if (ptypec_dev->is_flipped != TYPEC_UNDEFINED) {
					clear_bit(DFP_ATTACHED, &ptypec_dev->
						inputs);
				/* for continous interrupts in case of drp.*/
				/*if we already processed this then we skip
				this step*/
					pr_info("ERROR Same Interrupt arraived\n");
					break;
				}
			}
			if (check_substrate_req_ufp(ptypec_dev)) {
				/*check CC voltages if ok then enable vbus*/
				if (ptypec_dev->is_flipped ==
					TYPEC_UNDEFINED) {
					pr_info("attached ufp: not In range\n");
				} else {
					/*cc voltages are fine*/
					result = usbpd_set_ufp_init(ptypec_dev->
						drv_context->
						pusbpd_policy);
					if (!result) {
						pr_info("DFP:CONFIG FAILED\n");
						ptypec_dev->state = UNATTACHED;
						return;
					}
				}
			}
			work = false;
			break;

		case ATTACHED_DFP:
			pr_info("%s:ATTACHED_DFP:%s\n",
				ANSI_ESC_MAGENTA_TEXT, ANSI_ESC_RESET_TEXT);
			ptypec_dev->prev_state = ptypec_dev->state;

			if (test_bit(UFP_DETACHED, &ptypec_dev->inputs)) {
				/*pr_debug("UFP DETACHED\n");*/
				clear_bit(UFP_DETACHED, &ptypec_dev->inputs);
				if (ptypec_dev->cc_mode ==
					DRP) {
					ptypec_dev->state = LOCK_UFP;
					work = 1;
				} else {
					ptypec_dev->state = UNATTACHED;
					work = 1;
				}
				break;
			}

			if (test_bit(PR_SWAP_DONE, &ptypec_dev->inputs)) {
				clear_bit(PR_SWAP_DONE, &ptypec_dev->inputs);
				break;
			}
			if (test_bit(DR_SWAP_DONE, &ptypec_dev->inputs)) {
				clear_bit(DR_SWAP_DONE, &ptypec_dev->
					inputs);
				break;
			}
			if (test_bit(UFP_ATTACHED, &ptypec_dev->inputs)) {
				if (ptypec_dev->is_flipped != TYPEC_UNDEFINED) {
					clear_bit(UFP_ATTACHED, &ptypec_dev->
						inputs);
					pr_info("ERROR Same Interrupt arraived\n");
				/* for continous interrupts in case of drp.*/
				/*if we already processed this then we skip
				this step*/
					break;
				}
			}
			if (check_substrate_req_dfp(ptypec_dev)) {
				/*check cc voltages*/
				if (ptypec_dev->is_flipped == TYPEC_UNDEFINED)
					pr_debug("DFP: not In range\n");
				else {
					sii70xx_vbus_enable(drv_context,
						VBUS_SRC);
					result = usbpd_set_dfp_init(ptypec_dev->
						drv_context->pusbpd_policy);
					if (!result) {
						pr_debug("Error: DFP Config\n");
						ptypec_dev->state = UNATTACHED;
						work = 1;
						break;
					}
				}
			}
			break;

		case DFP_DRP_WAIT:
			sii_mask_detach_interrupts(ptypec_dev->drv_context);
			ptypec_dev->prev_state = ptypec_dev->state;

			pr_info("%s:DFP_DRP_WAIT:%s\n",
				ANSI_ESC_MAGENTA_TEXT, ANSI_ESC_RESET_TEXT);
			msleep(100);
			/*enable_vconn(phy);*/
			ptypec_dev->state = ATTACHED_DFP;
			work = true;
			sii_mask_attach_interrupts(ptypec_dev->drv_context);
			break;

		default:
			work = false;
			break;
		}

		up(&ptypec_dev->drv_context->isr_lock);

		if (work) {
			work = false;
			queue_work(ptypec_dev->typec_work_queue,
				&ptypec_dev->sm_work);
		}
	}
}

void *sii_phy_init(struct sii70xx_drv_context *drv_context)
{
	struct sii_typec *ptypeC =
		kzalloc(sizeof(struct sii_typec), GFP_KERNEL);

	if (!ptypeC) {
		pr_warning("%s: usbpd_phy Error.\n", __func__);
		return NULL;
	}
	ptypeC->drv_context = drv_context;
	ptypeC->prev_state = DISABLED;

	sii70xx_phy_sm_reset(ptypeC);

	typec_sm_state_init(ptypeC, drv_context->phy_mode);

	/*INIT_WORK(&ptypeC->sm_work, typec_sm0_work);

	schedule_work(&ptypeC->sm_work);*/
	ptypeC->typec_work_queue =
		create_singlethread_workqueue(SII_DRIVER_NAME);
	if (ptypeC->typec_work_queue == NULL)
		goto exit;

	INIT_WORK(&ptypeC->sm_work, typec_sm0_work);

	INIT_WORK(&ptypeC->adc_work, ufp_cc_adc_work);

	return ptypeC;
exit:
	sii_phy_exit(ptypeC);
	return NULL;
}

void sii_phy_exit(void *context)
{
	struct sii_typec *ptypec_dev  = (struct sii_typec *)context;

	cancel_work_sync(&ptypec_dev->adc_work);

	cancel_work_sync(&ptypec_dev->sm_work);
	destroy_workqueue(ptypec_dev->typec_work_queue);

	kfree(ptypec_dev);
}
