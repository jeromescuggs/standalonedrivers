/*
 *      Header files defines some RX inline routines
 *
 *      $Id: 8190n_rx.h,v 1.7 2009/08/06 11:41:29 yachang Exp $
 */

#ifndef _8190N_RX_H_
#define _8190N_RX_H_

#include "./8190n_cfg.h"
#include "./8190n.h"
#include "./8190n_util.h"

#ifdef	CONFIG_RTL865X_EXTPORT
extern struct net_device *cachedWlanDev;
#endif

#if defined(CONFIG_RTL_WAPI_SUPPORT)
extern unsigned int	extra_offset;
#endif

#define SN_NEXT(n)		((n + 1) & 0xfff)
#define SN_LESS(a, b)	(((a - b) & 0x800) != 0)
#define SN_DIFF(a, b)	((a >= b)? (a - b):(0xfff - b + a + 1))


#define init_frinfo(pinfo) \
	do	{	\
			pinfo->pskb = pskb;		\
			pinfo->rssi = 0;		\
			INIT_LIST_HEAD(&(pinfo->mpdu_list)); \
			INIT_LIST_HEAD(&(pinfo->rx_list)); \
	} while(0)

void rtl8192su_irq_rx_tasklet_new(unsigned int task_priv);
void rtl8192su_rx_urbsubmit(struct net_device *dev, struct urb* rx_urb,int idx);
void process_all_queues(struct rtl8190_priv *priv);

static __inline__ void init_rxdesc(struct sk_buff *pskb, int i, struct rtl8190_priv *priv)
{
	struct rtl8190_hw	*phw;
	struct rx_frinfo	*pfrinfo;

	phw = GET_HW(priv);

	pfrinfo = (struct rx_frinfo *)((pskb->end)-sizeof(struct rx_frinfo));
	//skb_reserve(pskb, RX_DMA_SHIFT);
	//pskb->data=pskb->head+RX_DMA_SHIFT;

	init_frinfo(pfrinfo);
#if defined(UNIVERSAL_REPEATER) || defined(MBSSID)
	pfrinfo->is_br_mgnt = 0;
#endif

}


#if	defined(RTL8190) || defined(RTL8192)
static __inline__ void init_rxcmddesc(struct sk_buff *pskb, int i, struct rtl8190_priv *priv)
{
	struct rtl8190_hw	*phw;
	struct rx_frinfo	*pfrinfo;
	int offset;

	phw = GET_HW(priv);
#ifdef CONFIG_RTL865XC
	extern int rtlglue_flushDCache(unsigned int start, unsigned int size);
#endif

	pfrinfo = (struct rx_frinfo *)pskb->data;
	offset = (((unsigned long)pskb->data) + sizeof(struct rx_frinfo)) & 0xff;
#ifdef RTL8190_FASTEXTDEV_FUNCALL
	rtl865x_extDev_skb_reserve(pskb, sizeof(struct rx_frinfo)+offset);
#else
	skb_reserve(pskb, sizeof(struct rx_frinfo)+offset);
#endif
	pskb->data -= offset;

	init_frinfo(pfrinfo);

	phw->rxcmd_info[i].pbuf  = (void *)pskb;
	phw->rxcmd_info[i].paddr = get_physical_addr(priv, pskb->data, (RX_BUF_LEN - sizeof(struct rx_frinfo)), PCI_DMA_FROMDEVICE);
	phw->rxcmd_desc[i].paddr = set_desc(phw->rxcmd_info[i].paddr);
	phw->rxcmd_desc[i].cmd   = set_desc((i == (NUM_CMD_DESC - 1)? _EOR_ : 0) | _OWN_ | (RX_BUF_LEN - sizeof(struct rx_frinfo)));
#ifdef CONFIG_RTL865XC
#ifdef RTL8190_RXRING_RANGEBASE_DCACHE_FLUSH
	rtl_cache_sync_wback(priv, phw->rxcmd_desc_dma_addr[i], sizeof(struct rx_desc), PCI_DMA_TODEVICE);
#else
	rtlglue_flushDCache(0, 0);
#endif
#endif
}
#endif


static __inline__ unsigned char cal_rssi_avg(unsigned int agv, unsigned int pkt_rssi)
{
	unsigned int rssi;

	rssi = ((agv * 19) + pkt_rssi) / 20;
	if (pkt_rssi > agv)
		rssi++;

	return (unsigned char)rssi;
}

static __inline__ void update_sta_rssi(struct rtl8190_priv *priv,
				struct stat_info *pstat, struct rx_frinfo *pfrinfo)
{
	int i;

#ifdef MP_TEST
	if (OPMODE & WIFI_MP_STATE) {
		if (priv->pshare->rf_ft_var.rssi_dump) {
			priv->pshare->mp_rssi = cal_rssi_avg(priv->pshare->mp_rssi, pfrinfo->rssi);
			priv->pshare->mp_sq   = pfrinfo->sq;
			for (i=0; i<4; i++)
				priv->pshare->mp_rf_info.mimorssi[i] = cal_rssi_avg(priv->pshare->mp_rf_info.mimorssi[i], pfrinfo->rf_info.mimorssi[i]);
			memcpy(&priv->pshare->mp_rf_info.mimosq[0], &pfrinfo->rf_info.mimosq[0], sizeof(struct rf_misc_info) - 4);
		}
		return;
	}
#endif

	if (pfrinfo->physt)
	{                     // single packet

		if (pfrinfo->rssi) {
			pstat->rssi             = cal_rssi_avg(pstat->rssi, pfrinfo->rssi);
			pstat->sq               = pfrinfo->sq;
			pstat->rx_rate          = pfrinfo->rx_rate;
			pstat->rx_bw            = pfrinfo->rx_bw;
			pstat->rx_splcp         = pfrinfo->rx_splcp;
			if (pfrinfo->rf_info.mimorssi[0] != 0)
				for (i=0; i<2; i++)

#if (defined(RTL8192SE)||defined(RTL8192SU)) && defined(EXT_ANT_DVRY)
			if (priv->pshare->EXT_AD_probe && priv->pshare->rf_ft_var.ExtAntDvry==1) {
				if (pstat->rf_info.mimorssi_hold[i] == 0)
					pstat->rf_info.mimorssi_hold[i] = pstat->rf_info.mimorssi[i];
				pstat->rf_info.mimorssi[i] = cal_rssi_avg(pstat->rf_info.mimorssi[i], pfrinfo->rf_info.mimorssi[i]);			
			}
			else
#endif
			{
					pstat->rf_info.mimorssi[i] = cal_rssi_avg(pstat->rf_info.mimorssi[i], pfrinfo->rf_info.mimorssi[i]);
			}

			if (priv->pshare->rf_ft_var.rssi_dump)
#if (defined(RTL8192SE)||defined(RTL8192SU)) && defined(EXT_ANT_DVRY)
				memcpy(&pstat->rf_info.mimosq[0], &pfrinfo->rf_info.mimosq[0], sizeof(struct rf_misc_info) - 6);
#else
				memcpy(&pstat->rf_info.mimosq[0], &pfrinfo->rf_info.mimosq[0], sizeof(struct rf_misc_info) - 4);
#endif
			if (pstat->highest_rx_rate < pstat->rx_rate)
				pstat->highest_rx_rate = pstat->rx_rate;
		}

	}
}
#endif // _8190N_RX_H_

