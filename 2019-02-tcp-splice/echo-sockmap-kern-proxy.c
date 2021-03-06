#include <stdint.h>

#include "bpf.h"
#include "bpf_helpers.h"
#include <arpa/inet.h>

struct bpf_map_def SEC("maps") sock_map = {
	.type = BPF_MAP_TYPE_SOCKMAP,
	.key_size = sizeof(int),
	.value_size = sizeof(int),
	.max_entries = 2,
};

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define DEBUG 1
#ifdef DEBUG
/* Only use this for debug output. Notice output from bpf_trace_printk()
 * end-up in /sys/kernel/debug/tracing/trace_pipe
 */
#define bpf_debug(fmt, ...)                                                    \
	({                                                                     \
		char ____fmt[] = fmt;                                          \
		bpf_trace_printk(____fmt, sizeof(____fmt), ##__VA_ARGS__);     \
	})
#else
#define bpf_debug(fmt, ...)                                                    \
	{                                                                      \
	}                                                                      \
	while (0)
#endif

SEC("prog_parser")
int _prog_parser(struct __sk_buff *skb)
{       
	//  bpf_debug("parser %d", skb->len);
	return skb->len;
}

SEC("prog_verdict")
int _prog_verdict(struct __sk_buff *skb)
{
	// static uint32_t ip_receiver = 599884472; // "35.193.130.184"
	// static uint32_t ip_sender = 602903655; // "	35.239.148.103"
	
	//uint32_t rem_ip = skb->remote_ip4;
	bpf_debug("verdict %lu", skb->remote_ip4);
	// uint32_t rem_ip = ntohl(skb->remote_ip4);
	int idx = 0;
	if(skb->remote_ip4 == 151027722)
		idx = 1;

	return bpf_sk_redirect_map(skb, &sock_map, 1, 0);
}
