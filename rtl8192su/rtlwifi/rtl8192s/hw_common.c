/******************************************************************************
 *
 * Copyright(c) 2009-2012  Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 * wlanfae <wlanfae@realtek.com>
 * Realtek Corporation, No. 2, Innovation Road II, Hsinchu Science Park,
 * Hsinchu 300, Taiwan.
 *
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 *****************************************************************************/

#include "../wifi.h"
#include "../efuse.h"
#include "../base.h"
#include "../regd.h"
#include "../cam.h"
#include "../ps.h"
#include "../pci.h"
#include "reg_common.h"
#include "def_common.h"
#include "phy_common.h"
#include "dm_common.h"
#include "fw_common.h"
#include "hw_common.h"

void rtl92s_get_hw_reg(struct ieee80211_hw *hw, u8 variable, u8 *val)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	struct rtl_ps_ctl *ppsc = rtl_psc(rtl_priv(hw));
	struct rtl_hal *rtlhal = rtl_hal(rtl_priv(hw));

	switch (variable) {
	case HW_VAR_RCR: {
			struct rtl_pci *rtlpci;
			if (IS_HARDWARE_TYPE_8192SE(rtlhal)) {
				rtlpci = rtl_pcidev(rtl_pcipriv(hw));
				*((u32 *) (val)) = rtlpci->receive_config;
			} else {
				*((u32 *) (val)) = rtl_read_dword(rtlpriv,
								  RCR);
			}
			break;
		}
	case HW_VAR_RF_STATE: {
			*((enum rf_pwrstate *)(val)) = ppsc->rfpwr_state;
			break;
		}
	case HW_VAR_FW_PSMODE_STATUS: {
			*((bool *) (val)) = ppsc->fw_current_inpsmode;
			break;
		}
	case HW_VAR_CORRECT_TSF: {
			u64 tsf;
			u32 *ptsf_low = (u32 *)&tsf;
			u32 *ptsf_high = ((u32 *)&tsf) + 1;

			*ptsf_high = rtl_read_dword(rtlpriv, (TSFR + 4));
			*ptsf_low = rtl_read_dword(rtlpriv, TSFR);

			*((u64 *) (val)) = tsf;

			break;
		}
	case HW_VAR_MRC: {
			*((bool *)(val)) = rtlpriv->dm.current_mrc_switch;
			break;
		}
	default: {
		RT_TRACE(rtlpriv, COMP_ERR, DBG_EMERG,
			 "switch case not processed\n");
			break;
		}
	}
}
EXPORT_SYMBOL_GPL(rtl92s_get_hw_reg);

void rtl92s_set_mac_addr(struct ieee80211_hw *hw, const u8 *addr)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	rtl_write_dword(rtlpriv, REG_MACID, *((u32 *)addr));
	rtl_write_word(rtlpriv, REG_MACID + 4, *((u16 *)(addr + 4)));
}
EXPORT_SYMBOL_GPL(rtl92s_set_mac_addr);

