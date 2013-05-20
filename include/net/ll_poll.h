/*
 * low latency network device queue flush
 * Copyright(c) 2013 Intel Corporation.
 * Author: Eliezer Tamir
 *
 * For now this depends on CONFIG_I86_TSC
 */

#ifndef _LINUX_NET_LL_POLL_H
#define _LINUX_NET_LL_POLL_H

#ifdef CONFIG_INET_LL_RX_POLL
#include <linux/netdevice.h>
struct napi_struct;
extern int sysctl_net_ll_poll __read_mostly;

/* return values from ndo_ll_poll */
#define LL_FLUSH_DONE		0
#define LL_FLUSH_FAILED		1
#define LL_FLUSH_BUSY		2

/* we don't mind a ~2.5% imprecision */
#define TSC_MHZ (tsc_khz >> 10)

static inline bool sk_valid_ll(struct sock *sk)
{
	return sysctl_net_ll_poll && sk->dev_ref &&
		!need_resched() && !signal_pending(current);
}

static inline bool sk_poll_ll(struct sock *sk, int nonblock)
{
	struct napi_struct *napi = sk->dev_ref;
	const struct net_device_ops *ops;
	unsigned long end_time = TSC_MHZ * ACCESS_ONCE(sysctl_net_ll_poll)
					+ get_cycles();

	if (!napi->dev->netdev_ops->ndo_ll_poll)
		return false;

	local_bh_disable();

	ops = napi->dev->netdev_ops;
	while (skb_queue_empty(&sk->sk_receive_queue) &&
			!time_after((unsigned long)get_cycles(), end_time)) {
		if (ops->ndo_ll_poll(napi) == LL_FLUSH_FAILED)
				break; /* premanent failure */
		if (nonblock)
			break;
	}

	local_bh_enable();

	return !skb_queue_empty(&sk->sk_receive_queue);
}

static inline void skb_mark_ll(struct sk_buff *skb, struct napi_struct *napi)
{
	skb->dev_ref = napi;
}

static inline void sk_mark_ll(struct sock *sk, struct sk_buff *skb)
{
	sk->dev_ref = skb->dev_ref;
}
#else /* CONFIG_INET_LL_RX_FLUSH */

static inline bool sk_valid_ll(struct sock *sk)
{
	return 0;
}

static inline bool sk_poll_ll(struct sock *sk, int nonblock)
{
	return 0;
}

static inline void skb_mark_ll(struct sk_buff *skb, struct napi_struct *napi)
{
}

static inline void sk_mark_ll(struct sock *sk, struct sk_buff *skb)
{
}
#endif /* CONFIG_INET_LL_RX_FLUSH */
#endif /* _LINUX_NET_LL_POLL_H */
