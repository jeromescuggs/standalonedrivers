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
#include "../rtl8192s/reg_common.h"
#include "../rtl8192s/def_common.h"
#include "../rtl8192s/phy_common.h"
#include "../rtl8192s/dm_common.h"
#include "../rtl8192s/fw_common.h"
#include "../rtl8192s/hw_common.h"
#include "../rtl8192s/led_common.h"
#include "hw.h"

static void _rtl92se_macconfig_before_fwdownload(struct ieee80211_hw *hw)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	struct rtl_pci *rtlpci = rtl_pcidev(rtl_pcipriv(hw));
	struct rtl_ps_ctl *ppsc = rtl_psc(rtl_priv(hw));

	u8 i;
	u8 tmpu1b;
	u16 tmpu2b;
	u8 pollingcnt = 20;

	if (rtlpci->first_init) {
		/* Reset PCIE Digital */
		tmpu1b = rtl_read_byte(rtlpriv, REG_SYS_FUNC_EN + 1);
		tmpu1b &= 0xFE;
		rtl_write_byte(rtlpriv, REG_SYS_FUNC_EN + 1, tmpu1b);
		udelay(1);
		rtl_write_byte(rtlpriv, REG_SYS_FUNC_EN + 1, tmpu1b | BIT(0));
	}

	/* Switch to SW IO control */
	tmpu2b = rtl_read_word(rtlpriv, REG_SYS_CLKR);
	if (tmpu2b & SYS_FWHW_SEL) {
		tmpu2b &= ~(SYS_SWHW_SEL | SYS_FWHW_SEL);

		/* Set failed, return to prevent hang. */
		if (!rtl92s_halset_sysclk(hw, tmpu2b))
			return;
	}

	rtl_write_byte(rtlpriv, AFE_PLL_CTRL, 0x0);
	udelay(50);
	rtl_write_byte(rtlpriv, LDOA15_CTRL, 0x34);
	udelay(50);

	/* Clear FW RPWM for FW control LPS.*/
	rtl_write_byte(rtlpriv, RPWM, 0x0);

	/* Reset MAC-IO and CPU and Core Digital BIT(10)/11/15 */
	tmpu1b = rtl_read_byte(rtlpriv, REG_SYS_FUNC_EN + 1);
	tmpu1b &= 0x73;
	rtl_write_byte(rtlpriv, REG_SYS_FUNC_EN + 1, tmpu1b);
	/* wait for BIT 10/11/15 to pull high automatically!! */
	mdelay(1);

	rtl_write_byte(rtlpriv, CMDR, 0);
	rtl_write_byte(rtlpriv, TCR, 0);

	/* Data sheet not define 0x562!!! Copy from WMAC!!!!! */
	tmpu1b = rtl_read_byte(rtlpriv, 0x562);
	tmpu1b |= 0x08;
	rtl_write_byte(rtlpriv, 0x562, tmpu1b);
	tmpu1b &= ~(BIT(3));
	rtl_write_byte(rtlpriv, 0x562, tmpu1b);

	/* Enable AFE clock source */
	tmpu1b = rtl_read_byte(rtlpriv, AFE_XTAL_CTRL);
	rtl_write_byte(rtlpriv, AFE_XTAL_CTRL, (tmpu1b | 0x01));
	/* Delay 1.5ms */
	mdelay(2);
	tmpu1b = rtl_read_byte(rtlpriv, AFE_XTAL_CTRL + 1);
	rtl_write_byte(rtlpriv, AFE_XTAL_CTRL + 1, (tmpu1b & 0xfb));

	/* Enable AFE Macro Block's Bandgap */
	tmpu1b = rtl_read_byte(rtlpriv, AFE_MISC);
	rtl_write_byte(rtlpriv, AFE_MISC, (tmpu1b | BIT(0)));
	mdelay(1);

	/* Enable AFE Mbias */
	tmpu1b = rtl_read_byte(rtlpriv, AFE_MISC);
	rtl_write_byte(rtlpriv, AFE_MISC, (tmpu1b | 0x02));
	mdelay(1);

	/* Enable LDOA15 block	*/
	tmpu1b = rtl_read_byte(rtlpriv, LDOA15_CTRL);
	rtl_write_byte(rtlpriv, LDOA15_CTRL, (tmpu1b | BIT(0)));

	/* Set Digital Vdd to Retention isolation Path. */
	tmpu2b = rtl_read_word(rtlpriv, REG_SYS_ISO_CTRL);
	rtl_write_word(rtlpriv, REG_SYS_ISO_CTRL, (tmpu2b | BIT(11)));

	/* For warm reboot NIC disappera bug. */
	tmpu2b = rtl_read_word(rtlpriv, REG_SYS_FUNC_EN);
	rtl_write_word(rtlpriv, REG_SYS_FUNC_EN, (tmpu2b | BIT(13)));

	rtl_write_byte(rtlpriv, REG_SYS_ISO_CTRL + 1, 0x68);

	/* Enable AFE PLL Macro Block */
	/* We need to delay 100u before enabling PLL. */
	udelay(200);
	tmpu1b = rtl_read_byte(rtlpriv, AFE_PLL_CTRL);
	rtl_write_byte(rtlpriv, AFE_PLL_CTRL, (tmpu1b | BIT(0) | BIT(4)));

	/* for divider reset  */
	udelay(100);
	rtl_write_byte(rtlpriv, AFE_PLL_CTRL, (tmpu1b | BIT(0) |
		       BIT(4) | BIT(6)));
	udelay(10);
	rtl_write_byte(rtlpriv, AFE_PLL_CTRL, (tmpu1b | BIT(0) | BIT(4)));
	udelay(10);

	/* Enable MAC 80MHZ clock  */
	tmpu1b = rtl_read_byte(rtlpriv, AFE_PLL_CTRL + 1);
	rtl_write_byte(rtlpriv, AFE_PLL_CTRL + 1, (tmpu1b | BIT(0)));
	mdelay(1);

	/* Release isolation AFE PLL & MD */
	rtl_write_byte(rtlpriv, REG_SYS_ISO_CTRL, 0xA6);

	/* Enable MAC clock */
	tmpu2b = rtl_read_word(rtlpriv, SYS_CLKR);
	rtl_write_word(rtlpriv, SYS_CLKR, (tmpu2b | BIT(12) | BIT(11)));

	/* Enable Core digital and enable IOREG R/W */
	tmpu2b = rtl_read_word(rtlpriv, REG_SYS_FUNC_EN);
	rtl_write_word(rtlpriv, REG_SYS_FUNC_EN, (tmpu2b | BIT(11)));

	tmpu1b = rtl_read_byte(rtlpriv, REG_SYS_FUNC_EN + 1);
	rtl_write_byte(rtlpriv, REG_SYS_FUNC_EN + 1, tmpu1b & ~(BIT(7)));

	/* enable REG_EN */
	rtl_write_word(rtlpriv, REG_SYS_FUNC_EN, (tmpu2b | BIT(11) | BIT(15)));

	/* Switch the control path. */
	tmpu2b = rtl_read_word(rtlpriv, SYS_CLKR);
	rtl_write_word(rtlpriv, SYS_CLKR, (tmpu2b & (~BIT(2))));

	tmpu2b = rtl_read_word(rtlpriv, REG_SYS_CLKR);
	tmpu2b |= SYS_FWHW_SEL;
	tmpu2b &= ~SYS_SWHW_SEL;
	if (!rtl92s_halset_sysclk(hw, tmpu2b))
		return; /* Set failed, return to prevent hang. */

	rtl_write_word(rtlpriv, CMDR, 0x07FC);

	/* MH We must enable the section of code to prevent load IMEM fail. */
	/* Load MAC register from WMAc temporarily We simulate macreg. */
	/* txt HW will provide MAC txt later  */
	rtl_write_byte(rtlpriv, 0x6, 0x30);
	rtl_write_byte(rtlpriv, 0x49, 0xf0);

	rtl_write_byte(rtlpriv, 0x4b, 0x81);

	rtl_write_byte(rtlpriv, 0xb5, 0x21);

	rtl_write_byte(rtlpriv, 0xdc, 0xff);
	rtl_write_byte(rtlpriv, 0xdd, 0xff);
	rtl_write_byte(rtlpriv, 0xde, 0xff);
	rtl_write_byte(rtlpriv, 0xdf, 0xff);

	rtl_write_byte(rtlpriv, 0x11a, 0x00);
	rtl_write_byte(rtlpriv, 0x11b, 0x00);

	for (i = 0; i < 32; i++)
		rtl_write_byte(rtlpriv, INIMCS_SEL + i, 0x1b);

	rtl_write_byte(rtlpriv, 0x236, 0xff);

	rtl_write_byte(rtlpriv, 0x503, 0x22);

	if (ppsc->support_aspm && !ppsc->support_backdoor)
		rtl_write_byte(rtlpriv, 0x560, 0x40);
	else
		rtl_write_byte(rtlpriv, 0x560, 0x00);

	rtl_write_byte(rtlpriv, DBG_PORT, 0x91);

	/* Set RX Desc Address */
	rtl_write_dword(rtlpriv, RDQDA, rtlpci->rx_ring[RX_MPDU_QUEUE].dma);
	rtl_write_dword(rtlpriv, RCDA, rtlpci->rx_ring[RX_CMD_QUEUE].dma);

	/* Set TX Desc Address */
	rtl_write_dword(rtlpriv, TBKDA, rtlpci->tx_ring[BK_QUEUE].dma);
	rtl_write_dword(rtlpriv, TBEDA, rtlpci->tx_ring[BE_QUEUE].dma);
	rtl_write_dword(rtlpriv, TVIDA, rtlpci->tx_ring[VI_QUEUE].dma);
	rtl_write_dword(rtlpriv, TVODA, rtlpci->tx_ring[VO_QUEUE].dma);
	rtl_write_dword(rtlpriv, TBDA, rtlpci->tx_ring[BEACON_QUEUE].dma);
	rtl_write_dword(rtlpriv, TCDA, rtlpci->tx_ring[TXCMD_QUEUE].dma);
	rtl_write_dword(rtlpriv, TMDA, rtlpci->tx_ring[MGNT_QUEUE].dma);
	rtl_write_dword(rtlpriv, THPDA, rtlpci->tx_ring[HIGH_QUEUE].dma);
	rtl_write_dword(rtlpriv, HDA, rtlpci->tx_ring[HCCA_QUEUE].dma);

	rtl_write_word(rtlpriv, CMDR, 0x37FC);

	/* To make sure that TxDMA can ready to download FW. */
	/* We should reset TxDMA if IMEM RPT was not ready. */
	do {
		tmpu1b = rtl_read_byte(rtlpriv, TCR);
		if ((tmpu1b & TXDMA_INIT_VALUE) == TXDMA_INIT_VALUE)
			break;

		udelay(5);
	} while (pollingcnt--);

	if (pollingcnt <= 0) {
		RT_TRACE(rtlpriv, COMP_ERR, DBG_EMERG,
			 "Polling TXDMA_INIT_VALUE timeout!! Current TCR(%#x)\n",
			 tmpu1b);
		tmpu1b = rtl_read_byte(rtlpriv, CMDR);
		rtl_write_byte(rtlpriv, CMDR, tmpu1b & (~TXDMA_EN));
		udelay(2);
		/* Reset TxDMA */
		rtl_write_byte(rtlpriv, CMDR, tmpu1b | TXDMA_EN);
	}

	/* After MACIO reset,we must refresh LED state. */
	if ((ppsc->rfoff_reason == RF_CHANGE_BY_IPS) ||
	   (ppsc->rfoff_reason == 0)) {
		struct rtl_pci_priv *pcipriv = rtl_pcipriv(hw);
		struct rtl_led *pLed0 = &(pcipriv->ledctl.sw_led0);
		enum rf_pwrstate rfpwr_state_toset;
		rfpwr_state_toset = rtl92s_rf_onoff_detect(hw);

		if (rfpwr_state_toset == ERFON)
			rtl92s_sw_led_on(hw, pLed0);
	}
}

