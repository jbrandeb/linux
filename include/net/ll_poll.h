/*
 * low latency device queue flush
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

static inline int sk_valid_ll(struct sock *sk)
{
	return sysctl_net_ll_poll && sk->dev_ref &&
		!need_resched() && !signal_pending(current);
}

/*
 * TODO: how do we know that we have a working get_cycles?
 * do we limit this by a configure dependacy?
 * TODO: this is not safe when the device can be removed,
 * but simple refcounting may prevent removal indefinatly
 */
static inline int sk_poll_ll(struct sock *sk)
{
	struct napi_struct *napi = sk->dev_ref;
	const struct net_device_ops *ops;
	unsigned long end_time = sysctl_net_ll_poll + get_cycles();

	if (!napi->dev || !napi->dev->netdev_ops ||
	    !napi->dev->netdev_ops->ndo_ll_poll)
		return false;

	local_bh_disable();

	ops = napi->dev->netdev_ops;
	while (skb_queue_empty(&sk->sk_receive_queue) &&
			!time_after((unsigned long)get_cycles(), end_time))
		if (ops->ndo_ll_poll(napi) == LL_FLUSH_FAILED)
				break; /* premanent failure */

	local_bh_enable();

	return !skb_queue_empty(&sk->sk_receive_queue);
}

static inline void skb_mark_ll(struct napi_struct *napi, struct sk_buff *skb)
{
	skb->dev_ref = napi;
}

static inline void sk_mark_ll(struct sock *sk, struct sk_buff *skb)
{
	if (skb->dev_ref)
		sk->dev_ref = skb->dev_ref;

}
#else /* CONFIG_INET_LL_RX_FLUSH */

#define sk_valid_ll(sk) 0
#define sk_poll_ll(sk) do {} while (0)
#define skb_mark_ll(napi, skb) do {} while (0)
#define sk_mark_ll(sk, skb) do {} while (0)

#endif /* CONFIG_INET_LL_RX_FLUSH */
#endif /* _LINUX_NET_LL_POLL_H */