void rtl92s_set_hw_reg(struct ieee80211_hw *hw, u8 variable, u8 *val)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	struct rtl_mac *mac = rtl_mac(rtl_priv(hw));
	struct rtl_hal *rtlhal = rtl_hal(rtl_priv(hw));
	struct rtl_efuse *rtlefuse = rtl_efuse(rtl_priv(hw));
	struct rtl_ps_ctl *ppsc = rtl_psc(rtl_priv(hw));

	switch (variable) {
	case HW_VAR_ETHER_ADDR:{
			rtl92s_set_mac_addr(hw, val);
			break;
		}
	case HW_VAR_BASIC_RATE:{
			u16 rate_cfg = ((u16 *) val)[0];
			u8 rate_index = 0;

			if (rtlhal->version == VERSION_8192S_ACUT)
				rate_cfg = rate_cfg & 0x150;
			else
				rate_cfg = rate_cfg & 0x15f;

			rate_cfg |= 0x01;

			rtl_write_byte(rtlpriv, RRSR, rate_cfg & 0xff);
			rtl_write_byte(rtlpriv, RRSR + 1,
				       (rate_cfg >> 8) & 0xff);

			while (rate_cfg > 0x1) {
				rate_cfg = (rate_cfg >> 1);
				rate_index++;
			}
			rtl_write_byte(rtlpriv, INIRTSMCS_SEL, rate_index);

			break;
		}
	case HW_VAR_BSSID:{
			rtl_write_dword(rtlpriv, BSSIDR, ((u32 *)(val))[0]);
			rtl_write_word(rtlpriv, BSSIDR + 4,
				       ((u16 *)(val + 4))[0]);
			break;
		}
	case HW_VAR_SIFS:{
			rtl_write_byte(rtlpriv, SIFS_OFDM, val[0]);
			rtl_write_byte(rtlpriv, SIFS_OFDM + 1, val[1]);
			break;
		}
	case HW_VAR_SLOT_TIME:{
			u8 e_aci;

			RT_TRACE(rtlpriv, COMP_MLME, DBG_LOUD,
				 "HW_VAR_SLOT_TIME %x\n", val[0]);

			rtl_write_byte(rtlpriv, SLOT_TIME, val[0]);

			for (e_aci = 0; e_aci < AC_MAX; e_aci++) {
				rtlpriv->cfg->ops->set_hw_reg(hw,
						HW_VAR_AC_PARAM,
						(&e_aci));
			}
			break;
		}
	case HW_VAR_ACK_PREAMBLE:{
			u8 reg_tmp;
			u8 short_preamble = (bool) (*val);
			reg_tmp = (mac->cur_40_prime_sc) << 5;
			if (short_preamble)
				reg_tmp |= 0x80;

			rtl_write_byte(rtlpriv, RRSR + 2, reg_tmp);
			break;
		}
	case HW_VAR_AMPDU_MIN_SPACE:{
			u8 min_spacing_to_set;
			u8 sec_min_space;

			min_spacing_to_set = *val;
			if (min_spacing_to_set <= 7) {
				if (rtlpriv->sec.pairwise_enc_algorithm ==
				    NO_ENCRYPTION)
					sec_min_space = 0;
				else
					sec_min_space = 1;

				if (min_spacing_to_set < sec_min_space)
					min_spacing_to_set = sec_min_space;
				if (min_spacing_to_set > 5)
					min_spacing_to_set = 5;

				mac->min_space_cfg =
						((mac->min_space_cfg & 0xf8) |
						min_spacing_to_set);

				*val = min_spacing_to_set;

				RT_TRACE(rtlpriv, COMP_MLME, DBG_LOUD,
					 "Set HW_VAR_AMPDU_MIN_SPACE: %#x\n",
					 mac->min_space_cfg);

				rtl_write_byte(rtlpriv, AMPDU_MIN_SPACE,
					       mac->min_space_cfg);
			}
			break;
		}
	case HW_VAR_SHORTGI_DENSITY:{
			u8 density_to_set;

			density_to_set = *val;
			mac->min_space_cfg = rtlpriv->rtlhal.minspace_cfg;
			mac->min_space_cfg |= (density_to_set << 3);

			RT_TRACE(rtlpriv, COMP_MLME, DBG_LOUD,
				 "Set HW_VAR_SHORTGI_DENSITY: %#x\n",
				 mac->min_space_cfg);

			rtl_write_byte(rtlpriv, AMPDU_MIN_SPACE,
				       mac->min_space_cfg);

			break;
		}
	case HW_VAR_AMPDU_FACTOR:{
			u8 factor_toset;
			u8 regtoset;
			u8 factorlevel[18] = {
				2, 4, 4, 7, 7, 13, 13,
				13, 2, 7, 7, 13, 13,
				15, 15, 15, 15, 0};
			u8 index = 0;

			factor_toset = *val;
			if (factor_toset <= 3) {
				factor_toset = (1 << (factor_toset + 2));
				if (factor_toset > 0xf)
					factor_toset = 0xf;

				for (index = 0; index < 17; index++) {
					if (factorlevel[index] > factor_toset)
						factorlevel[index] =
								 factor_toset;
				}

				for (index = 0; index < 8; index++) {
					regtoset = ((factorlevel[index * 2]) |
						    (factorlevel[index *
						    2 + 1] << 4));
					rtl_write_byte(rtlpriv,
						       AGGLEN_LMT_L + index,
						       regtoset);
				}

				regtoset = ((factorlevel[16]) |
					    (factorlevel[17] << 4));
				rtl_write_byte(rtlpriv, AGGLEN_LMT_H, regtoset);

				RT_TRACE(rtlpriv, COMP_MLME, DBG_LOUD,
					 "Set HW_VAR_AMPDU_FACTOR: %#x\n",
					 factor_toset);
			}
			break;
		}
	case HW_VAR_AC_PARAM:{
			struct rtl_pci *rtlpci;

			rtl92s_dm_init_edca_turbo(hw);
			if (IS_HARDWARE_TYPE_8192SE(rtlhal)) {
				u8 e_aci = *val;
				rtlpci = rtl_pcidev(rtl_pcipriv(hw));

				if (rtlpci->acm_method != EACMWAY2_SW) {
					rtlpriv->cfg->ops->set_hw_reg(hw,
							 HW_VAR_ACM_CTRL,
							 &e_aci);
				}
			}
			break;
		}
	case HW_VAR_ACM_CTRL:{
			struct rtl_pci *rtlpci;
			u8 e_aci = *val;
			union aci_aifsn *p_aci_aifsn = (union aci_aifsn *)(&(
							mac->ac[0].aifs));
			u8 acm = p_aci_aifsn->f.acm;
			u8 acm_ctrl = rtl_read_byte(rtlpriv, AcmHwCtrl);

			if (!IS_HARDWARE_TYPE_8192SE(rtlhal))
				goto out;

			rtlpci = rtl_pcidev(rtl_pcipriv(hw));

			acm_ctrl = acm_ctrl | ((rtlpci->acm_method == 2) ?
				   0x0 : 0x1);

			if (acm) {
				switch (e_aci) {
				case AC0_BE:
					acm_ctrl |= AcmHw_BeqEn;
					break;
				case AC2_VI:
					acm_ctrl |= AcmHw_ViqEn;
					break;
				case AC3_VO:
					acm_ctrl |= AcmHw_VoqEn;
					break;
				default:
					RT_TRACE(rtlpriv, COMP_ERR, DBG_WARNING,
						 "HW_VAR_ACM_CTRL acm set failed: eACI is %d\n",
						 acm);
					break;
				}
			} else {
				switch (e_aci) {
				case AC0_BE:
					acm_ctrl &= (~AcmHw_BeqEn);
					break;
				case AC2_VI:
					acm_ctrl &= (~AcmHw_ViqEn);
					break;
				case AC3_VO:
					acm_ctrl &= (~AcmHw_BeqEn);
					break;
				default:
					RT_TRACE(rtlpriv, COMP_ERR, DBG_EMERG,
						 "switch case not processed\n");
					break;
				}
			}

			RT_TRACE(rtlpriv, COMP_QOS, DBG_TRACE,
				 "HW_VAR_ACM_CTRL Write 0x%X\n", acm_ctrl);
			rtl_write_byte(rtlpriv, AcmHwCtrl, acm_ctrl);
out:
			break;
		}
	case HW_VAR_RCR:{
			struct rtl_pci *rtlpci;

			rtl_write_dword(rtlpriv, RCR, ((u32 *) (val))[0]);

			if (IS_HARDWARE_TYPE_8192SE(rtlhal)) {
				rtlpci = rtl_pcidev(rtl_pcipriv(hw));
				rtlpci->receive_config = ((u32 *) (val))[0];
			}
			break;
		}
	case HW_VAR_RETRY_LIMIT:{
			u8 retry_limit = val[0];

			rtl_write_word(rtlpriv, RETRY_LIMIT,
				       retry_limit << RETRY_LIMIT_SHORT_SHIFT |
				       retry_limit << RETRY_LIMIT_LONG_SHIFT);
			break;
		}
	case HW_VAR_DUAL_TSF_RST: {
			break;
		}
	case HW_VAR_EFUSE_BYTES: {
			rtlefuse->efuse_usedbytes = *((u16 *) val);
			break;
		}
	case HW_VAR_EFUSE_USAGE: {
			rtlefuse->efuse_usedpercentage = *val;
			break;
		}
	case HW_VAR_IO_CMD: {
			break;
		}
	case HW_VAR_WPA_CONFIG: {
			rtl_write_byte(rtlpriv, REG_SECR, *val);
			break;
		}
	case HW_VAR_SET_RPWM:{
			break;
		}
	case HW_VAR_H2C_FW_PWRMODE:{
			break;
		}
	case HW_VAR_FW_PSMODE_STATUS: {
			ppsc->fw_current_inpsmode = *((bool *) val);
			break;
		}
	case HW_VAR_H2C_FW_JOINBSSRPT:{
			break;
		}
	case HW_VAR_AID:{
			break;
		}
	case HW_VAR_CORRECT_TSF:{
			break;
		}
	case HW_VAR_MRC: {
			bool bmrc_toset = *((bool *)val);
			u8 u1bdata = 0;

			if (bmrc_toset) {
				rtl_set_bbreg(hw, ROFDM0_TRXPATHENABLE,
					      MASKBYTE0, 0x33);
				u1bdata = (u8)rtl_get_bbreg(hw,
						ROFDM1_TRXPATHENABLE,
						MASKBYTE0);
				rtl_set_bbreg(hw, ROFDM1_TRXPATHENABLE,
					      MASKBYTE0,
					      ((u1bdata & 0xf0) | 0x03));
				u1bdata = (u8)rtl_get_bbreg(hw,
						ROFDM0_TRXPATHENABLE,
						MASKBYTE1);
				rtl_set_bbreg(hw, ROFDM0_TRXPATHENABLE,
					      MASKBYTE1,
					      (u1bdata | 0x04));

				/* Update current settings. */
				rtlpriv->dm.current_mrc_switch = bmrc_toset;
			} else {
				rtl_set_bbreg(hw, ROFDM0_TRXPATHENABLE,
					      MASKBYTE0, 0x13);
				u1bdata = (u8)rtl_get_bbreg(hw,
						 ROFDM1_TRXPATHENABLE,
						 MASKBYTE0);
				rtl_set_bbreg(hw, ROFDM1_TRXPATHENABLE,
					      MASKBYTE0,
					      ((u1bdata & 0xf0) | 0x01));
				u1bdata = (u8)rtl_get_bbreg(hw,
						ROFDM0_TRXPATHENABLE,
						MASKBYTE1);
				rtl_set_bbreg(hw, ROFDM0_TRXPATHENABLE,
					      MASKBYTE1, (u1bdata & 0xfb));

				/* Update current settings. */
				rtlpriv->dm.current_mrc_switch = bmrc_toset;
			}

		}
		break;
	case HW_VAR_FW_LPS_ACTION: {
		bool enter_fwlps = *((bool *)val);
		u8 rpwm_val, fw_pwrmode;
		bool fw_current_inps;

		if (enter_fwlps) {
			rpwm_val = 0x02;	/* RF off */
			fw_current_inps = true;
			rtlpriv->cfg->ops->set_hw_reg(hw,
					HW_VAR_FW_PSMODE_STATUS,
					(u8 *)(&fw_current_inps));
			rtlpriv->cfg->ops->set_hw_reg(hw,
					HW_VAR_H2C_FW_PWRMODE,
					(u8 *)(&ppsc->fwctrl_psmode));

			rtlpriv->cfg->ops->set_hw_reg(hw,
					HW_VAR_SET_RPWM,
					(u8 *)(&rpwm_val));
		} else {
			rpwm_val = 0x0C;	/* RF on */
			fw_pwrmode = FW_PS_ACTIVE_MODE;
			fw_current_inps = false;
			rtlpriv->cfg->ops->set_hw_reg(hw, HW_VAR_SET_RPWM,
					(u8 *)(&rpwm_val));
			rtlpriv->cfg->ops->set_hw_reg(hw,
					HW_VAR_H2C_FW_PWRMODE,
					(u8 *)(&fw_pwrmode));

			rtlpriv->cfg->ops->set_hw_reg(hw,
					HW_VAR_FW_PSMODE_STATUS,
					(u8 *)(&fw_current_inps));
		}
		}
		break;
	default:
		RT_TRACE(rtlpriv, COMP_ERR, DBG_EMERG,
			 "switch case not processed\n");
		break;
	}

}
EXPORT_SYMBOL_GPL(rtl92s_set_hw_reg);