static void _rtl92se_macconfig_after_fwdownload(struct ieee80211_hw *hw)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	struct rtl_hal *rtlhal = rtl_hal(rtl_priv(hw));
	struct rtl_efuse *rtlefuse = rtl_efuse(rtl_priv(hw));
	struct rtl_pci *rtlpci = rtl_pcidev(rtl_pcipriv(hw));
	u8 i;
	u16 tmpu2b;

	/* 1. System Configure Register (Offset: 0x0000 - 0x003F) */

	/* 2. Command Control Register (Offset: 0x0040 - 0x004F) */
	/* Turn on 0x40 Command register */
	rtl_write_word(rtlpriv, CMDR, (BBRSTN | BB_GLB_RSTN |
			SCHEDULE_EN | MACRXEN | MACTXEN | DDMA_EN | FW2HW_EN |
			RXDMA_EN | TXDMA_EN | HCI_RXDMA_EN | HCI_TXDMA_EN));

	/* Set TCR TX DMA pre 2 FULL enable bit	*/
	rtl_write_dword(rtlpriv, TCR, rtl_read_dword(rtlpriv, TCR) |
			TXDMAPRE2FULL);

	/* Set RCR	*/
	rtl_write_dword(rtlpriv, RCR, rtlpci->receive_config);

	/* 3. MACID Setting Register (Offset: 0x0050 - 0x007F) */

	/* 4. Timing Control Register  (Offset: 0x0080 - 0x009F) */
	/* Set CCK/OFDM SIFS */
	/* CCK SIFS shall always be 10us. */
	rtl_write_word(rtlpriv, SIFS_CCK, 0x0a0a);
	rtl_write_word(rtlpriv, SIFS_OFDM, 0x1010);

	/* Set AckTimeout */
	rtl_write_byte(rtlpriv, ACK_TIMEOUT, 0x40);

	/* Beacon related */
	rtl_write_word(rtlpriv, BCN_INTERVAL, 100);
	rtl_write_word(rtlpriv, ATIMWND, 2);

	/* 5. FIFO Control Register (Offset: 0x00A0 - 0x015F) */
	/* 5.1 Initialize Number of Reserved Pages in Firmware Queue */
	/* Firmware allocate now, associate with FW internal setting.!!! */

	/* 5.2 Setting TX/RX page size 0/1/2/3/4=64/128/256/512/1024 */
	/* 5.3 Set driver info, we only accept PHY status now. */
	/* 5.4 Set RXDMA arbitration to control RXDMA/MAC/FW R/W for RXFIFO  */
	rtl_write_byte(rtlpriv, RXDMA, rtl_read_byte(rtlpriv, RXDMA) | BIT(6));

	/* 6. Adaptive Control Register  (Offset: 0x0160 - 0x01CF) */
	/* Set RRSR to all legacy rate and HT rate
	 * CCK rate is supported by default.
	 * CCK rate will be filtered out only when associated
	 * AP does not support it.
	 * Only enable ACK rate to OFDM 24M
	 * Disable RRSR for CCK rate in A-Cut	*/

	if (rtlhal->version == VERSION_8192S_ACUT)
		rtl_write_byte(rtlpriv, RRSR, 0xf0);
	else if (rtlhal->version == VERSION_8192S_BCUT)
		rtl_write_byte(rtlpriv, RRSR, 0xff);
	rtl_write_byte(rtlpriv, RRSR + 1, 0x01);
	rtl_write_byte(rtlpriv, RRSR + 2, 0x00);

	/* A-Cut IC do not support CCK rate. We forbid ARFR to */
	/* fallback to CCK rate */
	for (i = 0; i < 8; i++) {
		/*Disable RRSR for CCK rate in A-Cut */
		if (rtlhal->version == VERSION_8192S_ACUT)
			rtl_write_dword(rtlpriv, ARFR0 + i * 4, 0x1f0ff0f0);
	}

	/* Different rate use different AMPDU size */
	/* MCS32/ MCS15_SG use max AMPDU size 15*2=30K */
	rtl_write_byte(rtlpriv, AGGLEN_LMT_H, 0x0f);
	/* MCS0/1/2/3 use max AMPDU size 4*2=8K */
	rtl_write_word(rtlpriv, AGGLEN_LMT_L, 0x7442);
	/* MCS4/5 use max AMPDU size 8*2=16K 6/7 use 10*2=20K */
	rtl_write_word(rtlpriv, AGGLEN_LMT_L + 2, 0xddd7);
	/* MCS8/9 use max AMPDU size 8*2=16K 10/11 use 10*2=20K */
	rtl_write_word(rtlpriv, AGGLEN_LMT_L + 4, 0xd772);
	/* MCS12/13/14/15 use max AMPDU size 15*2=30K */
	rtl_write_word(rtlpriv, AGGLEN_LMT_L + 6, 0xfffd);

	/* Set Data / Response auto rate fallack retry count */
	rtl_write_dword(rtlpriv, DARFRC, 0x04010000);
	rtl_write_dword(rtlpriv, DARFRC + 4, 0x09070605);
	rtl_write_dword(rtlpriv, RARFRC, 0x04010000);
	rtl_write_dword(rtlpriv, RARFRC + 4, 0x09070605);

	/* 7. EDCA Setting Register (Offset: 0x01D0 - 0x01FF) */
	/* Set all rate to support SG */
	rtl_write_word(rtlpriv, SG_RATE, 0xFFFF);

	/* 8. WMAC, BA, and CCX related Register (Offset: 0x0200 - 0x023F) */
	/* Set NAV protection length */
	rtl_write_word(rtlpriv, NAV_PROT_LEN, 0x0080);
	/* CF-END Threshold */
	rtl_write_byte(rtlpriv, CFEND_TH, 0xFF);
	/* Set AMPDU minimum space */
	rtl_write_byte(rtlpriv, AMPDU_MIN_SPACE, 0x07);
	/* Set TXOP stall control for several queue/HI/BCN/MGT/ */
	rtl_write_byte(rtlpriv, TXOP_STALL_CTRL, 0x00);

	/* 9. Security Control Register (Offset: 0x0240 - 0x025F) */
	/* 10. Power Save Control Register (Offset: 0x0260 - 0x02DF) */
	/* 11. General Purpose Register (Offset: 0x02E0 - 0x02FF) */
	/* 12. Host Interrupt Status Register (Offset: 0x0300 - 0x030F) */
	/* 13. Test Mode and Debug Control Register (Offset: 0x0310 - 0x034F) */

	/* 14. Set driver info, we only accept PHY status now. */
	rtl_write_byte(rtlpriv, RXDRVINFO_SZ, 4);

	/* 15. For EEPROM R/W Workaround */
	/* 16. For EFUSE to share REG_SYS_FUNC_EN with EEPROM!!! */
	tmpu2b = rtl_read_byte(rtlpriv, REG_SYS_FUNC_EN);
	rtl_write_byte(rtlpriv, REG_SYS_FUNC_EN, tmpu2b | BIT(13));
	tmpu2b = rtl_read_byte(rtlpriv, REG_SYS_ISO_CTRL);
	rtl_write_byte(rtlpriv, REG_SYS_ISO_CTRL, tmpu2b & (~BIT(8)));

	/* 17. For EFUSE */
	/* We may R/W EFUSE in EEPROM mode */
	if (rtlefuse->epromtype == EEPROM_BOOT_EFUSE) {
		u8	tempval;

		tempval = rtl_read_byte(rtlpriv, REG_SYS_ISO_CTRL + 1);
		tempval &= 0xFE;
		rtl_write_byte(rtlpriv, REG_SYS_ISO_CTRL + 1, tempval);

		/* Change Program timing */
		rtl_write_byte(rtlpriv, REG_EFUSE_CTRL + 3, 0x72);
		RT_TRACE(rtlpriv, COMP_INIT, DBG_DMESG, "EFUSE CONFIG OK\n");
	}

	RT_TRACE(rtlpriv, COMP_INIT, DBG_DMESG, "OK\n");

}

