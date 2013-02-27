/*
 * measure the time between two points in the kernel.
 * for each stage compile with EXTRA_CFLAGS="-DSTAGE=$stage"
 * where stage goes from 0 to 20
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/ktime.h>
#include <linux/limits.h>
#include <linux/sched.h>

#ifndef STAGE
#define STAGE 3
#endif

#if (STAGE == 0)
#define START_SYMBOL "ixgbe_fetch_rx_buffer"
#define END_SYMBOL "__netif_receive_skb"
#elif (STAGE == 1)
#define START_SYMBOL "__netif_receive_skb"
#define END_SYMBOL "tcp_v4_rcv"
#elif (STAGE == 2)
#define START_SYMBOL "tcp_v4_rcv"
#define END_SYMBOL "sys_recvfrom"
#define END_POST
#elif (STAGE == 3)
#define START_SYMBOL "sys_recvfrom"
#define START_POST
#define END_SYMBOL "sys_sendto"
#elif (STAGE == 4)
#define START_SYMBOL "sys_sendto"
#define END_SYMBOL "tcp_transmit_skb"
#elif (STAGE == 5)
#define START_SYMBOL "tcp_transmit_skb"
#define END_SYMBOL "dev_queue_xmit"
#elif (STAGE == 6)
#define START_SYMBOL "dev_queue_xmit"
#define END_SYMBOL "ixgbe_xmit_frame"
#define END_POST
#elif (STAGE == 7)
#define START_SYMBOL "ixgbe_fetch_rx_buffer"
#define END_SYMBOL "ixgbe_xmit_frame"
#define END_POST
#elif (STAGE == 8)
#define START_SYMBOL "__netif_receive_skb"
#define END_SYMBOL "ixgbe_xmit_frame"
#define END_POST
#elif (STAGE == 9)
#define START_SYMBOL "tcp_v4_rcv"
#define END_SYMBOL "ixgbe_xmit_frame"
#define END_POST
#elif (STAGE == 10)
#define START_SYMBOL "sys_recvfrom"
#define START_POST
#define END_SYMBOL "ixgbe_xmit_frame"
#define END_POST
#elif (STAGE == 11)
#define START_SYMBOL "sys_sendto"
#define END_SYMBOL "ixgbe_xmit_frame"
#define END_POST
#elif (STAGE == 12)
#define START_SYMBOL "tcp_transmit_skb"
#define END_SYMBOL "ixgbe_xmit_frame"
#define END_POST
#elif (STAGE == 13)
#define START_SYMBOL "dev_queue_xmit"
#define END_SYMBOL "ixgbe_xmit_frame"
#define END_POST
#elif (STAGE == 14)
#define START_SYMBOL "ixgbe_fetch_rx_buffer"
#define END_SYMBOL "__netif_receive_skb"
#elif (STAGE == 15)
#define START_SYMBOL "ixgbe_fetch_rx_buffer"
#define END_SYMBOL "tcp_v4_rcv"
#elif (STAGE == 16)
#define START_SYMBOL "ixgbe_fetch_rx_buffer"
#define END_SYMBOL "sys_recvfrom"
#define END_POST
#elif (STAGE == 17)
#define START_SYMBOL "ixgbe_fetch_rx_buffer"
#define END_SYMBOL "sys_sendto"
#elif (STAGE == 18)
#define START_SYMBOL "ixgbe_fetch_rx_buffer"
#define END_SYMBOL "tcp_transmit_skb"
#elif (STAGE == 19)
#define START_SYMBOL "ixgbe_fetch_rx_buffer"
#define END_SYMBOL "dev_queue_xmit"
#elif (STAGE == 20)
#define START_SYMBOL "ixgbe_fetch_rx_buffer"
#define END_SYMBOL "ixgbe_xmit_frame"
#define END_POST
#endif

/* per-instance private data */
u64 start_cycles;
u64 total_cycles;
bool probe_valid;
unsigned int sample_count;

static u64 rdtsc(void)
{
	unsigned int l, h;

	asm volatile("rdtsc" : "=a" (l), "=d" (h));

	return l | (((u64)h) << 32);
}

/* Here we use the entry_hanlder to timestamp function entry */
static int start_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	start_cycles = rdtsc();
	probe_valid = true;

	return 0;
}

/*
 * Return-probe handler: Log the return value and duration. Duration may turn
 * out to be zero consistently, depending upon the granularity of time
 * accounting on the platform.
 */
static int end_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	if (probe_valid) {
		total_cycles += rdtsc() - start_cycles;
		sample_count++;
		probe_valid = false;
	}

	if (sample_count > 1000000) {
		trace_printk("%4lldns %22s -> " END_SYMBOL " %s\n",
				(long long)(total_cycles * 10) /
				(sample_count * 27),
#ifdef START_POST
			     START_SYMBOL " (ret)",
#else
			     START_SYMBOL,
#endif
#ifdef END_POST
			     "(ret)"
#else
			     ""
#endif
			);
		total_cycles = 0;
		sample_count = 0;
	}

	return 0;
}

/* For each probe you need to allocate a kprobe structure */
static struct kretprobe start_kretprobe = {
#ifdef START_POST
	.handler		= start_handler,
#else
	.entry_handler		= start_handler,
#endif
	.kp.symbol_name		= START_SYMBOL,
};

static struct kretprobe end_kretprobe = {
#ifdef END_POST
	.handler		= end_handler,
#else
	.entry_handler		= end_handler,
#endif
	.kp.symbol_name		= END_SYMBOL,
};

static int __init kretprobe_init(void)
{
	int ret;

	total_cycles = 0;
	sample_count = 0;
	probe_valid = false;

	ret = register_kretprobe(&start_kretprobe);
	if (ret < 0) {
		printk(KERN_INFO "register_kretprobe failed, returned %d\n",
			ret);
		return ret;
	}

	ret = register_kretprobe(&end_kretprobe);
	if (ret < 0) {
		unregister_kretprobe(&end_kretprobe);
		printk(KERN_INFO "register_kretprobe failed, returned %d\n",
			ret);
		return ret;
	}

	return 0;
}

static void __exit kretprobe_exit(void)
{
	unregister_kretprobe(&start_kretprobe);
	unregister_kretprobe(&end_kretprobe);
}

module_init(kretprobe_init)
module_exit(kretprobe_exit)
MODULE_LICENSE("GPL");