void rtl92s_enable_hw_security_config(struct ieee80211_hw *hw)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	u8 sec_reg_value = 0x0;

	RT_TRACE(rtlpriv, COMP_INIT, DBG_LOUD,
		 "PairwiseEncAlgorithm = %d GroupEncAlgorithm = %d\n",
		 rtlpriv->sec.pairwise_enc_algorithm,
		 rtlpriv->sec.group_enc_algorithm);

	if (rtlpriv->cfg->mod_params->sw_crypto || rtlpriv->sec.use_sw_sec) {
		RT_TRACE(rtlpriv, COMP_SEC, DBG_DMESG,
			 "not open hw encryption\n");
		return;
	}

	sec_reg_value = SCR_TXENCENABLE | SCR_RXENCENABLE;

	if (rtlpriv->sec.use_defaultkey) {
		sec_reg_value |= SCR_TXUSEDK;
		sec_reg_value |= SCR_RXUSEDK;
	}

	RT_TRACE(rtlpriv, COMP_SEC, DBG_LOUD, "The SECR-value %x\n",
		 sec_reg_value);

	rtlpriv->cfg->ops->set_hw_reg(hw, HW_VAR_WPA_CONFIG, &sec_reg_value);
}
EXPORT_SYMBOL_GPL(rtl92s_enable_hw_security_config);

bool rtl92s_halset_sysclk(struct ieee80211_hw *hw, u16 clk_set)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	u16 clk;
	u8 tries = 10;
	bool result;

	rtl_write_word(rtlpriv, REG_SYS_CLKR, clk_set);

	/* Wait until the MAC is synchronized. */
	mdelay(20);

	/* Check if it is set ready. */
	clk = rtl_read_word(rtlpriv, SYS_CLKR);
	result = ((clk & SYS_FWHW_SEL) == (clk_set & SYS_FWHW_SEL));

	if (!(clk_set & (SYS_SWHW_SEL | SYS_FWHW_SEL))) {
		do {
			msleep(20);

			clk = rtl_read_word(rtlpriv, SYS_CLKR);
			if ((clk & SYS_SWHW_SEL))
				return true;

			pr_err("wait for SYS_SWHW_SEL return value %x\n", clk);
		} while (--tries);
		return false;
	}
	return result;
}
EXPORT_SYMBOL_GPL(rtl92s_halset_sysclk);

bool rtl92s_rf_onoff_detect(struct ieee80211_hw *hw)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	u8 u1tmp;
	u8 retval = ERFON;

	/* The following config GPIO function */
	rtl_write_byte(rtlpriv, MAC_PINMUX_CFG, (GPIOMUX_EN | GPIOSEL_GPIO));
	u1tmp = rtl_read_byte(rtlpriv, GPIO_IO_SEL);

	/* config GPIO3 to input */
	u1tmp &= HAL_8192S_HW_GPIO_OFF_MASK;
	rtl_write_byte(rtlpriv, GPIO_IO_SEL, u1tmp);

	/* On some of the platform, driver cannot read correct
	 * value without delay between Write_GPIO_SEL and Read_GPIO_IN */
	mdelay(10);

	/* check GPIO3 */
	u1tmp = rtl_read_byte(rtlpriv, GPIO_IN_SE);
	retval = (u1tmp & HAL_8192S_HW_GPIO_OFF_BIT) ? ERFON : ERFOFF;

	return retval;
}
EXPORT_SYMBOL_GPL(rtl92s_rf_onoff_detect);

int rtl92s_set_media_status(struct ieee80211_hw *hw,
			    enum nl80211_iftype type)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	u8 bt_msr = rtl_read_byte(rtlpriv, MSR);
	u32 temp;
	bt_msr &= ~MSR_LINK_MASK;

	switch (type) {
	case NL80211_IFTYPE_UNSPECIFIED:
		bt_msr |= (MSR_LINK_NONE << MSR_LINK_SHIFT);
		RT_TRACE(rtlpriv, COMP_INIT, DBG_TRACE,
			 "Set Network type to NO LINK!\n");
		break;
	case NL80211_IFTYPE_ADHOC:
		bt_msr |= (MSR_LINK_ADHOC << MSR_LINK_SHIFT);
		RT_TRACE(rtlpriv, COMP_INIT, DBG_TRACE,
			 "Set Network type to Ad Hoc!\n");
		break;
	case NL80211_IFTYPE_STATION:
		bt_msr |= (MSR_LINK_MANAGED << MSR_LINK_SHIFT);
		RT_TRACE(rtlpriv, COMP_INIT, DBG_TRACE,
			 "Set Network type to STA!\n");
		break;
	case NL80211_IFTYPE_AP:
		bt_msr |= (MSR_LINK_MASTER << MSR_LINK_SHIFT);
		RT_TRACE(rtlpriv, COMP_INIT, DBG_TRACE,
			 "Set Network type to AP!\n");
		break;
	default:
		RT_TRACE(rtlpriv, COMP_ERR, DBG_EMERG,
			 "Network type %d not supported!\n", type);
		return 1;
		break;

	}

	rtl_write_byte(rtlpriv, (MSR), bt_msr);

	temp = rtl_read_dword(rtlpriv, TCR);
	rtl_write_dword(rtlpriv, TCR, temp & (~BIT(8)));
	rtl_write_dword(rtlpriv, TCR, temp | BIT(8));

	return 0;
}
EXPORT_SYMBOL_GPL(rtl92s_set_media_status);

/* don't set REG_EDCA_BE_PARAM here because mac80211 will send pkt when scan */
void rtl92s_set_qos(struct ieee80211_hw *hw, int aci)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	rtl92s_dm_init_edca_turbo(hw);

	switch (aci) {
	case AC1_BK:
		rtl_write_dword(rtlpriv, EDCAPARA_BK, 0xa44f);
		break;
	case AC0_BE:
		/* rtl_write_dword(rtlpriv, EDCAPARA_BE, u4b_ac_param); */
		break;
	case AC2_VI:
		rtl_write_dword(rtlpriv, EDCAPARA_VI, 0x5e4322);
		break;
	case AC3_VO:
		rtl_write_dword(rtlpriv, EDCAPARA_VO, 0x2f3222);
		break;
	default:
		RT_ASSERT(false, "invalid aci: %d !\n", aci);
		break;
	}
}
EXPORT_SYMBOL_GPL(rtl92s_set_qos);