static void _rtl92se_hw_configure(struct ieee80211_hw *hw)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	struct rtl_pci *rtlpci = rtl_pcidev(rtl_pcipriv(hw));
	struct rtl_phy *rtlphy = &(rtlpriv->phy);
	struct rtl_hal *rtlhal = rtl_hal(rtl_priv(hw));

	u8 reg_bw_opmode = 0;
	u32 reg_rrsr = 0;
	u8 regtmp = 0;

	reg_bw_opmode = BW_OPMODE_20MHZ;
	reg_rrsr = RATE_ALL_CCK | RATE_ALL_OFDM_AG;

	regtmp = rtl_read_byte(rtlpriv, INIRTSMCS_SEL);
	reg_rrsr = ((reg_rrsr & 0x000fffff) << 8) | regtmp;
	rtl_write_dword(rtlpriv, INIRTSMCS_SEL, reg_rrsr);
	rtl_write_byte(rtlpriv, BW_OPMODE, reg_bw_opmode);

	/* Set Retry Limit here */
	rtlpriv->cfg->ops->set_hw_reg(hw, HW_VAR_RETRY_LIMIT,
			(u8 *)(&rtlpci->shortretry_limit));

	rtl_write_byte(rtlpriv, MLT, 0x8f);

	/* For Min Spacing configuration. */
	switch (rtlphy->rf_type) {
	case RF_1T2R:
	case RF_1T1R:
		rtlhal->minspace_cfg = (MAX_MSS_DENSITY_1T << 3);
		break;
	case RF_2T2R:
	case RF_2T2R_GREEN:
		rtlhal->minspace_cfg = (MAX_MSS_DENSITY_2T << 3);
		break;
	}
	rtl_write_byte(rtlpriv, AMPDU_MIN_SPACE, rtlhal->minspace_cfg);
}

