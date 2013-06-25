/************************************************************************************************/
/*                                                                                              */
/*  Copyright 2012 Broadcom Corporation                                                        */
/*                                                                                              */
/*     Unless you and Broadcom execute a separate written software license agreement governing  */
/*     use of this software, this software is licensed to you under the terms of the GNU        */
/*     General Public License version 2 (the GPL), available at                                 */
/*                                                                                              */
/*          http://www.broadcom.com/licenses/GPLv2.php                                          */
/*                                                                                              */
/*     with the following added to such license:                                                */
/*                                                                                              */
/*     As a special exception, the copyright holders of this software give you permission to    */
/*     link this software with independent modules, and to copy and distribute the resulting    */
/*     executable under terms of your choice, provided that you also meet, for each linked      */
/*     independent module, the terms and conditions of the license of that module.              */
/*     An independent module is a module which is not derived from this software.  The special  */
/*     exception does not apply to any modifications of the software.                           */
/*                                                                                              */
/*     Notwithstanding the above, under no circumstances may you combine this software in any   */
/*     way with any other Broadcom software provided under a license other than the GPL,        */
/*     without Broadcom's express prior written consent.                                        */
/*                                                                                              */
/*     Date     :    Generated on 3/20/2012 13:8:57                                            */
/*     RDB file : /projects/CHIP/revA0                                                                    */
/************************************************************************************************/

#ifndef __REGISTERS_CHIP_H__
#define __REGISTERS_CHIP_H__