void rtl92s_phy_set_rfhalt(struct ieee80211_hw *hw)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	struct rtl_hal *rtlhal = rtl_hal(rtl_priv(hw));
	struct rtl_ps_ctl *ppsc = rtl_psc(rtl_priv(hw));
	u8 u1btmp;
	u16 tmpu2b;


	if (IS_HARDWARE_TYPE_8192SU(rtlhal)) {
		rtl_write_word(rtlpriv, REG_CR, 0x0);
		rtl_write_byte(rtlpriv, REG_RF_CTRL, 0x00);
		/* Turn off BB */
		msleep(20);

		/* Turn off MAC */
		/* Switch Control Path */
		rtl_write_byte(rtlpriv, REG_SYS_CLKR+1, 0x38);
		rtl_write_byte(rtlpriv, REG_SYS_FUNC_EN+1, 0x70);
		/* Enable Loader Data Keep */
		rtl_write_byte(rtlpriv, REG_PMC_FSM, 0x06);
		/* Isolation signals from CORE, PLL */
		rtl_write_byte(rtlpriv, REG_SYS_ISO_CTRL, 0xF9);
		/* Enable EFUSE 1.2V */
		rtl_write_byte(rtlpriv, REG_SYS_ISO_CTRL+1, 0xe8);
		/* Disable AFE PLL. */
		rtl_write_byte(rtlpriv, REG_AFE_PLL_CTRL, 0x00);
		/* Disable A15V */
		rtl_write_byte(rtlpriv, REG_LDOA15_CTRL, 0x54);
		/* Disable E-Fuse 1.2V */
		rtl_write_byte(rtlpriv, REG_SYS_FUNC_EN+1, 0x50);
		/* Disable LDO12(for CE) */
		rtl_write_byte(rtlpriv, REG_LDOV12D_CTRL, 0x24);
		/* Disable AFE BG&MB */
		rtl_write_byte(rtlpriv, REG_AFE_MISC, 0x30);
		/* Option for Disable 1.6V LDO. */
		/* Disable 1.6V LDO */
		rtl_write_byte(rtlpriv, REG_SPS0_CTRL, 0x56);
		/* Set SW PFM */
		rtl_write_byte(rtlpriv, REG_SPS0_CTRL+1, 0x43);
		return;
	}

	if (rtlhal->driver_going2unload)
		rtl_write_byte(rtlpriv, 0x560, 0x0);

	/* Power save for BB/RF */
	u1btmp = rtl_read_byte(rtlpriv, REG_LDOV12D_CTRL);
	u1btmp |= BIT(0);
	rtl_write_byte(rtlpriv, REG_LDOV12D_CTRL, u1btmp);
	rtl_write_byte(rtlpriv, REG_SPS1_CTRL, 0x0);
	rtl_write_byte(rtlpriv, REG_TXPAUSE, 0xFF);
	rtl_write_word(rtlpriv, REG_CR, 0x57FC);
	udelay(100);
	rtl_write_word(rtlpriv, REG_CR, 0x77FC);
	rtl_write_byte(rtlpriv, PHY_CCA, 0x0);
	udelay(10);
	rtl_write_word(rtlpriv, REG_CR, 0x37FC);
	udelay(10);
	rtl_write_word(rtlpriv, REG_CR, 0x77FC);
	udelay(10);
	rtl_write_word(rtlpriv, REG_CR, 0x57FC);
	rtl_write_word(rtlpriv, REG_CR, 0x0000);

	if (rtlhal->driver_going2unload) {
		u1btmp = rtl_read_byte(rtlpriv, (REG_SYS_FUNC_EN + 1));
		u1btmp &= ~(BIT(0));
		rtl_write_byte(rtlpriv, REG_SYS_FUNC_EN + 1, u1btmp);
	}

	/* After switch control path. register after page1 will be invisible.
	 * We can not do any IO for register>0x40. After resume&MACIO reset,
	 * we need to remember previous reg content. */
	tmpu2b = rtl_read_word(rtlpriv, REG_SYS_CLKR);
	if (tmpu2b & SYS_FWHW_SEL) {
		tmpu2b &= ~(SYS_SWHW_SEL | SYS_FWHW_SEL);

		/* Set failed, return to prevent hang. */
		if (!rtl92s_halset_sysclk(hw, tmpu2b)) {
			pr_err("Switch ctrl path fail\n");
			return;
		}
	}

	/* Power save for MAC */
	if (ppsc->rfoff_reason == RF_CHANGE_BY_IPS  &&
		!rtlhal->driver_going2unload) {
		/* enable LED function */
		rtl_write_byte(rtlpriv, 0x03, 0xF9);
	/* SW/HW radio off or halt adapter!! For example S3/S4 */
	} else {
		/* LED function disable. Power range is about 8mA now. */
		/* if write 0xF1 disconnet_pci power
		 *	 ifconfig wlan0 down power are both high 35:70 */
		/* if write oxF9 disconnet_pci power
		 * ifconfig wlan0 down power are both low  12:45*/
		rtl_write_byte(rtlpriv, 0x03, 0xF9);
	}

	rtl_write_byte(rtlpriv, REG_SYS_CLKR + 1, 0x70);
	rtl_write_byte(rtlpriv, REG_AFE_PLL_CTRL + 1, 0x68);
	rtl_write_byte(rtlpriv, REG_AFE_PLL_CTRL, 0x00);
	rtl_write_byte(rtlpriv, REG_LDOA15_CTRL, 0x34);
	rtl_write_byte(rtlpriv, REG_AFE_XTAL_CTRL, 0x0E);
	RT_SET_PS_LEVEL(ppsc, RT_RF_OFF_LEVL_HALT_NIC);
}
EXPORT_SYMBOL_GPL(rtl92s_phy_set_rfhalt);

void rtl92s_set_beacon_related_registers(struct ieee80211_hw *hw)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	struct rtl_hal *rtlhal = rtl_hal(rtl_priv(hw));
	struct rtl_mac *mac = rtl_mac(rtl_priv(hw));
	u16 bcntime_cfg = 0;
	u16 bcn_cw = 6, bcn_ifs = 0xf;
	u16 atim_window = 2;

	/* ATIM Window (in unit of TU). */
	rtl_write_word(rtlpriv, ATIMWND, atim_window);


	if (IS_HARDWARE_TYPE_8192SE(rtlhal)) {
		rtl_write_word(rtlpriv, BCN_DRV_EARLY_INT, 10 << 4);
		/* Beacon interval (in unit of TU). */

		/* DrvErlyInt (in unit of TU). (Time to send
		 * interrupt to notify driver to change
		 * beacon content) */
		rtl_write_word(rtlpriv, BCN_INTERVAL, mac->beacon_interval);
	} else {
		rtl_write_word(rtlpriv, BCN_DRV_EARLY_INT, 0);
		/* Beacon interval is set by the firmware */
	}

	/* BcnDMATIM(in unit of us). Indicates the
	 * time before TBTT to perform beacon queue DMA  */
	rtl_write_word(rtlpriv, BCN_DMATIME, 256);

	/* Force beacon frame transmission even
	 * after receiving beacon frame from
	 * other ad hoc STA */
	rtl_write_byte(rtlpriv, BCN_ERR_THRESH, 100);

	/* Beacon Time Configuration */
	if (mac->opmode == NL80211_IFTYPE_ADHOC)
		bcntime_cfg |= (bcn_cw << BCN_TCFG_CW_SHIFT);

	/* TODO: bcn_ifs may required to be changed on ASIC */
	bcntime_cfg |= bcn_ifs << BCN_TCFG_IFS;

	/*for beacon changed */
	rtl92s_phy_set_beacon_hwreg(hw, mac->beacon_interval);

	RT_TRACE(rtlpriv, COMP_SEC, DBG_TRACE, "Beacon Interval %d TU\n",
		 mac->beacon_interval);
}
EXPORT_SYMBOL_GPL(rtl92s_set_beacon_related_registers);

void rtl92s_set_beacon_interval(struct ieee80211_hw *hw)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	struct rtl_mac *mac = rtl_mac(rtl_priv(hw));
	u16 bcn_interval = mac->beacon_interval;

	/* Beacon interval (in unit of TU). */
	rtl_write_word(rtlpriv, BCN_INTERVAL, bcn_interval);
	/* 2008.10.24 added by tynli for beacon changed. */
	rtl92s_phy_set_beacon_hwreg(hw, bcn_interval);
}
EXPORT_SYMBOL_GPL(rtl92s_set_beacon_interval);

static void _rtl92s_get_IC_Inferiority(struct ieee80211_hw *hw)
{
	struct rtl_efuse *rtlefuse = rtl_efuse(rtl_priv(hw));
	struct rtl_hal *rtlhal = rtl_hal(rtl_priv(hw));
	u8 efuse_id;

	rtlhal->ic_class = IC_INFERIORITY_A;

	/* Only retrieving while using EFUSE. */
	if ((rtlefuse->epromtype == EEPROM_BOOT_EFUSE) &&
		!rtlefuse->autoload_failflag) {
		efuse_id = efuse_read_1byte(hw, EFUSE_IC_ID_OFFSET);

		if (efuse_id == 0xfe)
			rtlhal->ic_class = IC_INFERIORITY_B;
	}
}

void rtl92s_read_chip_version(struct ieee80211_hw *hw)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	struct rtl_hal *rtlhal = rtl_hal(rtl_priv(hw));

	rtlhal->version = (rtl_read_dword(rtlpriv, PMC_FSM) & 0xf8000) >> 15;
	pr_info("Chip version 0x%x\n", rtlhal->version);

	switch (rtlpriv->rtlhal.interface) {
	case INTF_USB:
		rtlpriv->rtlhal.hw_type = HARDWARE_TYPE_RTL8192SU;
		break;
	case INTF_PCI:
		rtlpriv->rtlhal.hw_type = HARDWARE_TYPE_RTL8192SE;
		break;
	}
}
EXPORT_SYMBOL_GPL(rtl92s_read_chip_version);