int rtl92se_hw_init(struct ieee80211_hw *hw)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	struct rtl_hal *rtlhal = rtl_hal(rtl_priv(hw));
	struct rtl_phy *rtlphy = &(rtlpriv->phy);
	struct rtl_pci *rtlpci = rtl_pcidev(rtl_pcipriv(hw));
	struct rtl_efuse *rtlefuse = rtl_efuse(rtl_priv(hw));
	u8 tmp_byte = 0;
	unsigned long flags;
	bool rtstatus = true;
	u8 tmp_u1b;
	int err = false;
	u8 i;
	int wdcapra_add[] = {
		EDCAPARA_BE, EDCAPARA_BK,
		EDCAPARA_VI, EDCAPARA_VO};
	u8 secr_value = 0x0;

	rtlpci->being_init_adapter = true;

	/* As this function can take a very long time (up to 350 ms)
	 * and can be called with irqs disabled, reenable the irqs
	 * to let the other devices continue being serviced.
	 *
	 * It is safe doing so since our own interrupts will only be enabled
	 * in a subsequent step.
	 */
	local_save_flags(flags);
	local_irq_enable();

	rtlpriv->intf_ops->disable_aspm(hw);

	/* 1. MAC Initialize */
	/* Before FW download, we have to set some MAC register */
	_rtl92se_macconfig_before_fwdownload(hw);

	rtlhal->version = (enum version_8192s)((rtl_read_dword(rtlpriv,
			PMC_FSM) >> 16) & 0xF);

	rtl92s_gpiobit3_cfg_inputmode(hw);

	/* 2. download firmware */
	rtstatus = rtl92s_download_fw(hw);
	if (!rtstatus) {
		RT_TRACE(rtlpriv, COMP_ERR, DBG_WARNING,
			 "Failed to download FW. Init HW without FW now... "
			 "Please copy FW into /lib/firmware/rtlwifi\n");
		err = 1;
		goto exit;
	}

	/* After FW download, we have to reset MAC register */
	_rtl92se_macconfig_after_fwdownload(hw);

	/*Retrieve default FW Cmd IO map. */
	rtlhal->fwcmd_iomap =	rtl_read_word(rtlpriv, LBUS_MON_ADDR);
	rtlhal->fwcmd_ioparam = rtl_read_dword(rtlpriv, LBUS_ADDR_MASK);

	/* 3. Initialize MAC/PHY Config by MACPHY_reg.txt */
	if (!rtl92s_phy_mac_config(hw)) {
		RT_TRACE(rtlpriv, COMP_ERR, DBG_EMERG, "MAC Config failed\n");
		err = rtstatus;
		goto exit;
	}

	/* because last function modify RCR, so we update
	 * rcr var here, or TP will unstable for receive_config
	 * is wrong, RX RCR_ACRC32 will cause TP unstabel & Rx
	 * RCR_APP_ICV will cause mac80211 unassoc for cisco 1252
	 */
	rtlpci->receive_config = rtl_read_dword(rtlpriv, RCR);
	rtlpci->receive_config &= ~(RCR_ACRC32 | RCR_AICV);
	rtl_write_dword(rtlpriv, RCR, rtlpci->receive_config);

	/* Make sure BB/RF write OK. We should prevent enter IPS. radio off. */
	/* We must set flag avoid BB/RF config period later!! */
	rtl_write_dword(rtlpriv, CMDR, 0x37FC);

	/* 4. Initialize BB After MAC Config PHY_reg.txt, AGC_Tab.txt */
	if (!rtl92s_phy_bb_config(hw)) {
		RT_TRACE(rtlpriv, COMP_INIT, DBG_EMERG, "BB Config failed\n");
		err = rtstatus;
		goto exit;
	}

	/* 5. Initiailze RF RAIO_A.txt RF RAIO_B.txt */
	/* Before initalizing RF. We can not use FW to do RF-R/W. */

	rtlphy->rf_mode = RF_OP_BY_SW_3WIRE;

	/* Before RF-R/W we must execute the IO from Scott's suggestion. */
	rtl_write_byte(rtlpriv, AFE_XTAL_CTRL + 1, 0xDB);
	if (rtlhal->version == VERSION_8192S_ACUT)
		rtl_write_byte(rtlpriv, SPS1_CTRL + 3, 0x07);
	else
		rtl_write_byte(rtlpriv, RF_CTRL, 0x07);

	if (!rtl92s_phy_rf_config(hw)) {
		RT_TRACE(rtlpriv, COMP_INIT, DBG_DMESG, "RF Config failed\n");
		err = rtstatus;
		goto exit;
	}

	/* After read predefined TXT, we must set BB/MAC/RF
	 * register as our requirement */

	rtlphy->rfreg_chnlval[0] = rtl92s_phy_query_rf_reg(hw,
							   (enum radio_path)0,
							   RF_CHNLBW,
							   RFREG_OFFSET_MASK);
	rtlphy->rfreg_chnlval[1] = rtl92s_phy_query_rf_reg(hw,
							   (enum radio_path)1,
							   RF_CHNLBW,
							   RFREG_OFFSET_MASK);

	/*---- Set CCK and OFDM Block "ON"----*/
	rtl_set_bbreg(hw, RFPGA0_RFMOD, BCCKEN, 0x1);
	rtl_set_bbreg(hw, RFPGA0_RFMOD, BOFDMEN, 0x1);

	/*3 Set Hardware(Do nothing now) */
	_rtl92se_hw_configure(hw);

	/* Read EEPROM TX power index and PHY_REG_PG.txt to capture correct */
	/* TX power index for different rate set. */
	/* Get original hw reg values */
	rtl92s_phy_get_hw_reg_originalvalue(hw);
	/* Write correct tx power index */
	rtl92s_phy_set_txpower(hw, rtlphy->current_channel);

	/* We must set MAC address after firmware download. */
	for (i = 0; i < 6; i++)
		rtl_write_byte(rtlpriv, MACIDR0 + i, rtlefuse->dev_addr[i]);

	/* EEPROM R/W workaround */
	tmp_u1b = rtl_read_byte(rtlpriv, MAC_PINMUX_CFG);
	rtl_write_byte(rtlpriv, MAC_PINMUX_CFG, tmp_u1b & (~BIT(3)));

	rtl_write_byte(rtlpriv, 0x4d, 0x0);

	if (hal_get_firmwareversion(rtlpriv) >= 0x49) {
		tmp_byte = rtl_read_byte(rtlpriv, FW_RSVD_PG_CRTL) & (~BIT(4));
		tmp_byte = tmp_byte | BIT(5);
		rtl_write_byte(rtlpriv, FW_RSVD_PG_CRTL, tmp_byte);
		rtl_write_dword(rtlpriv, TXDESC_MSK, 0xFFFFCFFF);
	}

	/* We enable high power and RA related mechanism after NIC
	 * initialized. */
	if (hal_get_firmwareversion(rtlpriv) >= 0x35) {
		/* Fw v.53 and later. */
		rtl92s_phy_set_fw_cmd(hw, FW_CMD_RA_INIT);
	} else if (hal_get_firmwareversion(rtlpriv) == 0x34) {
		/* Fw v.52. */
		rtl_write_dword(rtlpriv, WFM5, FW_RA_INIT);
		rtl92s_phy_chk_fwcmd_iodone(hw);
	} else {
		/* Compatible earlier FW version. */
		rtl_write_dword(rtlpriv, WFM5, FW_RA_RESET);
		rtl92s_phy_chk_fwcmd_iodone(hw);
		rtl_write_dword(rtlpriv, WFM5, FW_RA_ACTIVE);
		rtl92s_phy_chk_fwcmd_iodone(hw);
		rtl_write_dword(rtlpriv, WFM5, FW_RA_REFRESH);
		rtl92s_phy_chk_fwcmd_iodone(hw);
	}

	/* Add to prevent ASPM bug. */
	/* Always enable hst and NIC clock request. */
	rtl92s_phy_switch_ephy_parameter(hw);

	/* Security related
	 * 1. Clear all H/W keys.
	 * 2. Enable H/W encryption/decryption. */
	rtl_cam_reset_all_entry(hw);
	secr_value |= SCR_TXENCENABLE;
	secr_value |= SCR_RXENCENABLE;
	secr_value |= SCR_NOSKMC;
	rtl_write_byte(rtlpriv, REG_SECR, secr_value);

	for (i = 0; i < 4; i++)
		rtl_write_dword(rtlpriv, wdcapra_add[i], 0x5e4322);

	if (rtlphy->rf_type == RF_1T2R) {
		bool mrc2set = true;
		/* Turn on B-Path */
		rtlpriv->cfg->ops->set_hw_reg(hw, HW_VAR_MRC, (u8 *)&mrc2set);
	}

	rtlpriv->cfg->ops->led_control(hw, LED_CTL_POWER_ON);
	rtl92s_dm_init(hw);