#include "brcm_rdb_a9cpu.h"
#include "brcm_rdb_a9pmu.h"
#include "brcm_rdb_a9ptm.h"
#include "brcm_rdb_aci.h"
#include "brcm_rdb_ahintc.h"
#include "brcm_rdb_apbtoatb.h"
#include "brcm_rdb_aphy_csr.h"
#include "brcm_rdb_atbfilter.h"
#include "brcm_rdb_atb_stm.h"
#include "brcm_rdb_audioh.h"
#include "brcm_rdb_auxmic.h"
#include "brcm_rdb_axitp1.h"
#include "brcm_rdb_axitp16.h"
#include "brcm_rdb_axitp4.h"
#include "brcm_rdb_bbrx.h"
#include "brcm_rdb_bintc.h"
#include "brcm_rdb_bmdm_pwrmgr.h"
#include "brcm_rdb_bmodem_aoncfg.h"
#include "brcm_rdb_bmodem_syscfg.h"
#include "brcm_rdb_broadcast_storm_suppression_register.h"
#include "brcm_rdb_cfp_configuration_register.h"
#include "brcm_rdb_cfp_tcam_register.h"
#include "brcm_rdb_chipreg.h"
#include "brcm_rdb_cipher.h"
#include "brcm_rdb_cir.h"
#include "brcm_rdb_comms_sw.h"
#include "brcm_rdb_cpc_harq.h"
#include "brcm_rdb_cph_aadmac.h"
#include "brcm_rdb_cph_cfifo.h"
#include "brcm_rdb_cph_ssasw.h"
#include "brcm_rdb_cr4dbg.h"
#include "brcm_rdb_csr.h"
#include "brcm_rdb_cstf.h"
#include "brcm_rdb_cstpiu.h"
#include "brcm_rdb_cti.h"
#include "brcm_rdb_d1w.h"
#include "brcm_rdb_datapacker.h"
#include "brcm_rdb_ddr3ctl.h"
#include "brcm_rdb_ddr40_phy_addr_ctl.h"
#include "brcm_rdb_ddr40_phy_word_lane.h"
#include "brcm_rdb_dmux.h"
#include "brcm_rdb_dphy_csr.h"
#include "brcm_rdb_dsp_3wire_intf.h"
#include "brcm_rdb_dsp_ac.h"
#include "brcm_rdb_dsp_cipher.h"
#include "brcm_rdb_dsp_cpmr.h"
#include "brcm_rdb_dsp_digrf.h"
#include "brcm_rdb_dsp_equalizer.h"
#include "brcm_rdb_dsp_event_ctrl.h"
#include "brcm_rdb_dsp_intc.h"
#include "brcm_rdb_dsp_rfspi.h"
#include "brcm_rdb_dsp_rf_gpo.h"
#include "brcm_rdb_dsp_rx_control.h"
#include "brcm_rdb_dsp_sys_timer.h"
#include "brcm_rdb_dsp_tl3r.h"
#include "brcm_rdb_dsp_trace.h"
#include "brcm_rdb_dsp_tx_control.h"
#include "brcm_rdb_dwdma_ahb.h"
#include "brcm_rdb_dos_attack_prevent_register.h"
#include "brcm_rdb_eap_register.h"
#include "brcm_rdb_eav_dte.h"
#include "brcm_rdb_eav_register.h"
#include "brcm_rdb_ehci.h"
#include "brcm_rdb_esw_arl_cntrl.h"
#include "brcm_rdb_esw_arl_vtbl_acc.h"
#include "brcm_rdb_esw_busif_imp.h"
#include "brcm_rdb_esw_busif_mtp.h"
#include "brcm_rdb_esw_busif_ptm.h"
#include "brcm_rdb_esw_contrl.h"
#include "brcm_rdb_esw_diag.h"
#include "brcm_rdb_esw_flow.h"
#include "brcm_rdb_esw_flow_imp.h"
#include "brcm_rdb_esw_mem_acc.h"
#include "brcm_rdb_esw_mib.h"
#include "brcm_rdb_esw_mgmn.h"
#include "brcm_rdb_esw_status.h"
#include "brcm_rdb_etb.h"
#include "brcm_rdb_etb2axi.h"
#include "brcm_rdb_etmr4.h"
#include "brcm_rdb_esw_vlan_register.h"
#include "brcm_rdb_fmon.h"
#include "brcm_rdb_fshost.h"
#include "brcm_rdb_fshost_ctrl.h"
#include "brcm_rdb_gea.h"
#include "brcm_rdb_giccpu.h"
#include "brcm_rdb_gicdist.h"
#include "brcm_rdb_gictr.h"
#include "brcm_rdb_glbtmr.h"
#include "brcm_rdb_globperf.h"
#include "brcm_rdb_gpio.h"
#include "brcm_rdb_gptimers.h"
#include "brcm_rdb_harq.h"
#include "brcm_rdb_hsdpa_turbo_decoder_module.h"
#include "brcm_rdb_hsic_phy.h"
#include "brcm_rdb_hsi_apb.h"
#include "brcm_rdb_hsmcfg.h"
#include "brcm_rdb_hsotg.h"
#include "brcm_rdb_hsotg_ctrl.h"
#include "brcm_rdb_hubsw.h"
#include "brcm_rdb_hucm.h"
#include "brcm_rdb_ieee_802_1q_vlan_registers.h"
#include "brcm_rdb_ipcopen.h"
#include "brcm_rdb_ipcsec.h"
#include "brcm_rdb_jumbo_frame_control_register.h"
#include "brcm_rdb_kek_apb.h"
#include "brcm_rdb_keypad.h"
#include "brcm_rdb_konatzcfg.h"
#include "brcm_rdb_kona_gptimer.h"
#include "brcm_rdb_led_matrix.h"
#include "brcm_rdb_loop_detection_register.h"
#include "brcm_rdb_mastersw.h"
#include "brcm_rdb_mib_snapshot_register.h"
#include "brcm_rdb_missing.h"
#include "brcm_rdb_modem_sw.h"
#include "brcm_rdb_mp.h"
#include "brcm_rdb_mphi.h"
#include "brcm_rdb_mpu.h"
#include "brcm_rdb_mspt_register.h"
#include "brcm_rdb_nand.h"
#include "brcm_rdb_non_dmac.h"
#include "brcm_rdb_ohci.h"
#include "brcm_rdb_otp.h"
#include "brcm_rdb_padctrlreg.h"
#include "brcm_rdb_pka.h"
#include "brcm_rdb_pl310.h"
#include "brcm_rdb_ptmr_wd.h"
#include "brcm_rdb_pwm_top.h"
#include "brcm_rdb_pwrmgr.h"
#include "brcm_rdb_pwrwdog.h"
#include "brcm_rdb_qos_registers.h"
#include "brcm_rdb_rng.h"
#include "brcm_rdb_rtc.h"
#include "brcm_rdb_rtc_apb.h"
#include "brcm_rdb_rtc_config.h"
#include "brcm_rdb_sclkcal.h"
#include "brcm_rdb_scu.h"
#include "brcm_rdb_sdt.h"
#include "brcm_rdb_sec.h"
#include "brcm_rdb_sectrap.h"
#include "brcm_rdb_secwatchdog.h"
#include "brcm_rdb_secwatchdog_adrmod.h"
#include "brcm_rdb_sec_dmac.h"
#include "brcm_rdb_simi.h"
#include "brcm_rdb_slptimer.h"
#include "brcm_rdb_spis_ahb.h"
#include "brcm_rdb_spm.h"
#include "brcm_rdb_spum_apb.h"
#include "brcm_rdb_spum_axi.h"
#include "brcm_rdb_srcmixer.h"
#include "brcm_rdb_sspi.h"
#include "brcm_rdb_sspil.h"
#include "brcm_rdb_swstm.h"
#include "brcm_rdb_syssw.h"
#include "brcm_rdb_tmon.h"
#include "brcm_rdb_traffic_remarking_register.h"
#include "brcm_rdb_trunking_registers.h"
#include "brcm_rdb_uartb.h"
#include "brcm_rdb_usbh_phy.h"
#include "brcm_rdb_vcitm.h"
#include "brcm_rdb_wcdma_gpio.h"
#include "brcm_rdb_wti.h"
#include "brcm_rdb_afc_top.h"
#include "brcm_rdb_bb_tx_filters_and_2091_if.h"
#include "brcm_rdb_bmdm_clk_mgr_reg.h"
#include "brcm_rdb_bmdm_rst_mgr_reg.h"
#include "brcm_rdb_combiner.h"
#include "brcm_rdb_combiner2.h"
#include "brcm_rdb_cpp_cluster.h"
#include "brcm_rdb_deinterleaver.h"
#include "brcm_rdb_dsp_clk_mgr_reg.h"
#include "brcm_rdb_dsp_rst_mgr_reg.h"
#include "brcm_rdb_e2ss.h"
#include "brcm_rdb_emmcsdxc.h"
#include "brcm_rdb_edch_fng_core_top.h"
#include "brcm_rdb_esub_clk_mgr_reg.h"
#include "brcm_rdb_esub_rst_mgr_reg.h"
#include "brcm_rdb_fng_core_top.h"
#include "brcm_rdb_fsclkcal.h"
#include "brcm_rdb_hs_scch.h"
#include "brcm_rdb_i2c_mm_hs.h"
#include "brcm_rdb_i2c_mm_hs_slave.h"
#include "brcm_rdb_khub_clk_mgr_reg.h"
#include "brcm_rdb_khub_rst_mgr_reg.h"
#include "brcm_rdb_khubaon_clk_mgr_reg.h"
#include "brcm_rdb_khubaon_rst_mgr_reg.h"
#include "brcm_rdb_kpm_clk_mgr_reg.h"
#include "brcm_rdb_kpm_rst_mgr_reg.h"
#include "brcm_rdb_kproc_clk_mgr_reg.h"
#include "brcm_rdb_kproc_rst_mgr_reg.h"
#include "brcm_rdb_kps_clk_mgr_reg.h"
#include "brcm_rdb_kps_rst_mgr_reg.h"
#include "brcm_rdb_layer_1_int_cont.h"
#include "brcm_rdb_layer_2_async.h"
#include "brcm_rdb_layer_2_int_cont.h"
#include "brcm_rdb_master_timer.h"
#include "brcm_rdb_modem_cfg_and_core_clks.h"
#include "brcm_rdb_modem_clocks_part_1.h"
#include "brcm_rdb_modem_clocks_part_2.h"
#include "brcm_rdb_mpdxx_apb.h"
#include "brcm_rdb_pdm_top.h"
#include "brcm_rdb_psychic.h"
#include "brcm_rdb_psync_for_10ppm.h"
#include "brcm_rdb_rake_fng_top.h"
#include "brcm_rdb_rake_tiny_fng_top.h"
#include "brcm_rdb_rf_interface_block1_top.h"
#include "brcm_rdb_rf_interface_block2_top.h"
#include "brcm_rdb_rf_interface_block3_top.h"
#include "brcm_rdb_rf_interface_block4_top.h"
#include "brcm_rdb_rf_interface_block5_top.h"
#include "brcm_rdb_rfic_mst.h"
#include "brcm_rdb_rfic_reg_file.h"
#include "brcm_rdb_rfic_scheduler.h"
#include "brcm_rdb_root_clk_mgr_reg.h"
#include "brcm_rdb_root_rst_mgr_reg.h"
#include "brcm_rdb_rxbitlevel.h"
#include "brcm_rdb_sc_xy_state_derive.h"
#include "brcm_rdb_sleeptimer3g.h"
#include "brcm_rdb_ssync_apb.h"
#include "brcm_rdb_sttd_apb.h"
#include "brcm_rdb_tiny_fng_core_top.h"
#include "brcm_rdb_tx.h"
#include "brcm_rdb_wcdma_gp_timers.h"
#include "brcm_rdb_wcdma_uart.h"

#endif /* __REGISTERS_CHIP_H__ */