static void _rtl92s_read_adapter_info(struct ieee80211_hw *hw)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	struct rtl_efuse *rtlefuse = rtl_efuse(rtlpriv);
	struct rtl_phy *rtlphy = &(rtlpriv->phy);
	u16 i, usvalue;
	u16	eeprom_id;
	u8 tempval;
	u8 hwinfo[HWSET_MAX_SIZE_92S];
	u8 rf_path, index;

	if (rtlefuse->epromtype == EEPROM_93C46) {
		RT_TRACE(rtlpriv, COMP_ERR, DBG_EMERG,
			 "RTL819X Not boot from eeprom, check it !!\n");
	} else if (rtlefuse->epromtype == EEPROM_BOOT_EFUSE) {
		rtl_efuse_shadow_map_update(hw);
		memcpy((void *)hwinfo, (void *)
			&rtlefuse->efuse_map[EFUSE_INIT_MAP][0],
			HWSET_MAX_SIZE_92S);
	}

	RT_PRINT_DATA(rtlpriv, COMP_INIT, DBG_DMESG, "MAP",
		      hwinfo, HWSET_MAX_SIZE_92S);

	eeprom_id = *((u16 *)&hwinfo[0]);
	if (eeprom_id != RTL8190_EEPROM_ID) {
		RT_TRACE(rtlpriv, COMP_ERR, DBG_WARNING,
			 "EEPROM ID(%#x) is invalid!!\n", eeprom_id);
		rtlefuse->autoload_failflag = true;
	} else {
		RT_TRACE(rtlpriv, COMP_INIT, DBG_LOUD, "Autoload OK\n");
		rtlefuse->autoload_failflag = false;
	}

	if (rtlefuse->autoload_failflag)
		return;

	_rtl92s_get_IC_Inferiority(hw);

	/* Read IC Version && Channel Plan */
	/* VID, DID	 SE	0xA-D */
	rtlefuse->eeprom_vid = *(u16 *)&hwinfo[EEPROM_VID];
	rtlefuse->eeprom_did = *(u16 *)&hwinfo[EEPROM_DID];
	rtlefuse->eeprom_svid = *(u16 *)&hwinfo[EEPROM_SVID];
	rtlefuse->eeprom_smid = *(u16 *)&hwinfo[EEPROM_SMID];
	rtlefuse->eeprom_version = *(u16 *)&hwinfo[EEPROM_VERSION];

	RT_TRACE(rtlpriv, COMP_INIT, DBG_LOUD,
		 "EEPROMId = 0x%4x\n", eeprom_id);
	RT_TRACE(rtlpriv, COMP_INIT, DBG_LOUD,
		 "EEPROM VID = 0x%4x\n", rtlefuse->eeprom_vid);
	RT_TRACE(rtlpriv, COMP_INIT, DBG_LOUD,
		 "EEPROM DID = 0x%4x\n", rtlefuse->eeprom_did);
	RT_TRACE(rtlpriv, COMP_INIT, DBG_LOUD,
		 "EEPROM SVID = 0x%4x\n", rtlefuse->eeprom_svid);
	RT_TRACE(rtlpriv, COMP_INIT, DBG_LOUD,
		 "EEPROM SMID = 0x%4x\n", rtlefuse->eeprom_smid);

	for (i = 0; i < 6; i += 2) {
		usvalue = *(u16 *)&hwinfo[EEPROM_MAC_ADDR + i];
		*((u16 *) (&rtlefuse->dev_addr[i])) = usvalue;
	}

	for (i = 0; i < 6; i++)
		rtl_write_byte(rtlpriv, MACIDR0 + i, rtlefuse->dev_addr[i]);

	RT_TRACE(rtlpriv, COMP_INIT, DBG_DMESG, "%pM\n", rtlefuse->dev_addr);

	/* Get Tx Power Level by Channel */
	/* Read Tx power of Channel 1 ~ 14 from EEPROM. */
	/* 92S suupport RF A & B */
	for (rf_path = 0; rf_path < 2; rf_path++) {
		for (i = 0; i < 3; i++) {
			/* Read CCK RF A & B Tx power  */
			rtlefuse->eeprom_chnlarea_txpwr_cck[rf_path][i] =
			hwinfo[EEPROM_TXPOWERBASE + rf_path * 3 + i];

			/* Read OFDM RF A & B Tx power for 1T */
			rtlefuse->eeprom_chnlarea_txpwr_ht40_1s[rf_path][i] =
			hwinfo[EEPROM_TXPOWERBASE + 6 + rf_path * 3 + i];

			/* Read OFDM RF A & B Tx power for 2T */
			rtlefuse->eprom_chnl_txpwr_ht40_2sdf[rf_path][i]
				 = hwinfo[EEPROM_TXPOWERBASE + 12 +
				   rf_path * 3 + i];
		}
	}

	for (rf_path = 0; rf_path < 2; rf_path++)
		for (i = 0; i < 3; i++)
			RTPRINT(rtlpriv, FINIT, INIT_EEPROM,
				"RF(%d) EEPROM CCK Area(%d) = 0x%x\n",
				rf_path, i,
				rtlefuse->eeprom_chnlarea_txpwr_cck
				[rf_path][i]);
	for (rf_path = 0; rf_path < 2; rf_path++)
		for (i = 0; i < 3; i++)
			RTPRINT(rtlpriv, FINIT, INIT_EEPROM,
				"RF(%d) EEPROM HT40 1S Area(%d) = 0x%x\n",
				rf_path, i,
				rtlefuse->eeprom_chnlarea_txpwr_ht40_1s
				[rf_path][i]);
	for (rf_path = 0; rf_path < 2; rf_path++)
		for (i = 0; i < 3; i++)
			RTPRINT(rtlpriv, FINIT, INIT_EEPROM,
				"RF(%d) EEPROM HT40 2S Diff Area(%d) = 0x%x\n",
				rf_path, i,
				rtlefuse->eprom_chnl_txpwr_ht40_2sdf
				[rf_path][i]);

	for (rf_path = 0; rf_path < 2; rf_path++) {

		/* Assign dedicated channel tx power */
		for (i = 0; i < 14; i++)	{
			/* channel 1~3 use the same Tx Power Level. */
			if (i < 3)
				index = 0;
			/* Channel 4-8 */
			else if (i < 8)
				index = 1;
			/* Channel 9-14 */
			else
				index = 2;

			/* Record A & B CCK /OFDM - 1T/2T Channel area
			 * tx power */
			rtlefuse->txpwrlevel_cck[rf_path][i]  =
				rtlefuse->eeprom_chnlarea_txpwr_cck
							[rf_path][index];
			rtlefuse->txpwrlevel_ht40_1s[rf_path][i]  =
				rtlefuse->eeprom_chnlarea_txpwr_ht40_1s
							[rf_path][index];
			rtlefuse->txpwrlevel_ht40_2s[rf_path][i]  =
				rtlefuse->eprom_chnl_txpwr_ht40_2sdf
							[rf_path][index];
		}

		for (i = 0; i < 14; i++) {
			RTPRINT(rtlpriv, FINIT, INIT_TXPOWER,
				"RF(%d)-Ch(%d) [CCK / HT40_1S / HT40_2S] = [0x%x / 0x%x / 0x%x]\n",
				rf_path, i,
				rtlefuse->txpwrlevel_cck[rf_path][i],
				rtlefuse->txpwrlevel_ht40_1s[rf_path][i],
				rtlefuse->txpwrlevel_ht40_2s[rf_path][i]);
		}
	}

	for (rf_path = 0; rf_path < 2; rf_path++) {
		for (i = 0; i < 3; i++) {
			/* Read Power diff limit. */
			rtlefuse->eeprom_pwrgroup[rf_path][i] =
				hwinfo[EEPROM_TXPWRGROUP + rf_path * 3 + i];
		}
	}

	for (rf_path = 0; rf_path < 2; rf_path++) {
		/* Fill Pwr group */
		for (i = 0; i < 14; i++) {
			/* Chanel 1-3 */
			if (i < 3)
				index = 0;
			/* Channel 4-8 */
			else if (i < 8)
				index = 1;
			/* Channel 9-13 */
			else
				index = 2;

			rtlefuse->pwrgroup_ht20[rf_path][i] =
				(rtlefuse->eeprom_pwrgroup[rf_path][index] &
				0xf);
			rtlefuse->pwrgroup_ht40[rf_path][i] =
				((rtlefuse->eeprom_pwrgroup[rf_path][index] &
				0xf0) >> 4);

			RTPRINT(rtlpriv, FINIT, INIT_TXPOWER,
				"RF-%d pwrgroup_ht20[%d] = 0x%x\n",
				rf_path, i,
				rtlefuse->pwrgroup_ht20[rf_path][i]);
			RTPRINT(rtlpriv, FINIT, INIT_TXPOWER,
				"RF-%d pwrgroup_ht40[%d] = 0x%x\n",
				rf_path, i,
				rtlefuse->pwrgroup_ht40[rf_path][i]);
			}
	}

	for (i = 0; i < 14; i++) {
		/* Read tx power difference between HT OFDM 20/40 MHZ */
		/* channel 1-3 */
		if (i < 3)
			index = 0;
		/* Channel 4-8 */
		else if (i < 8)
			index = 1;
		/* Channel 9-14 */
		else
			index = 2;

		tempval = hwinfo[EEPROM_TX_PWR_HT20_DIFF + index] & 0xff;
		rtlefuse->txpwr_ht20diff[RF90_PATH_A][i] = (tempval & 0xF);
		rtlefuse->txpwr_ht20diff[RF90_PATH_B][i] =
						 ((tempval >> 4) & 0xF);

		/* Read OFDM<->HT tx power diff */
		/* Channel 1-3 */
		if (i < 3)
			index = 0;
		/* Channel 4-8 */
		else if (i < 8)
			index = 0x11;
		/* Channel 9-14 */
		else
			index = 1;

		tempval = hwinfo[EEPROM_TX_PWR_OFDM_DIFF + index] & 0xff;
		rtlefuse->txpwr_legacyhtdiff[RF90_PATH_A][i] =
				 (tempval & 0xF);
		rtlefuse->txpwr_legacyhtdiff[RF90_PATH_B][i] =
				 ((tempval >> 4) & 0xF);

		tempval = hwinfo[TX_PWR_SAFETY_CHK];
		rtlefuse->txpwr_safetyflag = (tempval & 0x01);
	}

	rtlefuse->eeprom_regulatory = 0;
	if (rtlefuse->eeprom_version >= 2) {
		/* BIT(0)~2 */
		if (rtlefuse->eeprom_version >= 4)
			rtlefuse->eeprom_regulatory =
				 (hwinfo[EEPROM_REGULATORY] & 0x7);
		else /* BIT(0) */
			rtlefuse->eeprom_regulatory =
				 (hwinfo[EEPROM_REGULATORY] & 0x1);
	}
	RTPRINT(rtlpriv, FINIT, INIT_TXPOWER,
		"eeprom_regulatory = 0x%x\n", rtlefuse->eeprom_regulatory);

	for (i = 0; i < 14; i++)
		RTPRINT(rtlpriv, FINIT, INIT_TXPOWER,
			"RF-A Ht20 to HT40 Diff[%d] = 0x%x\n",
			i, rtlefuse->txpwr_ht20diff[RF90_PATH_A][i]);
	for (i = 0; i < 14; i++)
		RTPRINT(rtlpriv, FINIT, INIT_TXPOWER,
			"RF-A Legacy to Ht40 Diff[%d] = 0x%x\n",
			i, rtlefuse->txpwr_legacyhtdiff[RF90_PATH_A][i]);
	for (i = 0; i < 14; i++)
		RTPRINT(rtlpriv, FINIT, INIT_TXPOWER,
			"RF-B Ht20 to HT40 Diff[%d] = 0x%x\n",
			i, rtlefuse->txpwr_ht20diff[RF90_PATH_B][i]);
	for (i = 0; i < 14; i++)
		RTPRINT(rtlpriv, FINIT, INIT_TXPOWER,
			"RF-B Legacy to HT40 Diff[%d] = 0x%x\n",
			i, rtlefuse->txpwr_legacyhtdiff[RF90_PATH_B][i]);

	RTPRINT(rtlpriv, FINIT, INIT_TXPOWER,
		"TxPwrSafetyFlag = %d\n", rtlefuse->txpwr_safetyflag);

	/* Read RF-indication and Tx Power gain
	 * index diff of legacy to HT OFDM rate. */
	tempval = hwinfo[EEPROM_RFIND_POWERDIFF] & 0xff;
	rtlefuse->eeprom_txpowerdiff = tempval;
	rtlefuse->legacy_httxpowerdiff =
		rtlefuse->txpwr_legacyhtdiff[RF90_PATH_A][0];

	RTPRINT(rtlpriv, FINIT, INIT_TXPOWER,
		"TxPowerDiff = %#x\n", rtlefuse->eeprom_txpowerdiff);

	/* Get TSSI value for each path. */
	usvalue = *(u16 *)&hwinfo[EEPROM_TSSI_A];
	rtlefuse->eeprom_tssi[RF90_PATH_A] = (u8)((usvalue & 0xff00) >> 8);
	usvalue = hwinfo[EEPROM_TSSI_B];
	rtlefuse->eeprom_tssi[RF90_PATH_B] = (u8)(usvalue & 0xff);

	RTPRINT(rtlpriv, FINIT, INIT_TXPOWER, "TSSI_A = 0x%x, TSSI_B = 0x%x\n",
		rtlefuse->eeprom_tssi[RF90_PATH_A],
		rtlefuse->eeprom_tssi[RF90_PATH_B]);

	/* Read antenna tx power offset of B/C/D to A  from EEPROM */
	/* and read ThermalMeter from EEPROM */
	tempval = hwinfo[EEPROM_THERMALMETER];
	rtlefuse->eeprom_thermalmeter = tempval;
	RTPRINT(rtlpriv, FINIT, INIT_TXPOWER,
		"thermalmeter = 0x%x\n", rtlefuse->eeprom_thermalmeter);

	/* ThermalMeter, BIT(0)~3 for RFIC1, BIT(4)~7 for RFIC2 */
	rtlefuse->thermalmeter[0] = (rtlefuse->eeprom_thermalmeter & 0x1f);
	rtlefuse->tssi_13dbm = rtlefuse->eeprom_thermalmeter * 100;

	/* Read CrystalCap from EEPROM */
	tempval = hwinfo[EEPROM_CRYSTALCAP] >> 4;
	rtlefuse->eeprom_crystalcap = tempval;
	/* CrystalCap, BIT(12)~15 */
	rtlefuse->crystalcap = rtlefuse->eeprom_crystalcap;

	/* Read IC Version && Channel Plan */
	/* Version ID, Channel plan */
	rtlefuse->eeprom_channelplan = hwinfo[EEPROM_CHANNELPLAN];
	rtlefuse->txpwr_fromeprom = true;
	RTPRINT(rtlpriv, FINIT, INIT_TXPOWER,
		"EEPROM ChannelPlan = 0x%4x\n", rtlefuse->eeprom_channelplan);

	/* Read Customer ID or Board Type!!! */
	tempval = hwinfo[EEPROM_BOARDTYPE];
	/* Change RF type definition */
	if (tempval == 0)
		rtlphy->rf_type = RF_1T1R;
	else if (tempval == 1)
		rtlphy->rf_type = RF_1T2R;
	else if (tempval == 2)
		rtlphy->rf_type = RF_2T2R;
	else if (tempval == 3)
		rtlphy->rf_type = RF_1T1R;

	/* 1T2R but 1SS (1x1 receive combining) */
	rtlefuse->b1x1_recvcombine = false;
	if (rtlphy->rf_type == RF_1T2R) {
		tempval = rtl_read_byte(rtlpriv, 0x07);
		if (!(tempval & BIT(0))) {
			rtlefuse->b1x1_recvcombine = true;
			RT_TRACE(rtlpriv, COMP_INIT, DBG_LOUD,
				 "RF_TYPE=1T2R but only 1SS\n");
		}
	}
	rtlefuse->b1ss_support = rtlefuse->b1x1_recvcombine;
	rtlefuse->eeprom_oemid = *&hwinfo[EEPROM_CUSTOMID];

	RT_TRACE(rtlpriv, COMP_INIT, DBG_LOUD, "EEPROM Customer ID: 0x%2x",
		 rtlefuse->eeprom_oemid);

	/* set channel paln to world wide 13 */
	rtlefuse->channel_plan = COUNTRY_CODE_WORLD_WIDE_13;
}