exit:
	local_irq_restore(flags);
	rtlpci->being_init_adapter = false;
	return err;
}

void rtl92se_set_mac_addr(struct rtl_io *io, const u8 *addr)
{
	/* This is a stub. */
}

void rtl92se_set_check_bssid(struct ieee80211_hw *hw, bool check_bssid)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	u32 reg_rcr;

	if (rtlpriv->psc.rfpwr_state != ERFON)
		return;

	rtlpriv->cfg->ops->get_hw_reg(hw, HW_VAR_RCR, (u8 *)(&reg_rcr));

	if (check_bssid) {
		reg_rcr |= (RCR_CBSSID);
		rtlpriv->cfg->ops->set_hw_reg(hw, HW_VAR_RCR, (u8 *)(&reg_rcr));
	} else if (!check_bssid) {
		reg_rcr &= (~RCR_CBSSID);
		rtlpriv->cfg->ops->set_hw_reg(hw, HW_VAR_RCR, (u8 *)(&reg_rcr));
	}

}

/* HW_VAR_MEDIA_STATUS & HW_VAR_CECHK_BSSID */
int rtl92se_set_network_type(struct ieee80211_hw *hw, enum nl80211_iftype type)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);

	if (rtl92s_set_media_status(hw, type))
		return -EOPNOTSUPP;

	if (rtlpriv->mac80211.link_state == MAC80211_LINKED) {
		if (type != NL80211_IFTYPE_AP)
			rtl92se_set_check_bssid(hw, true);
	} else {
		rtl92se_set_check_bssid(hw, false);
	}

	return 0;
}

void rtl92se_enable_interrupt(struct ieee80211_hw *hw)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	struct rtl_pci *rtlpci = rtl_pcidev(rtl_pcipriv(hw));

	rtl_write_dword(rtlpriv, INTA_MASK, rtlpci->irq_mask[0]);
	/* Support Bit 32-37(Assign as Bit 0-5) interrupt setting now */
	rtl_write_dword(rtlpriv, INTA_MASK + 4, rtlpci->irq_mask[1] & 0x3F);
}

void rtl92se_disable_interrupt(struct ieee80211_hw *hw)
{
	struct rtl_priv *rtlpriv;
	struct rtl_pci *rtlpci;

	rtlpriv = rtl_priv(hw);
	/* if firmware not available, no interrupts */
	if (!rtlpriv || !rtlpriv->max_fw_size)
		return;
	rtlpci = rtl_pcidev(rtl_pcipriv(hw));
	rtl_write_dword(rtlpriv, INTA_MASK, 0);
	rtl_write_dword(rtlpriv, INTA_MASK + 4, 0);

	synchronize_irq(rtlpci->pdev->irq);
}

static void _rtl92se_gen_refreshledstate(struct ieee80211_hw *hw)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	struct rtl_pci *rtlpci = rtl_pcidev(rtl_pcipriv(hw));
	struct rtl_pci_priv *pcipriv = rtl_pcipriv(hw);
	struct rtl_led *pLed0 = &(pcipriv->ledctl.sw_led0);

	if (rtlpci->up_first_time == 1)
		return;

	if (rtlpriv->psc.rfoff_reason == RF_CHANGE_BY_IPS)
		rtl92s_sw_led_on(hw, pLed0);
	else
		rtl92s_sw_led_off(hw, pLed0);
}