void rtl92s_read_eeprom_info(struct ieee80211_hw *hw)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	struct rtl_efuse *rtlefuse = rtl_efuse(rtl_priv(hw));
	u8 tmp_u1b = 0;

	tmp_u1b = rtl_read_byte(rtlpriv, EPROM_CMD);

	if (tmp_u1b & BIT(4)) {
		RT_TRACE(rtlpriv, COMP_INIT, DBG_DMESG, "Boot from EEPROM\n");
		rtlefuse->epromtype = EEPROM_93C46;
	} else {
		RT_TRACE(rtlpriv, COMP_INIT, DBG_DMESG, "Boot from EFUSE\n");
		rtlefuse->epromtype = EEPROM_BOOT_EFUSE;
	}

	if (tmp_u1b & BIT(5)) {
		RT_TRACE(rtlpriv, COMP_INIT, DBG_LOUD, "Autoload OK\n");
		rtlefuse->autoload_failflag = false;
		_rtl92s_read_adapter_info(hw);
	} else {
		RT_TRACE(rtlpriv, COMP_ERR, DBG_EMERG, "Autoload ERR!!\n");
		rtlefuse->autoload_failflag = true;
	}
}
EXPORT_SYMBOL_GPL(rtl92s_read_eeprom_info);

static void rtl92s_update_hal_rate_table(struct ieee80211_hw *hw,
					 struct ieee80211_sta *sta)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	struct rtl_phy *rtlphy = &(rtlpriv->phy);
	struct rtl_mac *mac = rtl_mac(rtl_priv(hw));
	struct rtl_hal *rtlhal = rtl_hal(rtl_priv(hw));
	u32 ratr_value;
	u8 ratr_index = 0;
	u8 nmode = mac->ht_enable;
	u8 mimo_ps = IEEE80211_SMPS_OFF;
	u16 shortgi_rate = 0;
	u32 tmp_ratr_value = 0;
	u8 curtxbw_40mhz = mac->bw_40;
	u8 curshortgi_40mhz = (sta->ht_cap.cap & IEEE80211_HT_CAP_SGI_40) ?
				1 : 0;
	u8 curshortgi_20mhz = (sta->ht_cap.cap & IEEE80211_HT_CAP_SGI_20) ?
				1 : 0;
	enum wireless_mode wirelessmode = mac->mode;

	if (rtlhal->current_bandtype == BAND_ON_5G)
		ratr_value = sta->supp_rates[1] << 4;
	else
		ratr_value = sta->supp_rates[0];
	if (mac->opmode == NL80211_IFTYPE_ADHOC)
		ratr_value = 0xfff;
	ratr_value |= (sta->ht_cap.mcs.rx_mask[1] << 20 |
			sta->ht_cap.mcs.rx_mask[0] << 12);
	switch (wirelessmode) {
	case WIRELESS_MODE_B:
		ratr_value &= 0x0000000D;
		break;
	case WIRELESS_MODE_G:
		ratr_value &= 0x00000FF5;
		break;
	case WIRELESS_MODE_N_24G:
	case WIRELESS_MODE_N_5G:
		nmode = 1;
		if (mimo_ps == IEEE80211_SMPS_STATIC) {
			ratr_value &= 0x0007F005;
		} else {
			u32 ratr_mask;

			if (get_rf_type(rtlphy) == RF_1T2R ||
			    get_rf_type(rtlphy) == RF_1T1R) {
				if (curtxbw_40mhz)
					ratr_mask = 0x000ff015;
				else
					ratr_mask = 0x000ff005;
			} else {
				if (curtxbw_40mhz)
					ratr_mask = 0x0f0ff015;
				else
					ratr_mask = 0x0f0ff005;
			}

			ratr_value &= ratr_mask;
		}
		break;
	default:
		if (rtlphy->rf_type == RF_1T2R)
			ratr_value &= 0x000ff0ff;
		else
			ratr_value &= 0x0f0ff0ff;

		break;
	}

	if (rtlpriv->rtlhal.version >= VERSION_8192S_BCUT)
		ratr_value &= 0x0FFFFFFF;
	else if (rtlpriv->rtlhal.version == VERSION_8192S_ACUT)
		ratr_value &= 0x0FFFFFF0;

	if (nmode && ((curtxbw_40mhz &&
			 curshortgi_40mhz) || (!curtxbw_40mhz &&
						 curshortgi_20mhz))) {

		ratr_value |= 0x10000000;
		tmp_ratr_value = (ratr_value >> 12);

		for (shortgi_rate = 15; shortgi_rate > 0; shortgi_rate--) {
			if ((1 << shortgi_rate) & tmp_ratr_value)
				break;
		}

		shortgi_rate = (shortgi_rate << 12) | (shortgi_rate << 8) |
		    (shortgi_rate << 4) | (shortgi_rate);

		rtl_write_byte(rtlpriv, SG_RATE, shortgi_rate);
	}

	rtl_write_dword(rtlpriv, ARFR0 + ratr_index * 4, ratr_value);
	if (ratr_value & 0xfffff000)
		rtl92s_phy_set_fw_cmd(hw, FW_CMD_RA_REFRESH_N);
	else
		rtl92s_phy_set_fw_cmd(hw, FW_CMD_RA_REFRESH_BG);

	if (IS_HARDWARE_TYPE_8192SU(rtlhal)) {
		/* The station is protected by rcu, hence rtl_read_* can't be
		 * used, as it would block. However, this is just a debug code
		 * so don't bother.
		 */
		RT_TRACE(rtlpriv, COMP_RATR, DBG_DMESG, "\n");
	} else {
		RT_TRACE(rtlpriv, COMP_RATR, DBG_DMESG, "%x\n",
			 rtl_read_dword(rtlpriv, ARFR0));
	}
}