static void _rtl92se_power_domain_init(struct ieee80211_hw *hw)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	u16 tmpu2b;
	u8 tmpu1b;

	rtlpriv->psc.pwrdomain_protect = true;

	tmpu2b = rtl_read_word(rtlpriv, REG_SYS_CLKR);
	if (tmpu2b & SYS_FWHW_SEL) {
		tmpu2b &= ~(SYS_SWHW_SEL | SYS_FWHW_SEL);
		if (!rtl92s_halset_sysclk(hw, tmpu2b)) {
			rtlpriv->psc.pwrdomain_protect = false;
			return;
		}
	}

	rtl_write_byte(rtlpriv, AFE_PLL_CTRL, 0x0);
	rtl_write_byte(rtlpriv, LDOA15_CTRL, 0x34);

	/* Reset MAC-IO and CPU and Core Digital BIT10/11/15 */
	tmpu1b = rtl_read_byte(rtlpriv, REG_SYS_FUNC_EN + 1);

	/* If IPS we need to turn LED on. So we not
	 * not disable BIT 3/7 of reg3. */
	if (rtlpriv->psc.rfoff_reason & (RF_CHANGE_BY_IPS | RF_CHANGE_BY_HW))
		tmpu1b &= 0xFB;
	else
		tmpu1b &= 0x73;

	rtl_write_byte(rtlpriv, REG_SYS_FUNC_EN + 1, tmpu1b);
	/* wait for BIT 10/11/15 to pull high automatically!! */
	mdelay(1);

	rtl_write_byte(rtlpriv, CMDR, 0);
	rtl_write_byte(rtlpriv, TCR, 0);

	/* Data sheet not define 0x562!!! Copy from WMAC!!!!! */
	tmpu1b = rtl_read_byte(rtlpriv, 0x562);
	tmpu1b |= 0x08;
	rtl_write_byte(rtlpriv, 0x562, tmpu1b);
	tmpu1b &= ~(BIT(3));
	rtl_write_byte(rtlpriv, 0x562, tmpu1b);

	/* Enable AFE clock source */
	tmpu1b = rtl_read_byte(rtlpriv, AFE_XTAL_CTRL);
	rtl_write_byte(rtlpriv, AFE_XTAL_CTRL, (tmpu1b | 0x01));
	/* Delay 1.5ms */
	udelay(1500);
	tmpu1b = rtl_read_byte(rtlpriv, AFE_XTAL_CTRL + 1);
	rtl_write_byte(rtlpriv, AFE_XTAL_CTRL + 1, (tmpu1b & 0xfb));

	/* Enable AFE Macro Block's Bandgap */
	tmpu1b = rtl_read_byte(rtlpriv, AFE_MISC);
	rtl_write_byte(rtlpriv, AFE_MISC, (tmpu1b | BIT(0)));
	mdelay(1);

	/* Enable AFE Mbias */
	tmpu1b = rtl_read_byte(rtlpriv, AFE_MISC);
	rtl_write_byte(rtlpriv, AFE_MISC, (tmpu1b | 0x02));
	mdelay(1);

	/* Enable LDOA15 block */
	tmpu1b = rtl_read_byte(rtlpriv, LDOA15_CTRL);
	rtl_write_byte(rtlpriv, LDOA15_CTRL, (tmpu1b | BIT(0)));

	/* Set Digital Vdd to Retention isolation Path. */
	tmpu2b = rtl_read_word(rtlpriv, REG_SYS_ISO_CTRL);
	rtl_write_word(rtlpriv, REG_SYS_ISO_CTRL, (tmpu2b | BIT(11)));


	/* For warm reboot NIC disappera bug. */
	tmpu2b = rtl_read_word(rtlpriv, REG_SYS_FUNC_EN);
	rtl_write_word(rtlpriv, REG_SYS_FUNC_EN, (tmpu2b | BIT(13)));

	rtl_write_byte(rtlpriv, REG_SYS_ISO_CTRL + 1, 0x68);

	/* Enable AFE PLL Macro Block */
	tmpu1b = rtl_read_byte(rtlpriv, AFE_PLL_CTRL);
	rtl_write_byte(rtlpriv, AFE_PLL_CTRL, (tmpu1b | BIT(0) | BIT(4)));
	/* Enable MAC 80MHZ clock */
	tmpu1b = rtl_read_byte(rtlpriv, AFE_PLL_CTRL + 1);
	rtl_write_byte(rtlpriv, AFE_PLL_CTRL + 1, (tmpu1b | BIT(0)));
	mdelay(1);

	/* Release isolation AFE PLL & MD */
	rtl_write_byte(rtlpriv, REG_SYS_ISO_CTRL, 0xA6);

	/* Enable MAC clock */
	tmpu2b = rtl_read_word(rtlpriv, SYS_CLKR);
	rtl_write_word(rtlpriv, SYS_CLKR, (tmpu2b | BIT(12) | BIT(11)));

	/* Enable Core digital and enable IOREG R/W */
	tmpu2b = rtl_read_word(rtlpriv, REG_SYS_FUNC_EN);
	rtl_write_word(rtlpriv, REG_SYS_FUNC_EN, (tmpu2b | BIT(11)));
	/* enable REG_EN */
	rtl_write_word(rtlpriv, REG_SYS_FUNC_EN, (tmpu2b | BIT(11) | BIT(15)));

	/* Switch the control path. */
	tmpu2b = rtl_read_word(rtlpriv, SYS_CLKR);
	rtl_write_word(rtlpriv, SYS_CLKR, (tmpu2b & (~BIT(2))));

	tmpu2b = rtl_read_word(rtlpriv, REG_SYS_CLKR);
	tmpu2b |= SYS_FWHW_SEL;
	tmpu2b &= ~SYS_SWHW_SEL;
	if (!rtl92s_halset_sysclk(hw, tmpu2b)) {
		rtlpriv->psc.pwrdomain_protect = false;
		return;
	}

	rtl_write_word(rtlpriv, CMDR, 0x37FC);

	/* After MACIO reset,we must refresh LED state. */
	_rtl92se_gen_refreshledstate(hw);

	rtlpriv->psc.pwrdomain_protect = false;
}

void rtl92se_card_disable(struct ieee80211_hw *hw)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	struct rtl_mac *mac = rtl_mac(rtl_priv(hw));
	struct rtl_pci *rtlpci = rtl_pcidev(rtl_pcipriv(hw));
	struct rtl_ps_ctl *ppsc = rtl_psc(rtl_priv(hw));
	enum nl80211_iftype opmode;
	u8 wait = 30;

	rtlpriv->intf_ops->enable_aspm(hw);

	if (rtlpci->driver_is_goingto_unload ||
		ppsc->rfoff_reason > RF_CHANGE_BY_PS)
		rtlpriv->cfg->ops->led_control(hw, LED_CTL_POWER_OFF);

	/* we should chnge GPIO to input mode
	 * this will drop away current about 25mA*/
	rtl92s_gpiobit3_cfg_inputmode(hw);

	/* this is very important for ips power save */
	while (wait-- >= 10 && rtlpriv->psc.pwrdomain_protect) {
		if (rtlpriv->psc.pwrdomain_protect)
			mdelay(20);
		else
			break;
	}

	mac->link_state = MAC80211_NOLINK;
	opmode = NL80211_IFTYPE_UNSPECIFIED;
	rtl92s_set_media_status(hw, opmode);

	rtl92s_phy_set_rfhalt(hw);
	udelay(100);
}