static void rtl92s_update_hal_rate_mask(struct ieee80211_hw *hw,
					struct ieee80211_sta *sta,
					u8 rssi_level)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	struct rtl_phy *rtlphy = &(rtlpriv->phy);
	struct rtl_mac *mac = rtl_mac(rtl_priv(hw));
	struct rtl_hal *rtlhal = rtl_hal(rtl_priv(hw));
	struct rtl_sta_info *sta_entry = NULL;
	u32 ratr_bitmap;
	u8 ratr_index = 0;
	u8 curtxbw_40mhz = (sta->bandwidth >= IEEE80211_STA_RX_BW_40) ? 1 : 0;
	u8 curshortgi_40mhz = (sta->ht_cap.cap & IEEE80211_HT_CAP_SGI_40) ?
				1 : 0;
	u8 curshortgi_20mhz = (sta->ht_cap.cap & IEEE80211_HT_CAP_SGI_20) ?
				1 : 0;
	enum wireless_mode wirelessmode = 0;
	bool shortgi = false;
	u32 ratr_value = 0;
	u8 shortgi_rate = 0;
	u32 mask = 0;
	u32 band = 0;
	bool bmulticast = false;
	u8 macid = 0;
	u8 mimo_ps = IEEE80211_SMPS_OFF;

	sta_entry = (struct rtl_sta_info *) sta->drv_priv;
	wirelessmode = sta_entry->wireless_mode;
	if (mac->opmode == NL80211_IFTYPE_STATION)
		curtxbw_40mhz = mac->bw_40;
	else if (mac->opmode == NL80211_IFTYPE_AP ||
		mac->opmode == NL80211_IFTYPE_ADHOC)
		macid = sta->aid + 1;

	if (rtlhal->current_bandtype == BAND_ON_5G)
		ratr_bitmap = sta->supp_rates[1] << 4;
	else
		ratr_bitmap = sta->supp_rates[0];
	if (mac->opmode == NL80211_IFTYPE_ADHOC)
		ratr_bitmap = 0xfff;
	ratr_bitmap |= (sta->ht_cap.mcs.rx_mask[1] << 20 |
			sta->ht_cap.mcs.rx_mask[0] << 12);
	switch (wirelessmode) {
	case WIRELESS_MODE_B:
		band |= WIRELESS_11B;
		ratr_index = RATR_INX_WIRELESS_B;
		if (ratr_bitmap & 0x0000000c)
			ratr_bitmap &= 0x0000000d;
		else
			ratr_bitmap &= 0x0000000f;
		break;
	case WIRELESS_MODE_G:
		band |= (WIRELESS_11G | WIRELESS_11B);
		ratr_index = RATR_INX_WIRELESS_GB;

		if (rssi_level == 1)
			ratr_bitmap &= 0x00000f00;
		else if (rssi_level == 2)
			ratr_bitmap &= 0x00000ff0;
		else
			ratr_bitmap &= 0x00000ff5;
		break;
	case WIRELESS_MODE_A:
		band |= WIRELESS_11A;
		ratr_index = RATR_INX_WIRELESS_A;
		ratr_bitmap &= 0x00000ff0;
		break;
	case WIRELESS_MODE_N_24G:
	case WIRELESS_MODE_N_5G:
		band |= (WIRELESS_11N | WIRELESS_11G | WIRELESS_11B);
		ratr_index = RATR_INX_WIRELESS_NGB;

		if (mimo_ps == IEEE80211_SMPS_STATIC) {
			if (rssi_level == 1)
				ratr_bitmap &= 0x00070000;
			else if (rssi_level == 2)
				ratr_bitmap &= 0x0007f000;
			else
				ratr_bitmap &= 0x0007f005;
		} else {
			if (rtlphy->rf_type == RF_1T2R ||
				rtlphy->rf_type == RF_1T1R) {
				if (rssi_level == 1) {
						ratr_bitmap &= 0x000f0000;
				} else if (rssi_level == 3) {
					ratr_bitmap &= 0x000fc000;
				} else if (rssi_level == 5) {
						ratr_bitmap &= 0x000ff000;
				} else {
					if (curtxbw_40mhz)
						ratr_bitmap &= 0x000ff015;
					else
						ratr_bitmap &= 0x000ff005;
				}
			} else {
				if (rssi_level == 1) {
					ratr_bitmap &= 0x0f8f0000;
				} else if (rssi_level == 3) {
					ratr_bitmap &= 0x0f8fc000;
				} else if (rssi_level == 5) {
					ratr_bitmap &= 0x0f8ff000;
				} else {
					if (curtxbw_40mhz)
						ratr_bitmap &= 0x0f8ff015;
					else
						ratr_bitmap &= 0x0f8ff005;
				}
			}
		}

		if ((curtxbw_40mhz && curshortgi_40mhz) ||
		    (!curtxbw_40mhz && curshortgi_20mhz)) {
			if (macid == 0)
				shortgi = true;
			else if (macid == 1)
				shortgi = false;
		}
		break;
	default:
		band |= (WIRELESS_11N | WIRELESS_11G | WIRELESS_11B);
		ratr_index = RATR_INX_WIRELESS_NGB;

		if (rtlphy->rf_type == RF_1T2R)
			ratr_bitmap &= 0x000ff0ff;
		else
			ratr_bitmap &= 0x0f8ff0ff;
		break;
	}
	sta_entry->ratr_index = ratr_index;

	if (rtlpriv->rtlhal.version >= VERSION_8192S_BCUT)
		ratr_bitmap &= 0x0FFFFFFF;
	else if (rtlpriv->rtlhal.version == VERSION_8192S_ACUT)
		ratr_bitmap &= 0x0FFFFFF0;

	if (shortgi) {
		ratr_bitmap |= 0x10000000;
		/* Get MAX MCS available. */
		ratr_value = (ratr_bitmap >> 12);
		for (shortgi_rate = 15; shortgi_rate > 0; shortgi_rate--) {
			if ((1 << shortgi_rate) & ratr_value)
				break;
		}

		shortgi_rate = (shortgi_rate << 12) | (shortgi_rate << 8) |
			(shortgi_rate << 4) | (shortgi_rate);
		rtl_write_byte(rtlpriv, SG_RATE, shortgi_rate);
	}

	mask |= (bmulticast ? 1 : 0) << 9 | (macid & 0x1f) << 4 | (band & 0xf);

	RT_TRACE(rtlpriv, COMP_RATR, DBG_TRACE, "mask = %x, bitmap = %x\n",
		 mask, ratr_bitmap);
	rtl_write_dword(rtlpriv, 0x2c4, ratr_bitmap);
	rtl_write_dword(rtlpriv, WFM5, (FW_RA_UPDATE_MASK | (mask << 8)));

	if (macid != 0)
		sta_entry->ratr_index = ratr_index;
}

void rtl92s_update_hal_rate_tbl(struct ieee80211_hw *hw,
		struct ieee80211_sta *sta, u8 rssi_level)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);

	if (rtlpriv->dm.useramask)
		rtl92s_update_hal_rate_mask(hw, sta, rssi_level);
	else
		rtl92s_update_hal_rate_table(hw, sta);
}
EXPORT_SYMBOL_GPL(rtl92s_update_hal_rate_tbl);

void rtl92s_update_channel_access_setting(struct ieee80211_hw *hw)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	struct rtl_mac *mac = rtl_mac(rtl_priv(hw));
	u16 sifs_timer;

	rtlpriv->cfg->ops->set_hw_reg(hw, HW_VAR_SLOT_TIME,
				      &mac->slot_time);
	sifs_timer = 0x0e0e;
	rtlpriv->cfg->ops->set_hw_reg(hw, HW_VAR_SIFS, (u8 *)&sifs_timer);

}
EXPORT_SYMBOL_GPL(rtl92s_update_channel_access_setting);

/* Is_wepkey just used for WEP used as group & pairwise key
 * if pairwise is AES ang group is WEP Is_wepkey == false.*/
void rtl92s_set_key(struct ieee80211_hw *hw, u32 key_index, u8 *p_macaddr,
	bool is_group, u8 enc_algo, bool is_wepkey, bool clear_all)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	struct rtl_mac *mac = rtl_mac(rtl_priv(hw));
	struct rtl_efuse *rtlefuse = rtl_efuse(rtl_priv(hw));
	u8 *macaddr = p_macaddr;

	u32 entry_id = 0;
	bool is_pairwise = false;

	static u8 cam_const_addr[4][6] = {
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x01},
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x02},
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x03}
	};
	static u8 cam_const_broad[] = {
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff
	};

	if (clear_all) {
		u8 idx = 0;
		u8 cam_offset = 0;
		u8 clear_number = 5;

		RT_TRACE(rtlpriv, COMP_SEC, DBG_DMESG, "clear_all\n");

		for (idx = 0; idx < clear_number; idx++) {
			rtl_cam_mark_invalid(hw, cam_offset + idx);
			rtl_cam_empty_entry(hw, cam_offset + idx);

			if (idx < 5) {
				memset(rtlpriv->sec.key_buf[idx], 0,
				       MAX_KEY_LEN);
				rtlpriv->sec.key_len[idx] = 0;
			}
		}

	} else {
		switch (enc_algo) {
		case WEP40_ENCRYPTION:
			enc_algo = CAM_WEP40;
			break;
		case WEP104_ENCRYPTION:
			enc_algo = CAM_WEP104;
			break;
		case TKIP_ENCRYPTION:
			enc_algo = CAM_TKIP;
			break;
		case AESCCMP_ENCRYPTION:
			enc_algo = CAM_AES;
			break;
		default:
			RT_TRACE(rtlpriv, COMP_ERR, DBG_EMERG,
				 "switch case not processed\n");
			enc_algo = CAM_TKIP;
			break;
		}

		if (is_wepkey || rtlpriv->sec.use_defaultkey) {
			macaddr = cam_const_addr[key_index];
			entry_id = key_index;
		} else {
			if (is_group) {
				macaddr = cam_const_broad;
				entry_id = key_index;
			} else {
				if (mac->opmode == NL80211_IFTYPE_AP) {
					entry_id = rtl_cam_get_free_entry(hw,
								 p_macaddr);
					if (entry_id >=  TOTAL_CAM_ENTRY) {
						RT_TRACE(rtlpriv,
							 COMP_SEC, DBG_EMERG,
							 "Can not find free hw security cam entry\n");
						return;
					}
				} else {
					entry_id = CAM_PAIRWISE_KEY_POSITION;
				}

				key_index = PAIRWISE_KEYIDX;
				is_pairwise = true;
			}
		}

		if (rtlpriv->sec.key_len[key_index] == 0) {
			RT_TRACE(rtlpriv, COMP_SEC, DBG_DMESG,
				 "delete one entry, entry_id is %d\n",
				 entry_id);
			if (mac->opmode == NL80211_IFTYPE_AP)
				rtl_cam_del_entry(hw, p_macaddr);
			rtl_cam_delete_one_entry(hw, p_macaddr, entry_id);
		} else {
			RT_TRACE(rtlpriv, COMP_SEC, DBG_DMESG,
				 "add one entry\n");
			if (is_pairwise) {
				RT_TRACE(rtlpriv, COMP_SEC, DBG_DMESG,
					 "set Pairwise key\n");

				rtl_cam_add_one_entry(hw, macaddr, key_index,
					entry_id, enc_algo,
					CAM_CONFIG_NO_USEDK,
					rtlpriv->sec.key_buf[key_index]);
			} else {
				RT_TRACE(rtlpriv, COMP_SEC, DBG_DMESG,
					 "set group key\n");

				if (mac->opmode == NL80211_IFTYPE_ADHOC) {
					rtl_cam_add_one_entry(hw,
						rtlefuse->dev_addr,
						PAIRWISE_KEYIDX,
						CAM_PAIRWISE_KEY_POSITION,
						enc_algo, CAM_CONFIG_NO_USEDK,
						rtlpriv->sec.key_buf[entry_id]);
				}

				rtl_cam_add_one_entry(hw, macaddr, key_index,
					      entry_id, enc_algo,
					      CAM_CONFIG_NO_USEDK,
					      rtlpriv->sec.key_buf[entry_id]);
			}

		}
	}
}
EXPORT_SYMBOL_GPL(rtl92s_set_key);