void rtl92se_interrupt_recognized(struct ieee80211_hw *hw, u32 *p_inta,
			     u32 *p_intb)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	struct rtl_pci *rtlpci = rtl_pcidev(rtl_pcipriv(hw));

	*p_inta = rtl_read_dword(rtlpriv, ISR) & rtlpci->irq_mask[0];
	rtl_write_dword(rtlpriv, ISR, *p_inta);

	*p_intb = rtl_read_dword(rtlpriv, ISR + 4) & rtlpci->irq_mask[1];
	rtl_write_dword(rtlpriv, ISR + 4, *p_intb);
}

void rtl92se_update_interrupt_mask(struct ieee80211_hw *hw,
		u32 add_msr, u32 rm_msr)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	struct rtl_pci *rtlpci = rtl_pcidev(rtl_pcipriv(hw));

	RT_TRACE(rtlpriv, COMP_INTR, DBG_LOUD, "add_msr:%x, rm_msr:%x\n",
		 add_msr, rm_msr);

	if (add_msr)
		rtlpci->irq_mask[0] |= add_msr;

	if (rm_msr)
		rtlpci->irq_mask[0] &= (~rm_msr);

	rtl92se_disable_interrupt(hw);
	rtl92se_enable_interrupt(hw);
}

/* this ifunction is for RFKILL, it's different with windows,
 * because UI will disable wireless when GPIO Radio Off.
 * And here we not check or Disable/Enable ASPM like windows*/
bool rtl92se_gpio_radio_on_off_checking(struct ieee80211_hw *hw, u8 *valid)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	struct rtl_ps_ctl *ppsc = rtl_psc(rtl_priv(hw));
	struct rtl_pci *rtlpci = rtl_pcidev(rtl_pcipriv(hw));
	enum rf_pwrstate rfpwr_toset /*, cur_rfstate */;
	unsigned long flag = 0;
	bool actuallyset = false;
	bool turnonbypowerdomain = false;

	/* just 8191se can check gpio before firstup, 92c/92d have fixed it */
	if ((rtlpci->up_first_time == 1) || (rtlpci->being_init_adapter))
		return false;

	if (ppsc->swrf_processing)
		return false;

	spin_lock_irqsave(&rtlpriv->locks.rf_ps_lock, flag);
	if (ppsc->rfchange_inprogress) {
		spin_unlock_irqrestore(&rtlpriv->locks.rf_ps_lock, flag);
		return false;
	} else {
		ppsc->rfchange_inprogress = true;
		spin_unlock_irqrestore(&rtlpriv->locks.rf_ps_lock, flag);
	}

	/* cur_rfstate = ppsc->rfpwr_state;*/

	/* because after _rtl92s_phy_set_rfhalt, all power
	 * closed, so we must open some power for GPIO check,
	 * or we will always check GPIO RFOFF here,
	 * And we should close power after GPIO check */
	if (RT_IN_PS_LEVEL(ppsc, RT_RF_OFF_LEVL_HALT_NIC)) {
		_rtl92se_power_domain_init(hw);
		turnonbypowerdomain = true;
	}

	rfpwr_toset = rtl92s_rf_onoff_detect(hw);

	if ((ppsc->hwradiooff) && (rfpwr_toset == ERFON)) {
		RT_TRACE(rtlpriv, COMP_RF, DBG_DMESG,
			 "RFKILL-HW Radio ON, RF ON\n");

		rfpwr_toset = ERFON;
		ppsc->hwradiooff = false;
		actuallyset = true;
	} else if ((!ppsc->hwradiooff) && (rfpwr_toset == ERFOFF)) {
		RT_TRACE(rtlpriv, COMP_RF,
			 DBG_DMESG, "RFKILL-HW Radio OFF, RF OFF\n");

		rfpwr_toset = ERFOFF;
		ppsc->hwradiooff = true;
		actuallyset = true;
	}

	if (actuallyset) {
		spin_lock_irqsave(&rtlpriv->locks.rf_ps_lock, flag);
		ppsc->rfchange_inprogress = false;
		spin_unlock_irqrestore(&rtlpriv->locks.rf_ps_lock, flag);

	/* this not include ifconfig wlan0 down case */
	/* } else if (rfpwr_toset == ERFOFF || cur_rfstate == ERFOFF) { */
	} else {
		/* because power_domain_init may be happen when
		 * _rtl92s_phy_set_rfhalt, this will open some powers
		 * and cause current increasing about 40 mA for ips,
		 * rfoff and ifconfig down, so we set
		 * _rtl92s_phy_set_rfhalt again here */
		if (ppsc->reg_rfps_level & RT_RF_OFF_LEVL_HALT_NIC &&
			turnonbypowerdomain) {
			rtl92s_phy_set_rfhalt(hw);
			RT_SET_PS_LEVEL(ppsc, RT_RF_OFF_LEVL_HALT_NIC);
		}

		spin_lock_irqsave(&rtlpriv->locks.rf_ps_lock, flag);
		ppsc->rfchange_inprogress = false;
		spin_unlock_irqrestore(&rtlpriv->locks.rf_ps_lock, flag);
	}

	*valid = 1;
	return !ppsc->hwradiooff;

}

void rtl92se_suspend(struct ieee80211_hw *hw)
{
	struct rtl_pci *rtlpci = rtl_pcidev(rtl_pcipriv(hw));

	rtlpci->up_first_time = true;
}

void rtl92se_resume(struct ieee80211_hw *hw)
{
	struct rtl_pci *rtlpci = rtl_pcidev(rtl_pcipriv(hw));
	u32 val;

	pci_read_config_dword(rtlpci->pdev, 0x40, &val);
	if ((val & 0x0000ff00) != 0)
		pci_write_config_dword(rtlpci->pdev, 0x40,
			val & 0xffff00ff);
}
